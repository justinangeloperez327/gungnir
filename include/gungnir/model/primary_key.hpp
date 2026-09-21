#pragma once

#include <gungnir/model/field.hpp>

namespace gungnir {

template <typename T>
class PrimaryKey : public Field<T> {
public:
    using Field<T>::Field;
    using Field<T>::operator=;
};

} // namespace gungnir
