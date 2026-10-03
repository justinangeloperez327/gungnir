#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gungnir::benchmark {

struct Config {
    std::size_t iterations{1000};
    std::size_t samples{9};
    std::size_t warmup_iterations{100};
    bool json{false};
};

inline std::size_t parse_count(
    std::string_view value,
    std::string_view option
) {
    if (value.empty()) {
        throw std::invalid_argument(
            std::string{option} +
            " requires a positive integer"
        );
    }

    std::size_t result = 0;

    for (const auto character : value) {
        if (
            character < '0' ||
            character > '9'
        ) {
            throw std::invalid_argument(
                std::string{option} +
                " requires a positive integer"
            );
        }

        const auto digit =
            static_cast<std::size_t>(
                character - '0'
            );

        result =
            result * 10U +
            digit;
    }

    if (result == 0) {
        throw std::invalid_argument(
            std::string{option} +
            " must be greater than zero"
        );
    }

    return result;
}

inline Config parse_config(
    int argc,
    char** argv
) {
    Config config;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{
            argv[index]
        };

        if (argument == "--json") {
            config.json = true;
            continue;
        }

        if (argument == "--smoke") {
            config.iterations = 1;
            config.samples = 1;
            config.warmup_iterations = 1;
            continue;
        }

        constexpr std::string_view
            iterations_prefix{
                "--iterations="
            };

        constexpr std::string_view
            samples_prefix{
                "--samples="
            };

        constexpr std::string_view
            warmup_prefix{
                "--warmup="
            };

        if (
            argument.starts_with(
                iterations_prefix
            )
        ) {
            config.iterations =
                parse_count(
                    argument.substr(
                        iterations_prefix.size()
                    ),
                    "--iterations"
                );

            continue;
        }

        if (
            argument.starts_with(
                samples_prefix
            )
        ) {
            config.samples =
                parse_count(
                    argument.substr(
                        samples_prefix.size()
                    ),
                    "--samples"
                );

            continue;
        }

        if (
            argument.starts_with(
                warmup_prefix
            )
        ) {
            config.warmup_iterations =
                parse_count(
                    argument.substr(
                        warmup_prefix.size()
                    ),
                    "--warmup"
                );

            continue;
        }

        throw std::invalid_argument(
            "Unknown benchmark option: " +
            std::string{argument}
        );
    }

    return config;
}

struct Result {
    std::string name;
    double median_ns_per_operation{0.0};
    double p95_ns_per_operation{0.0};
    double operations_per_second{0.0};
};

inline std::atomic<std::uint64_t>
    benchmark_sink{0};

class Suite {
public:
    Suite(
        std::string name,
        Config config
    )
        : name_(std::move(name)),
          config_(config) {}

    template <typename Operation>
    void run(
        std::string name,
        Operation&& operation
    ) {
        std::uint64_t checksum = 0;

        for (
            std::size_t iteration = 0;
            iteration <
                config_.warmup_iterations;
            ++iteration
        ) {
            checksum ^=
                static_cast<std::uint64_t>(
                    operation(iteration)
                );
        }

        std::vector<double> values;
        values.reserve(config_.samples);

        for (
            std::size_t sample = 0;
            sample < config_.samples;
            ++sample
        ) {
            const auto started =
                std::chrono::
                    steady_clock::now();

            for (
                std::size_t iteration = 0;
                iteration <
                    config_.iterations;
                ++iteration
            ) {
                checksum ^=
                    static_cast<std::uint64_t>(
                        operation(iteration)
                    ) +
                    static_cast<
                        std::uint64_t
                    >(iteration);
            }

            const auto elapsed =
                std::chrono::
                    duration<double, std::nano>(
                        std::chrono::
                            steady_clock::now() -
                        started
                    ).count();

            values.push_back(
                elapsed /
                static_cast<double>(
                    config_.iterations
                )
            );
        }

        benchmark_sink.fetch_xor(
            checksum,
            std::memory_order_relaxed
        );

        std::sort(
            values.begin(),
            values.end()
        );

        const auto median =
            values[
                values.size() / 2U
            ];

        const auto percentile_index =
            std::min(
                values.size() - 1U,
                (
                    95U * values.size() +
                    99U
                ) /
                100U -
                1U
            );

        const auto p95 =
            values[percentile_index];

        results_.push_back({
            std::move(name),
            median,
            p95,
            median > 0.0
                ? 1000000000.0 / median
                : 0.0
        });
    }

    int finish() const {
        if (config_.json) {
            print_json();
        } else {
            print_human();
        }

        return 0;
    }

private:
    void print_human() const {
        std::cout
            << "suite=" << name_
            << " iterations="
            << config_.iterations
            << " samples="
            << config_.samples
            << '\n';

        for (const auto& result :
             results_) {
            std::cout
                << result.name
                << " median_ns/op="
                << std::fixed
                << std::setprecision(2)
                << result
                    .median_ns_per_operation
                << " p95_ns/op="
                << result
                    .p95_ns_per_operation
                << " ops/s="
                << result
                    .operations_per_second
                << '\n';
        }
    }

    void print_json() const {
        std::cout
            << "{\"schema\":1"
            << ",\"suite\":\""
            << name_
            << "\""
            << ",\"iterations\":"
            << config_.iterations
            << ",\"samples\":"
            << config_.samples
            << ",\"results\":[";

        for (
            std::size_t index = 0;
            index < results_.size();
            ++index
        ) {
            if (index != 0) {
                std::cout << ',';
            }

            const auto& result =
                results_[index];

            std::cout
                << "{\"name\":\""
                << result.name
                << "\",\"median_ns_per_operation\":"
                << std::fixed
                << std::setprecision(3)
                << result
                    .median_ns_per_operation
                << ",\"p95_ns_per_operation\":"
                << result
                    .p95_ns_per_operation
                << ",\"operations_per_second\":"
                << result
                    .operations_per_second
                << '}';
        }

        std::cout
            << "],\"checksum\":"
            << benchmark_sink.load(
                std::memory_order_relaxed
            )
            << "}\n";
    }

    std::string name_;
    Config config_;
    std::vector<Result> results_;
};

} // namespace gungnir::benchmark
