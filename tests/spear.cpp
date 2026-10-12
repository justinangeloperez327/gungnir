#include <gungnir/gungnir.hpp>
#include <gungnir/orm/orm.hpp>
#include <cassert>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <limits>
#include <thread>
#include <tuple>

namespace {
void write(const std::filesystem::path& file, std::string_view source) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream out{file, std::ios::binary | std::ios::trunc};
    out << source; assert(out.good());
}
template<class Action> gungnir::view::SyntaxError syntax(Action action) {
    try { action(); } catch (const gungnir::view::SyntaxError& error) { return error; }
    throw std::logic_error("Expected Spear syntax failure");
}
template<class Error, class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const Error&) { rejected = true; }
    assert(rejected);
}
}

class PublicUser : public gungnir::Model<PublicUser> {
public:
    inline static constexpr gungnir::Table table{"users"};
    inline static constexpr auto hidden = std::array<std::string_view, 1>{"password"};
    gungnir::Field<gungnir::String> name, password;
};
template<> struct gungnir::model::Generated<PublicUser> {
    inline static constexpr auto attributes = std::tuple{
        gungnir::model::attribute("name", &PublicUser::name),
        gungnir::model::attribute("password", &PublicUser::password)
    };
    inline static constexpr auto relations = std::tuple{};
};

int main() {
    using namespace gungnir;
    using view::Value;
    const auto root = std::filesystem::temp_directory_path() / "gungnir-spear-test";
    std::filesystem::remove_all(root);
    view::Engine engine{root};
    auto render = [&](std::string_view source, const view::Data& data = view::Data{}) {
        return engine.render_spear_text(source, data);
    };
    view::Data data{{"title", "<A&\"'>"}, {"raw", "<b>trusted</b>"}, {"count", 2}};
    assert(render("{{ title }}|{!! raw !!}", data) == "&lt;A&amp;&quot;&#39;&gt;|<b>trusted</b>");
    assert(render("{{-- {{ bad( }} @if( --}}@@if @{{ title }}@verbatim{{ title }}@if(false)@endverbatim") == "@if {{ title }}{{ title }}@if(false)");
    assert(render("<style>@media screen { x: y; } @font-face { x: y; }</style>user@if.example") == "<style>@media screen { x: y; } @font-face { x: y; }</style>user@if.example");
    assert(render("@if(count > 5)A@elseif(count >= 2 && !missing)B@else C@endif@unless(false)D@endunless", data) == "BD");
    assert(render("{{ missing ?? 'fallback' }}|{{ false ?? true }}|{{ true ? 'yes' : bad() }}") == "fallback|false|yes");
    assert(render("{{ false && bad() }}|{{ true || bad() }}") == "false|true");
    assert(render("{{ '}} !!})' }}|{{ {value: ['x', 'y']}['value'][1] }}") == "}} !!})|y");
    assert(render("{{ -9223372036854775808 }}|{{ 18446744073709551615 }}") == "-9223372036854775808|18446744073709551615");
    assert(render("{{ 18446744073709551615 > 18446744073709551614 }}|{{ 9223372036854775807 < 9223372036854775808 }}|{{ -1 < 0 }}") == "true|true|true");
    assert(render("{{ 18446744073709551615 == 18446744073709551614 }}|{{ 9223372036854775807 == 9223372036854775808 }}|{{ 1 == '1' }}|{{ null == false }}") == "false|false|false|false");
    assert(render("{{ -1 < -0.5 }}|{{ 0 < 0.5 }}|{{ 18446744073709551615 < 18446744073709551616.0 }}") == "true|true|true");
    assert(render("{{ [1, {a: 2}] == [1, {a: 2}] }}|{{ {a: 1} != {a: 2} }}") == "true|true");
    std::atomic<unsigned> calls{0};
    engine.helper("upper", [&](const std::vector<Value>& args) {
        ++calls; assert(args.size() == 1); return Value{args[0].string() + "!"};
    });
    assert(render("{{ upper(title) }}", data) == "&lt;A&amp;&quot;&#39;&gt;!");
    assert(calls == 1);
    assert(render("@if(false){{ upper(title) }}@endif", data).empty());
    assert(calls == 1);
    auto located = syntax([&] { (void)engine.render_spear_text("first\n{{ bad() }}", {}, "pages/error.spear"); });
    assert(located.template_name() == "pages/error.spear" && located.line() == 2 && located.column() > 1);
    assert(std::string{located.what()}.find("Unknown view helper") != std::string::npos);
    for (auto source : {"{{ }}", "{{ value + 1 }}", "{{ user.erase() }}", "{{ 'unclosed }}", "{{ 18446744073709551616 }}",
                        "@if(false){{ broken + }}@endif", "@if(true)x@endforeach", "@foreach(users as user)x@endif",
                        "@forelse(users as loop)x@endforelse", "@else", "@include('partial',)", "{{ {x: 1, x: 2} }}",
                        "@section('x')unclosed", "@extends('x')output", "@if(true)@extends('x')@endif", "@verbatim x"}) {
        (void)syntax([&] { (void)render(source); });
    }
    located = syntax([&] { (void)engine.render_spear_text("line one\n@if(true)\n{{ name + 1 }}\n@endif", {}, "pages/error.spear"); });
    assert(located.template_name() == "pages/error.spear" && located.line() == 3);

    const auto users = Value::array({Value::object({{"name", "Ada"}}), Value::object({{"name", "<Bob>"}})});
    view::Data loops{{"users", users}, {"user", "outside"}, {"items", Value::array({"x", "y"})}};
    assert(render("@foreach($users as $key => $user){{ key }}:{{ $user->name }}:{{ loop.iteration }}:{{ loop.remaining }}:{{ loop.first }}:{{ loop.last }};@endforeach{{ user }}{{ loop ?? 'clean' }}", loops) ==
           "0:Ada:1:1:true:false;1:&lt;Bob&gt;:2:0:false:true;outsideclean");
    assert(render("@forelse([] as user)bad@empty empty @endforelse") == " empty ");
    assert(render("@forelse(users as user){{ user.name }}@empty bad@endforelse", loops) == "Ada&lt;Bob&gt;");
    assert(render("@foreach(users as user)@foreach(items as item){{ loop.depth }}:{{ loop.parent.index }}{{ item }};@endforeach{{ loop.depth }};@endforeach", loops) ==
           "2:0x;2:0y;1;2:1x;2:1y;1;");
    assert(render("@foreach({z: 'Z', a: 'A'} as key => item){{ key }}{{ item }}@endforeach") == "aAzZ");
    (void)syntax([&] { (void)render("@foreach('wrong' as item)x@endforeach"); });
    PublicUser public_user = PublicUser::hydrate({{"name", String{"Visible"}}, {"password", String{"secret"}}});
    assert(render("{{ user.name }}|{{ user.password }}", {{"user", public_user}}) == "Visible|");

    write(root / "layouts/base.spear", "<title>@yield('title', '<Default>')</title><main>@yield('content')</main>@yield('footer', 'Default footer')");
    write(root / "layouts/middle.spear", "@extends('layouts/base')@section('footer')Middle@endsection");
    write(root / "partials/user.spear", "<li>{{ user.name }}:{{ label }}</li>");
    write(root / "page.spear", "@extends('layouts/middle')@section('title', title)@section('content')@foreach(users as user)@include('partials/user', {label: 'item'})@endforeach@endsection");
    assert(engine.render("page", loops.merge(data)) == "<title>&lt;A&amp;&quot;&#39;&gt;</title><main><li>Ada:item</li><li>&lt;Bob&gt;:item</li></main>Middle");
    assert(engine.render("layouts/base") == "<title>&lt;Default&gt;</title><main></main>Default footer");
    write(root / "compat.html", "{{ title }}{{#if count}}!{{/if}}");
    write(root / "compat.spear", "Spear {{ title }}");
    assert(engine.render("compat", data) == "Spear &lt;A&amp;&quot;&#39;&gt;");
    assert(engine.render("compat.html", data) == "&lt;A&amp;&quot;&#39;&gt;!");
    assert(engine.render_spear_text("@include('compat.html', {title: 'Legacy', count: 1})") == "Legacy!");
    write(root / "bridge.html", "{{#each users}}{{> 'partials/user' user=this label='old' }}{{/each}}");
    assert(engine.render("bridge", loops) == "<li>Ada:old</li><li>&lt;Bob&gt;:old</li>");
    assert(engine.render_text("{{#if enabled}}yes{{else}}no{{/if}}", {{"enabled", true}}) == "yes");
    write(root / "loop.spear", "@include('loop')");
    (void)syntax([&] { (void)engine.render("loop"); });
    write(root / "cycle.html", "{{> 'cycle.spear' }}");
    write(root / "cycle.spear", "@include('cycle.html')");
    (void)syntax([&] { (void)engine.render("cycle"); });
    write(root / "layout-cycle.spear", "@extends('layout-cycle')");
    (void)syntax([&] { (void)engine.render("layout-cycle"); });
    located = syntax([&] { (void)engine.render_spear_text("\n@include('../secret')", {}, "safe.spear"); });
    assert(located.template_name() == "safe.spear" && located.line() == 2);
    (void)syntax([&] { (void)render("@include('page', 'not an object')"); });
    rejects<view::InvalidPath>([&] { (void)engine.render("../secret"); });
    rejects<view::NotFound>([&] { (void)engine.render("missing"); });

    write(root / "reload.spear", "old {{ name }}");
    const auto original_time = std::filesystem::last_write_time(root / "reload.spear");
    assert(engine.render("reload", {{"name", "one"}}) == "old one");
    write(root / "reload.spear", "new {{ name }}");
    std::filesystem::last_write_time(root / "reload.spear", original_time);
    assert(engine.render("reload", {{"name", "two"}}) == "new two");
    write(root / "partials/user.spear", "{{ user.name }}*");
    assert(engine.render("page", loops).find("Ada*&lt;Bob&gt;*") != String::npos);
    engine.cache_mode(view::CacheMode::immutable);
    assert(engine.cached_templates() == 0);
    assert(engine.render("reload", {{"name", "three"}}) == "new three");
    write(root / "reload.spear", "now {{ name }}");
    assert(engine.render("reload", {{"name", "four"}}) == "new four");
    engine.clear_cache();
    assert(engine.render("reload", {{"name", "five"}}) == "now five");
    std::vector<std::thread> renderers;
    std::atomic<bool> failed{false};
    for (unsigned i = 0; i < 8; ++i) renderers.emplace_back([&, i] {
        for (unsigned j = 0; j < 100; ++j) {
            auto name = std::to_string(i) + ":" + std::to_string(j);
            if (engine.render("reload", {{"name", name}}) != "now " + name) failed = true;
        }
    });
    for (auto& thread : renderers) thread.join();
    assert(!failed && engine.cached_templates() == 1);
    const auto second_root = root / "second";
    write(second_root / "reload.spear", "second");
    engine.root(second_root);
    assert(engine.cached_templates() == 0 && engine.render("reload") == "second");
    engine.root(root).cache_mode(view::CacheMode::reload);
    write(root / "reload.spear", "{{ broken + }}");
    (void)syntax([&] { (void)engine.render("reload"); });
    write(root / "reload.spear", "fixed");
    assert(engine.render("reload") == "fixed");
    engine.cache_mode(view::CacheMode::immutable);
    for (unsigned i = 0; i < 270; ++i) {
        auto name = "bounded/" + std::to_string(i);
        write(root / (name + ".spear"), "x");
        assert(engine.render(name) == "x");
    }
    assert(engine.cached_templates() <= 256);
    write(root / "symlink.spear", "safe");
    assert(engine.render("symlink") == "safe");
    write(root / "outside.txt", "secret");
    std::filesystem::remove(root / "symlink.spear");
    std::error_code error;
    std::filesystem::create_symlink(root / "outside.txt", root / "symlink.spear", error);
    if (!error) rejects<view::InvalidPath>([&] { (void)engine.render("symlink"); });
    engine.clear_cache();
    const String large_template(2 * 1024 * 1024, 'x');
    for (unsigned i = 0; i < 6; ++i) {
        const auto name = "heavy/" + std::to_string(i);
        write(root / (name + ".spear"), large_template);
        assert(engine.render(name) == large_template);
    }
    assert(engine.cached_templates() <= 3);
    (void)syntax([&] { (void)render(String(2 * 1024 * 1024 + 1, 'x')); });
    (void)syntax([&] { (void)render("{!! huge !!}", {{"huge", String(8 * 1024 * 1024 + 1, 'x')}}); });
    String many;
    for (unsigned i = 0; i < 60000; ++i) many += "{{ true }}";
    (void)syntax([&] { (void)render(many); });
    String chain = "{{ true";
    for (unsigned i = 0; i < 10000; ++i) chain += " && true";
    chain += " }}";
    (void)syntax([&] { (void)render(chain); });
    (void)syntax([&] { (void)render("@section('a', 'x')@section('a', 'y')"); });
    std::filesystem::remove_all(root);
}
