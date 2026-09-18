#pragma once

namespace gungnir {

// ForeignKey intentionally carries only relation identity for now.
// Its value type will be resolved from model metadata once the
// PrimaryKey discovery mechanism is finalized.
template <typename Related>
class ForeignKey {
public:
    using related_type = Related;
};

} // namespace gungnir
