#include <gungnir/migration/migration.hpp>

#include "context.hpp"

namespace gungnir {

migration::Plan Migration::plan_up() {
    migration::Plan plan;
    migration::detail::Scope scope{plan};
    up();
    return plan;
}

migration::Plan Migration::plan_down() {
    migration::Plan plan;
    migration::detail::Scope scope{plan};
    down();
    return plan;
}

} // namespace gungnir
