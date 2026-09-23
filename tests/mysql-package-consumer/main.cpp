#include <gungnir/database/mysql.hpp>

int main() {
    gungnir::database::DriverRegistry registry;

    gungnir::database::register_mysql(
        registry
    );

    return registry.has(
        gungnir::database::Backend::mysql
    )
        ? 0
        : 1;
}
