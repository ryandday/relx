cmake_minimum_required(VERSION 3.14)

# Add the postgresql example
add_executable(postgresql_example postgresql_example.cpp)

# Link against the necessary libraries
target_link_libraries(postgresql_example PRIVATE relx)

# Set C++23 standard
target_compile_features(postgresql_example PRIVATE cxx_std_23) 