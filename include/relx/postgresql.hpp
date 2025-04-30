#pragma once

#include "connection/postgresql_connection.hpp"
#include "connection/postgresql_async_connection.hpp"

namespace relx {
// Convenient import from the connection namespace
using connection::PostgreSQLConnection;
using connection::PostgreSQLAsyncConnection;
} 