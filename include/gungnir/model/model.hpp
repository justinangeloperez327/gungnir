#pragma once

#include <gungnir/core/types.hpp>
#include <gungnir/model/field.hpp>
#include <gungnir/model/foreign_key.hpp>
#include <gungnir/model/primary_key.hpp>
#include <gungnir/model/relation.hpp>
#include <gungnir/model/table.hpp>

namespace gungnir {

template <typename Derived>
class Model {
public:
    using model_type = Derived;

protected:
    Model() = default;
    ~Model() = default;
};

} // namespace gungnir
