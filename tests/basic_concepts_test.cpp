#include <type_traits>
#include <string>
#include <concepts>
#include <iostream>

// Define basic concepts for testing

// A basic concept for numeric types
template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

// A concept for string-like types
template <typename T>
concept StringLike = requires(T t) {
    { t.c_str() } -> std::convertible_to<const char*>;
    { t.length() } -> std::convertible_to<std::size_t>;
    { t.empty() } -> std::convertible_to<bool>;
};

// A concept for container types
template <typename T>
concept Container = requires(T t) {
    typename T::value_type;
    typename T::iterator;
    { t.begin() } -> std::same_as<typename T::iterator>;
    { t.end() } -> std::same_as<typename T::iterator>;
    { t.size() } -> std::convertible_to<std::size_t>;
};

// Some test types
struct CustomString {
    const char* c_str() const { return "test"; }
    std::size_t length() const { return 4; }
    bool empty() const { return false; }
};

struct NotAString {
    int value;
};

// Static concept tests
// static_assert(Numeric<int>, "int should satisfy Numeric");
// static_assert(Numeric<double>, "double should satisfy Numeric");
// static_assert(!Numeric<std::string>, "std::string should not satisfy Numeric");

// static_assert(StringLike<std::string>, "std::string should satisfy StringLike");
// static_assert(StringLike<CustomString>, "CustomString should satisfy StringLike");
// static_assert(!StringLike<NotAString>, "NotAString should not satisfy StringLike");

// static_assert(Container<std::string>, "std::string should satisfy Container");
// static_assert(!Container<int>, "int should not satisfy Container");

// Main function
int main() {
    // Runtime checks instead of static_assert
    // Numeric concept
    if (!Numeric<int>) {
        std::cerr << "int should satisfy Numeric\n";
        return 1;
    }
    if (!Numeric<double>) {
        std::cerr << "double should satisfy Numeric\n";
        return 1;
    }
    if (Numeric<std::string>) {
        std::cerr << "std::string should not satisfy Numeric\n";
        return 1;
    }

    // StringLike concept
    if (!StringLike<std::string>) {
        std::cerr << "std::string should satisfy StringLike\n";
        return 1;
    }
    if (!StringLike<CustomString>) {
        std::cerr << "CustomString should satisfy StringLike\n";
        return 1;
    }
    if (StringLike<NotAString>) {
        std::cerr << "NotAString should not satisfy StringLike\n";
        return 1;
    }

    // Container concept
    if (!Container<std::string>) {
        std::cerr << "std::string should satisfy Container\n";
        return 1;
    }
    if (Container<int>) {
        std::cerr << "int should not satisfy Container\n";
        return 1;
    }

    std::cout << "All concept tests passed!\n";
    return 0;
} 