#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include <gungnir/core/types.hpp>
#include <gungnir/database/connection.hpp>
#include <gungnir/migration/migration.hpp>

namespace gungnir::migration {

struct Record {
    String name;
    std::size_t batch{0};
};

struct Named {
    String name;
    Migration* migration{nullptr};
};

class Repository {
public:
    virtual ~Repository() = default;

    virtual void ensure(database::Connection& connection) = 0;
    [[nodiscard]] virtual std::vector<Record> applied(
        database::Connection& connection
    ) = 0;
    [[nodiscard]] virtual std::size_t last_batch(
        database::Connection& connection
    ) = 0;
    virtual void record(
        database::Connection& connection,
        const Record& migration
    ) = 0;
    virtual void remove(
        database::Connection& connection,
        std::string_view name
    ) = 0;
};

class DatabaseRepository final : public Repository {
public:
    void ensure(database::Connection& connection) override;

    [[nodiscard]] std::vector<Record> applied(
        database::Connection& connection
    ) override;

    [[nodiscard]] std::size_t last_batch(
        database::Connection& connection
    ) override;

    void record(
        database::Connection& connection,
        const Record& migration
    ) override;

    void remove(
        database::Connection& connection,
        std::string_view name
    ) override;
};

class Runner {
public:
    explicit Runner(
        Repository& repository,
        String connection = "default"
    );

    [[nodiscard]] std::size_t migrate(
        const std::vector<Named>& migrations
    );

    [[nodiscard]] std::size_t rollback(
        const std::vector<Named>& migrations
    );

    [[nodiscard]] std::size_t reset(
        const std::vector<Named>& migrations
    );

private:
    void execute_plan(
        database::Connection& connection,
        const Plan& plan
    );

    Repository& repository_;
    String connection_;
};

} // namespace gungnir::migration
