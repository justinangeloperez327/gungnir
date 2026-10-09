#pragma once

#include <atomic>
#include <cstddef>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <string>

#include <gungnir/http/json.hpp>
#include <gungnir/logging/logger.hpp>

namespace gungnir::logging {

// Newline-delimited JSON for a platform log agent. The stream must outlive the
// sink. Writes are synchronous, serialized, and flushed after each record;
// rotation, retention and durable shipping belong to the stream/log agent.
class JsonStreamSink final : public Sink {
public:
    explicit JsonStreamSink(std::ostream& stream, std::size_t max_record_bytes = 64 * 1024)
        : stream_(stream), maximum_(max_record_bytes) {
        if (!maximum_) throw std::invalid_argument("Log record limit must be positive");
    }
    void write(const Record& record) override {
        // Reject oversized input before constructing a second encoded copy.
        std::size_t estimate = record.message.size();
        if (estimate > maximum_) { ++dropped_; return; }
        for (const auto& [key, value] : record.context) {
            if (key.size() > maximum_ - estimate) { ++dropped_; return; }
            estimate += key.size();
            if (value.size() > maximum_ - estimate) { ++dropped_; return; }
            estimate += value.size();
        }
        http::Json::Object fields;
        for (const auto& [key, value] : record.context) fields.emplace(key, value);
        const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(record.timestamp.time_since_epoch()).count();
        const auto line = http::Json::object({
            {"timestamp_unix_nano", std::to_string(nanos)},
            {"level", level_name(record.level)}, {"message", record.message}, {"context", http::Json::object(std::move(fields))}
        }).dump();
        if (line.size() + 1 > maximum_) { ++dropped_; return; }
        std::lock_guard lock{mutex_};
        try {
            stream_ << line << '\n';
            stream_.flush();
            if (!stream_) throw std::runtime_error("Unable to write structured log record");
        } catch (...) { ++dropped_; throw; }
    }
    [[nodiscard]] std::size_t dropped() const noexcept { return dropped_.load(); }
private:
    static std::string level_name(Level level) {
        switch (level) {
            case Level::trace: return "trace";
            case Level::debug: return "debug";
            case Level::info: return "info";
            case Level::warning: return "warning";
            case Level::error: return "error";
            case Level::critical: return "critical";
        }
        return "unknown";
    }
    std::ostream& stream_;
    std::size_t maximum_;
    std::mutex mutex_;
    std::atomic<std::size_t> dropped_{0};
};

} // namespace gungnir::logging
