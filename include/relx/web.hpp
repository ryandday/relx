#pragma once

/// @brief relx::web - async HTTP layer for relx-backed CRUD services.
///
/// Boost.Beast transport with C++20 coroutine handlers, glaze JSON on plain
/// aggregates, and an awaitable facade over the relx connection pool. Policy
/// (permissions, validation, filters) is plain code in handlers; the library
/// provides the mechanics.
///
/// Design rule: explicitness beats shorter syntax where it matters, especially
/// error handling. Fallible calls stay visibly fallible — value-producing
/// calls return ApiResult and unwrap(...) marks the extraction at the call
/// site (Db conveniences never throw invisibly); pure guards with no value
/// (require_*, validate_*) throw directly, their names being the visibility.
/// When an error is intentionally replaced (e.g. masking an invite lookup as
/// "event not found" to avoid leaking existence), use the explicit
/// unwrap(result, override) overload so the masking is readable.
///
/// Requires the RELX_ENABLE_WEB CMake option (adds the glaze dependency).

#include "web/auth.hpp"
#include "web/authed.hpp"
#include "web/db.hpp"
#include "web/error.hpp"
#include "web/http.hpp"
#include "web/json.hpp"
#include "web/ownership.hpp"
#include "web/pagination.hpp"
#include "web/projection.hpp"
#include "web/router.hpp"
#include "web/server.hpp"

// relx/web/jwt_hs256.hpp is opt-in: include it directly and link OpenSSL::Crypto
