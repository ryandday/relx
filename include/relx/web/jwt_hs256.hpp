#pragma once

/// @brief Minimal HS256 JWT signing/verification for relx::web services.
///
/// Deliberately small: symmetric HMAC-SHA256 only, suitable for services that
/// issue their own tokens. For RS256/JWKS providers (Firebase, Auth0), plug a
/// dedicated verifier into relx::web::authenticate instead.
///
/// Not included by relx/web.hpp: requires linking OpenSSL::Crypto.

#include "error.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <openssl/crypto.h>
#include <openssl/hmac.h>

namespace relx::web::jwt {

namespace detail {

inline constexpr std::string_view b64url_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

inline std::string b64url_encode(std::string_view input) {
  std::string out;
  out.reserve((input.size() + 2) / 3 * 4);
  for (size_t i = 0; i < input.size(); i += 3) {
    uint32_t chunk = static_cast<unsigned char>(input[i]) << 16;
    if (i + 1 < input.size()) {
      chunk |= static_cast<unsigned char>(input[i + 1]) << 8;
    }
    if (i + 2 < input.size()) {
      chunk |= static_cast<unsigned char>(input[i + 2]);
    }
    out += b64url_chars[(chunk >> 18) & 0x3F];
    out += b64url_chars[(chunk >> 12) & 0x3F];
    if (i + 1 < input.size()) {
      out += b64url_chars[(chunk >> 6) & 0x3F];
    }
    if (i + 2 < input.size()) {
      out += b64url_chars[chunk & 0x3F];
    }
  }
  return out;
}

inline std::optional<std::string> b64url_decode(std::string_view input) {
  auto value_of = [](char c) -> int {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-') return 62;
    if (c == '_') return 63;
    return -1;
  };
  std::string out;
  out.reserve(input.size() * 3 / 4);
  uint32_t buffer = 0;
  int bits = 0;
  for (char c : input) {
    int v = value_of(c);
    if (v < 0) {
      return std::nullopt;
    }
    buffer = (buffer << 6) | static_cast<uint32_t>(v);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out += static_cast<char>((buffer >> bits) & 0xFF);
    }
  }
  return out;
}

inline std::string hmac_sha256(std::string_view key, std::string_view data) {
  unsigned char digest[32];
  unsigned int digest_len = 0;
  HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
       reinterpret_cast<const unsigned char*>(data.data()), data.size(), digest, &digest_len);
  return std::string(reinterpret_cast<char*>(digest), digest_len);
}

/// @brief Numeric value of a top-level JSON claim like "exp":1700000000, if present.
/// A minimal scan so this header stays JSON-library-free.
inline std::optional<long long> numeric_claim(std::string_view payload, std::string_view name) {
  std::string needle = "\"" + std::string(name) + "\"";
  auto pos = payload.find(needle);
  if (pos == std::string_view::npos) {
    return std::nullopt;
  }
  pos = payload.find(':', pos + needle.size());
  if (pos == std::string_view::npos) {
    return std::nullopt;
  }
  ++pos;
  while (pos < payload.size() && (payload[pos] == ' ' || payload[pos] == '\t')) {
    ++pos;
  }
  char* end = nullptr;
  long long value = std::strtoll(payload.data() + pos, &end, 10);
  if (end == payload.data() + pos) {
    return std::nullopt;
  }
  return value;
}

}  // namespace detail

/// @brief Sign a JSON payload as an HS256 JWT
inline std::string sign(std::string_view payload_json, std::string_view secret) {
  const std::string header = detail::b64url_encode(R"({"alg":"HS256","typ":"JWT"})");
  const std::string payload = detail::b64url_encode(payload_json);
  const std::string signing_input = header + "." + payload;
  return signing_input + "." + detail::b64url_encode(detail::hmac_sha256(secret, signing_input));
}

/// @brief Verify an HS256 JWT and return its payload JSON.
/// Checks structure, signature (constant-time), and the exp claim when present.
inline ApiResult<std::string> verify(std::string_view token, std::string_view secret) {
  const auto first_dot = token.find('.');
  const auto second_dot = token.find('.', first_dot + 1);
  if (first_dot == std::string_view::npos || second_dot == std::string_view::npos ||
      token.find('.', second_dot + 1) != std::string_view::npos) {
    return std::unexpected(unauthorized("malformed token"));
  }

  const std::string_view signing_input = token.substr(0, second_dot);
  const std::string_view header_b64 = token.substr(0, first_dot);
  const std::string_view payload_b64 = token.substr(first_dot + 1, second_dot - first_dot - 1);
  const std::string_view signature_b64 = token.substr(second_dot + 1);

  auto header = detail::b64url_decode(header_b64);
  if (!header || header->find("\"HS256\"") == std::string::npos) {
    return std::unexpected(unauthorized("unsupported token algorithm"));
  }

  auto signature = detail::b64url_decode(signature_b64);
  const std::string expected = detail::hmac_sha256(secret, signing_input);
  if (!signature || signature->size() != expected.size() ||
      CRYPTO_memcmp(signature->data(), expected.data(), expected.size()) != 0) {
    return std::unexpected(unauthorized("invalid token signature"));
  }

  auto payload = detail::b64url_decode(payload_b64);
  if (!payload) {
    return std::unexpected(unauthorized("malformed token payload"));
  }

  if (auto exp = detail::numeric_claim(*payload, "exp")) {
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    if (*exp < now) {
      return std::unexpected(unauthorized("token expired"));
    }
  }

  return *payload;
}

}  // namespace relx::web::jwt
