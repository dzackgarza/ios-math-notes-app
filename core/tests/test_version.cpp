#include <catch2/catch_test_macros.hpp>
#include <string_view>

#include "ink.h"

TEST_CASE("ink_version is the project version") {
  REQUIRE(std::string_view(ink_version()) == INK_TEST_VERSION);
}
