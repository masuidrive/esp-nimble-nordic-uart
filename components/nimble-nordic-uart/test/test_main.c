#include "unity.h"
#include <limits.h>

TEST_CASE("test!", "[test]") {
  //
  TEST_ASSERT_EQUAL(0, 0);
}

TEST_CASE("test?", "[test]") {
  //
  TEST_ASSERT_EQUAL(5, 5);
}

TEST_CASE("Another test case which fails", "[test][fails]") {
  //
  TEST_ASSERT_EQUAL(0, 1);
}
