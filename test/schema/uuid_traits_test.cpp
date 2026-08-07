#include <stdexcept>
#include <string>

#include <boost/uuid/uuid.hpp>
#include <gtest/gtest.h>
#include <relx/schema/uuid_traits.hpp>

namespace {

using traits = relx::schema::column_traits<boost::uuids::uuid>;

TEST(UuidTraitsTest, RoundTripsItsOwnOutput) {
  const std::string canonical = "12345678-9abc-def0-0123-456789abcdef";
  const boost::uuids::uuid parsed = traits::from_sql_string(canonical);

  // to_sql_string produces the quoted SQL-literal form, which must parse back
  const std::string quoted = traits::to_sql_string(parsed);
  EXPECT_EQ(quoted, "'" + canonical + "'");
  EXPECT_EQ(traits::from_sql_string(quoted), parsed);
}

TEST(UuidTraitsTest, InvalidTextThrowsInvalidArgument) {
  // std::invalid_argument, like every other trait (boost throws runtime_error)
  EXPECT_THROW(traits::from_sql_string("not-a-uuid"), std::invalid_argument);
  EXPECT_THROW(traits::from_sql_string(""), std::invalid_argument);
}

}  // namespace
