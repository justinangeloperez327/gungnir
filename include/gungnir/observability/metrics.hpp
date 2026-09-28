#pragma once

#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

#include <gungnir/observability/trace.hpp>

namespace gungnir::observability {

enum class MetricKind {
    counter,
    gauge,
    histogram
};

struct MetricPoint {
    MetricKind kind{
        MetricKind::counter
    };
    std::string name;
    double value{0.0};
    Attributes attributes;
    std::string trace_id;
    std::string span_id;
    std::chrono::system_clock::time_point
        timestamp{
            std::chrono::
                system_clock::now()
        };
};

class MetricSink {
public:
    virtual ~MetricSink() = default;

    virtual void export_metric(
        const MetricPoint& point
    ) = 0;

    virtual void flush() {}
    virtual void shutdown() {}
};

namespace detail {

[[nodiscard]]
inline std::mutex&
global_meter_mutex() {
    static std::mutex mutex;
    return mutex;
}

class MeterStorage;

} // namespace detail

class Instrument {
public:
    Instrument() = default;

    Instrument(
        std::shared_ptr<MetricSink> sink,
        std::string name,
        MetricKind kind
    )
        : sink_(
            std::move(sink)
          ),
          name_(
            std::move(name)
          ),
          kind_(kind) {}

    [[nodiscard]]
    bool valid()
        const noexcept {
        return
            sink_ != nullptr &&
            !name_.empty();
    }

protected:
    void emit(
        double value,
        Attributes attributes
    ) const noexcept {
        if (
            !valid() ||
            !std::isfinite(value)
        ) {
            return;
        }

        MetricPoint point;
        point.kind = kind_;
        point.name = name_;
        point.value = value;
        point.attributes =
            std::move(attributes);
        point.timestamp =
            std::chrono::
                system_clock::now();

        const auto context =
            current_context();

        if (context.valid()) {
            point.trace_id =
                context.trace_id;

            point.span_id =
                context.span_id;
        }

        try {
            sink_->export_metric(
                point
            );
        } catch (...) {
        }
    }

private:
    std::shared_ptr<MetricSink> sink_;
    std::string name_;
    MetricKind kind_{
        MetricKind::counter
    };
};

class Counter final :
    public Instrument {
public:
    Counter() = default;

    Counter(
        std::shared_ptr<MetricSink> sink,
        std::string name
    )
        : Instrument(
            std::move(sink),
            std::move(name),
            MetricKind::counter
          ) {}

    void add(
        double value = 1.0,
        Attributes attributes = {}
    ) const noexcept {
        if (value < 0.0) {
            return;
        }

        emit(
            value,
            std::move(attributes)
        );
    }
};

class Gauge final :
    public Instrument {
public:
    Gauge() = default;

    Gauge(
        std::shared_ptr<MetricSink> sink,
        std::string name
    )
        : Instrument(
            std::move(sink),
            std::move(name),
            MetricKind::gauge
          ) {}

    void set(
        double value,
        Attributes attributes = {}
    ) const noexcept {
        emit(
            value,
            std::move(attributes)
        );
    }
};

class Histogram final :
    public Instrument {
public:
    Histogram() = default;

    Histogram(
        std::shared_ptr<MetricSink> sink,
        std::string name
    )
        : Instrument(
            std::move(sink),
            std::move(name),
            MetricKind::histogram
          ) {}

    void record(
        double value,
        Attributes attributes = {}
    ) const noexcept {
        if (value < 0.0) {
            return;
        }

        emit(
            value,
            std::move(attributes)
        );
    }
};

class Meter {
public:
    Meter() = default;

    explicit Meter(
        std::shared_ptr<MetricSink> sink
    )
        : sink_(
            std::move(sink)
          ) {}

    Meter& sink(
        std::shared_ptr<MetricSink> value
    ) {
        std::lock_guard lock{
            mutex_
        };

        sink_ =
            std::move(value);

        return *this;
    }

    [[nodiscard]]
    Counter counter(
        std::string name
    ) const {
        return Counter{
            current_sink(),
            std::move(name)
        };
    }

    [[nodiscard]]
    Gauge gauge(
        std::string name
    ) const {
        return Gauge{
            current_sink(),
            std::move(name)
        };
    }

    [[nodiscard]]
    Histogram histogram(
        std::string name
    ) const {
        return Histogram{
            current_sink(),
            std::move(name)
        };
    }

    void flush()
        const {
        const auto sink =
            current_sink();

        if (sink) {
            sink->flush();
        }
    }

    void shutdown()
        const {
        const auto sink =
            current_sink();

        if (sink) {
            sink->shutdown();
        }
    }

private:
    [[nodiscard]]
    std::shared_ptr<MetricSink>
    current_sink()
        const {
        std::lock_guard lock{
            mutex_
        };

        return sink_;
    }

    mutable std::mutex mutex_;
    std::shared_ptr<MetricSink>
        sink_;
};

namespace detail {

[[nodiscard]]
inline std::shared_ptr<Meter>&
global_meter_storage() {
    static std::shared_ptr<Meter>
        meter;

    return meter;
}

} // namespace detail

[[nodiscard]]
inline std::shared_ptr<Meter>
global_meter() {
    std::lock_guard lock{
        detail::
            global_meter_mutex()
    };

    auto& meter =
        detail::
            global_meter_storage();

    if (!meter) {
        meter =
            std::make_shared<Meter>();
    }

    return meter;
}

inline void set_global_meter(
    std::shared_ptr<Meter> meter
) {
    std::lock_guard lock{
        detail::
            global_meter_mutex()
    };

    detail::
        global_meter_storage() =
        meter
            ? std::move(meter)
            : std::make_shared<Meter>();
}

} // namespace gungnir::observability
