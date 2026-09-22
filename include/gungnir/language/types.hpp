#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace gungnir::language {

using Integer = std::int64_t;
using Decimal = double;
using Boolean = bool;
using String = std::string;

template <typename T>
using Optional = std::optional<T>;

template <typename T>
using List = std::vector<T>;

template <typename T>
using Map = std::unordered_map<std::string, T>;

struct Null final {};
inline constexpr Null null{};

} // namespace gungnir::language
