#include <gungnir/view/engine.hpp>
#include <gungnir/view/error.hpp>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* bytes, std::size_t size) {
    if (size > 65536) return 0;
    gungnir::view::Engine engine{"__gungnir_spear_fuzz_views__"};
    using gungnir::view::Value;
    const gungnir::view::Data data{
        {"items", Value::array({"<one>", true, gungnir::Int64{42}, nullptr})},
        {"user", Value::object({{"name", "<Ada>"}, {"enabled", true}})},
        {"raw", "<em>trusted</em>"}
    };
    engine.helper("echo", [](const std::vector<Value>& values) { return values.empty() ? Value{} : values[0]; });
    try {
        (void)engine.render_spear_text({reinterpret_cast<const char*>(bytes), size}, data, "fuzz.spear");
    } catch (const gungnir::view::SyntaxError& error) {
        if (error.template_name().empty() || error.line() == 0 || error.column() == 0) std::abort();
    } catch (const gungnir::view::Error&) {
        // Missing fixture files and rejected filesystem paths are expected.
    }
    return 0;
}
