#pragma once

#include <gungnir/core/types.hpp>
#include <gungnir/model/field.hpp>

namespace gungnir {

template <typename Related, typename T = Integer>
class ForeignKey : public Field<T> {
public:
    using related_type = Related;
    using value_type = T;
    using Field<T>::Field;
    using Field<T>::operator=;
};

} // namespace gungnir
