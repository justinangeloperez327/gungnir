#include <gungnir/database/postgresql.hpp>

int main() {
    gungnir::database::DriverRegistry registry;

    gungnir::database::register_postgresql(
        registry
    );

    return registry.has(
        gungnir::database::Backend::postgresql
    )
        ? 0
        : 1;
}
