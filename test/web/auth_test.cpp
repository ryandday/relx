#include <chrono>
#include <string>

#include <gtest/gtest.h>
#include <relx/web/auth.hpp>
#include <relx/web/jwt_hs256.hpp>
#include <relx/web/pagination.hpp>

namespace {

namespace web = relx::web;

web::Request make_request(std::string target, std::string auth_header = "") {
  web::Request req{web::http::verb::get, target, 11};
  if (!auth_header.empty()) {
    req.set(web::http::field::authorization, auth_header);
  }
  return req;
}

TEST(BearerTokenTest, ExtractsToken) {
  auto req = make_request("/x", "Bearer abc.def.ghi");
  auto token = web::bearer_token(req);
  ASSERT_TRUE(token);
  EXPECT_EQ(*token, "abc.def.ghi");
}

TEST(BearerTokenTest, MissingOrMalformedHeader) {
  EXPECT_FALSE(web::bearer_token(make_request("/x")));
  EXPECT_FALSE(web::bearer_token(make_request("/x", "Basic dXNlcjpwdw==")));
}

TEST(JwtHs256Test, SignVerifyRoundTrip) {
  auto token = web::jwt::sign(R"({"sub":42,"name":"ada"})", "secret");
  auto payload = web::jwt::verify(token, "secret");
  ASSERT_TRUE(payload) << payload.error().message;
  EXPECT_NE(payload->find(R"("sub":42)"), std::string::npos);
}

TEST(JwtHs256Test, RejectsTamperedSignatureAndWrongSecret) {
  auto token = web::jwt::sign(R"({"sub":42})", "secret");
  auto tampered = token;
  tampered.back() = tampered.back() == 'A' ? 'B' : 'A';
  EXPECT_FALSE(web::jwt::verify(tampered, "secret"));
  EXPECT_FALSE(web::jwt::verify(token, "other-secret"));
  EXPECT_FALSE(web::jwt::verify("not-a-token", "secret"));
}

TEST(JwtHs256Test, HonorsExpClaim) {
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
  auto expired = web::jwt::sign(R"({"sub":1,"exp":)" + std::to_string(now - 60) + "}", "s");
  auto valid = web::jwt::sign(R"({"sub":1,"exp":)" + std::to_string(now + 3600) + "}", "s");
  auto expired_result = web::jwt::verify(expired, "s");
  ASSERT_FALSE(expired_result);
  EXPECT_EQ(expired_result.error().status, 401);
  EXPECT_TRUE(web::jwt::verify(valid, "s"));
}

TEST(AuthenticateTest, MissingTokenIs401WithoutVerifier) {
  auto req = make_request("/x");
  bool verifier_called = false;
  auto result = web::authenticate(req, [&](std::string_view) -> web::ApiResult<int> {
    verifier_called = true;
    return 1;
  });
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error().status, 401);
  EXPECT_FALSE(verifier_called);
}

TEST(PaginationTest, DefaultsAndParsing) {
  auto req = make_request("/events?limit=10&offset=5");
  web::RequestContext ctx{.req = req, .path_params = {}};
  auto page = web::page_params(ctx);
  ASSERT_TRUE(page);
  EXPECT_EQ(page->limit, 10);
  EXPECT_EQ(page->offset, 5);

  auto bare_req = make_request("/events");
  web::RequestContext bare{.req = bare_req, .path_params = {}};
  auto defaults = web::page_params(bare, 25);
  ASSERT_TRUE(defaults);
  EXPECT_EQ(defaults->limit, 25);
  EXPECT_EQ(defaults->offset, 0);
}

TEST(PaginationTest, RejectsInvalidValues) {
  for (std::string_view target : {"/e?limit=abc", "/e?limit=0", "/e?limit=9999", "/e?offset=-1"}) {
    auto req = make_request(std::string(target));
    web::RequestContext ctx{.req = req, .path_params = {}};
    EXPECT_FALSE(web::page_params(ctx)) << target;
  }
}

}  // namespace
