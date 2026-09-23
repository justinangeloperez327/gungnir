#include <gungnir/database/sqlserver.hpp>

int main() {
    gungnir::database::DriverRegistry registry;

    gungnir::database::register_sqlserver(
        registry
    );

    return registry.has(
        gungnir::database::Backend::mssql
    )
        ? 0
        : 1;
}
