#include <gungnir/cli/project.hpp>
#include <gungnir/language/language.hpp>
#include <cassert>
#include <chrono>
#include <fstream>
#include <string>
namespace {
std::string read(const std::filesystem::path& path) { std::ifstream in(path); return {std::istreambuf_iterator<char>{in},{}}; }
void write(const std::filesystem::path& path,const std::string& source) { std::ofstream{path} << source; }
}
int main() {
    using namespace gungnir::language;
    const std::string source="function int sum(){let total=0;for(let i=0;i<3;i++){total+=i;}// retain this comment\nreturn total;}\nfunction Json object(){return {text:'literal { ; // \\n',count:1};}\n";
    const auto formatted=Formatter{}.format(source);
    assert(formatted==Formatter{}.format(formatted));
    assert(formatted.find("for (let i = 0; i < 3; i ++)")!=std::string::npos);
    assert(Compiler{}.compile(formatted).success());
    assert(Compiler{}.compile(source,"same.gnr",{.emit_line_directives=false}).code==Compiler{}.compile(formatted,"same.gnr",{.emit_line_directives=false}).code);
    for(const auto broken:{"function int f() {", "/* unclosed", "#define VALUE 1\n"}) { bool rejected=false; try { (void)Formatter{}.format(broken); } catch(const std::invalid_argument&) { rejected=true; } assert(rejected); }
    assert(Formatter{}.format("function List<int> f(){return [1,2].map((n)=>n+1);}").find("=>")!=std::string::npos);
    const auto root=std::filesystem::temp_directory_path()/("gungnir-tooling-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup { std::filesystem::path path; ~Cleanup(){std::error_code e;std::filesystem::remove_all(path,e);} } cleanup{root};
    auto project=gungnir::cli::Project::create(root,"sample");
    assert(project.structured() && project.name()=="sample");
    (void)project.make("model","User");
    (void)project.make("event","Created");
    (void)project.make("listener","Record","app.events.created::Created");
    (void)project.make("policy","User","app.models.user::User");
    (void)project.make("notification","Welcome","app.models.user::User");
    for(const auto kind:{"job","mail","request","middleware","migration","controller"}) (void)project.make(kind,"Sample");
    const auto main=project.assemble();
    const auto header=root/".gungnir/generated/program.hpp";
    const auto first=std::filesystem::last_write_time(header);
    const auto main_time=std::filesystem::last_write_time(main);
    (void)project.assemble();
    assert(first==std::filesystem::last_write_time(header) && main_time==std::filesystem::last_write_time(main));
    const auto home=root/"app/controllers/home_controller.gnr";
    const auto unit=root/".gungnir/generated/app.controllers.home_controller.cpp";
    const auto old=read(unit); const auto interface=read(header);
    write(home,"controller HomeController { Response index() { return text('changed'); } }\n");
    (void)project.assemble();
    assert(old!=read(unit) && interface==read(header) && first==std::filesystem::last_write_time(header));
    write(home,"import absent; controller HomeController { index() { return text('bad'); } }\n");
    bool rejected=false; try {(void)project.assemble();} catch(const std::runtime_error&) {rejected=true;} assert(rejected);
    assert(interface==read(header));
    DependencyGraph graph; graph.add("b",{"a"}); graph.add("a",{}); assert((graph.order()==std::vector<std::string>{"a","b"}));
    graph.add("a",{"b"}); rejected=false;try{(void)graph.order();}catch(const std::runtime_error&){rejected=true;}assert(rejected);
    ModuleResolver resolver{root}; rejected=false;try{(void)resolver.resolve("../outside");}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    LanguageServer lsp;
    const std::string math="function int add(int left, int right) { return left + right; }";
    const std::string main_source="import math; function int answer() { return add(2, 3); }";
    lsp.update({{"math.gnr","math",math},{"main.gnr","main",main_source}});
    assert(lsp.diagnostics().empty());
    const auto info=lsp.symbol_at("main.gnr",main_source.find("add"));
    assert(info && info->definition.file=="math.gnr" && info->definition.begin==math.find("add"));
    assert(lsp.symbols("math.gnr").size()==1);
    assert(lsp.symbol_at("math.gnr",math.find("left +"))->definition.begin==math.find("left,"));
    const std::string scope="function int scoped() { const local = 1; if (true) { const hidden = 2; } return local; }";
    lsp.update({{"scope.gnr","scope",scope}});
    const auto completions=lsp.completions("scope.gnr",scope.find("return"));
    assert(std::any_of(completions.begin(),completions.end(),[](const auto& item){return item.label=="local";}));
    assert(std::none_of(completions.begin(),completions.end(),[](const auto& item){return item.label=="hidden";}));
    lsp.update({{"math.gnr","math","function int add() { return unknown; }"}});
    assert(!lsp.diagnostics().empty() && lsp.symbols("math.gnr").size()==1);
}
