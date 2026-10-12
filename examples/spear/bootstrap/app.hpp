#pragma once
#include <gungnir/core/services.hpp>
#include <algorithm>
#include <cctype>

namespace bootstrap {
inline void configure(gungnir::Application& app) {
    app.provider<gungnir::ServicesProvider>();
    app.views().helper("upper", [](const std::vector<gungnir::view::Value>& values) {
        auto text = values.empty() ? gungnir::String{} : values.front().string();
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return gungnir::view::Value{std::move(text)};
    });
    // App-specific services and helpers go here.
}
inline void boot(gungnir::Application&) {}
}
