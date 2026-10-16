#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <gungnir/logging/log.hpp>

namespace gungnir::logging {

class Sink {
public:
    virtual ~Sink() = default;
    virtual void write(const Record& record) = 0;
};

class Logger {
public:
    Logger& sink(std::shared_ptr<Sink> value) {
        if (value) sinks_.push_back(std::move(value));
        return *this;
    }

    void log(Level level, std::string message, Context context = {}) const {
        Record record{.level=level, .message=std::move(message), .context=std::move(context)};
        // A sink may register another sink while handling this record.
        // Keep both iteration and sink lifetime independent of that mutation.
        const auto sinks = sinks_;
        for (const auto& sink : sinks) {
            try { sink->write(record); } catch (...) {}
        }
    }

    void info(std::string message, Context context = {}) const { log(Level::info, std::move(message), std::move(context)); }
    void warning(std::string message, Context context = {}) const { log(Level::warning, std::move(message), std::move(context)); }
    void error(std::string message, Context context = {}) const { log(Level::error, std::move(message), std::move(context)); }

private:
    std::vector<std::shared_ptr<Sink>> sinks_;
};

} // namespace gungnir::logging
