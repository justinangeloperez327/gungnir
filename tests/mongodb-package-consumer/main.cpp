#include <gungnir/database/mongodb.hpp>

int main() {
    gungnir::database::DriverRegistry registry;

    gungnir::database::register_mongodb(
        registry
    );

    return registry.has(
        gungnir::database::Backend::mongodb
    )
        ? 0
        : 1;
}
