#pragma once

#include <cstddef>
#include <functional>
#include <utility>

#include <gungnir/core/types.hpp>
#include <gungnir/database/backend.hpp>

namespace gungnir::orm {

struct QueryEvent {
    String connection;
    database::Backend backend;
    String statement;
    std::size_t binding_count{0};
};

using QueryListener = std::function<void(const QueryEvent&)>;

inline QueryListener& query_listener() {
    static QueryListener listener;
    return listener;
}

inline void listen(QueryListener listener) {
    query_listener() = std::move(listener);
}

inline void stop_listening() {
    query_listener() = {};
}

inline void report(const QueryEvent& event) {
    if (auto& listener = query_listener()) {
        listener(event);
    }
}

} // namespace gungnir::orm
