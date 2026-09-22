#pragma once
#include <chrono>
#include <string>
#include <unordered_map>
#include <utility>

namespace gungnir::logging {

enum class Level { trace, debug, info, warning, error, critical };

using Context = std::unordered_map<std::string, std::string>;

struct Record {
    Level level{Level::info};
    std::string message;
    Context context;
    std::chrono::system_clock::time_point timestamp{std::chrono::system_clock::now()};
};

} // namespace gungnir::logging
