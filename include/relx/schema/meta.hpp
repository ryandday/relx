#pragma once

#include <type_traits>

namespace relx::schema {

/// @brief Helper to extract the class type from a member pointer
/// @tparam T The member pointer type
template <typename T>
struct member_pointer_class;

/// @brief Specialization for extracting class type from member pointer
/// @tparam C The class type
/// @tparam T The member type
template <typename C, typename T>
struct member_pointer_class<T C::*> {
  using type = C;
};

/// @brief Convenience alias for member_pointer_class
/// @tparam T The member pointer type
template <typename T>
using member_pointer_class_t = typename member_pointer_class<T>::type;

/// @brief Helper to extract the member type from a member pointer
/// @tparam T The member pointer type
template <typename T>
struct member_pointer_type;

/// @brief Specialization for extracting member type from member pointer
/// @tparam C The class type
/// @tparam T The member type
template <typename C, typename T>
struct member_pointer_type<T C::*> {
  using type = T;
};

/// @brief Convenience alias for member_pointer_type
/// @tparam T The member pointer type
template <typename T>
using member_pointer_type_t = typename member_pointer_type<T>::type;

}  // namespace relx::schema 