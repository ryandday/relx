cmake_minimum_required(VERSION 3.25)
project(relx_crud_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# relx is consumed from a local checkout by default (the parent directory).
# When this project is broken out into its own repo, point RELX_SOURCE_DIR at a
# checkout or swap this block for a FetchContent_Declare(relx GIT_REPOSITORY ...).
set(RELX_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "Path to a relx checkout")
set(RELX_ENABLE_POSTGRES_CLIENT ON CACHE BOOL "" FORCE)
add_subdirectory("${RELX_SOURCE_DIR}" relx EXCLUDE_FROM_ALL)

FetchContent_Declare(
  glaze
  GIT_REPOSITORY https://github.com/stephenberry/glaze.git
  GIT_TAG v8.0.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glaze)

find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)

add_executable(crud_server
  src/main.cpp
)

target_link_libraries(crud_server PRIVATE
  relx::relx
  relx::postgresql
  glaze::glaze
  Boost::system
  Threads::Threads
)
