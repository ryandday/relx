cmake_minimum_required(VERSION 3.25)
project(relx_crud_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# relx is consumed from a local checkout by default (the parent directory).
# When this project is broken out into its own repo, point RELX_SOURCE_DIR at a
# checkout or swap this block for a FetchContent_Declare(relx GIT_REPOSITORY ...).
set(RELX_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "Path to a relx checkout")
set(RELX_ENABLE_POSTGRES_CLIENT ON CACHE BOOL "" FORCE)
set(RELX_ENABLE_WEB ON CACHE BOOL "" FORCE)
add_subdirectory("${RELX_SOURCE_DIR}" relx EXCLUDE_FROM_ALL)

find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(crud_server
  src/main.cpp
)

target_link_libraries(crud_server PRIVATE
  relx::web
  Boost::system
  Threads::Threads
  OpenSSL::Crypto
)
