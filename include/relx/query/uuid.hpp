#pragma once

/// @brief boost::uuids::uuid support. The Value/val bind overloads live in
/// value.hpp (they must be visible wherever the query machinery is defined);
/// this header adds the column traits for DDL and result parsing.

#include "../schema/uuid_traits.hpp"
#include "value.hpp"
