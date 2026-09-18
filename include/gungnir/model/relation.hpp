#pragma once

namespace gungnir {

template <typename Related>
class HasOne {
public:
    using related_type = Related;
};

template <typename Related>
class HasMany {
public:
    using related_type = Related;
};

template <typename Related>
class BelongsTo {
public:
    using related_type = Related;
};

template <typename Related>
class BelongsToMany {
public:
    using related_type = Related;
};

} // namespace gungnir
