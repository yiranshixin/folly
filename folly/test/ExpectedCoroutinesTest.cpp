/*
 * Copyright 2017-present Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <folly/Expected.h>
#include <folly/Portability.h>
#include <folly/ScopeGuard.h>
#include <folly/portability/GTest.h>

using namespace folly;

namespace {

struct Exn {};

// not default-constructible, thereby preventing Expected<T, Err> from being
// default-constructible, forcing our implementation to handle such cases
class Err {
 private:
  enum class Type { Bad, Badder, Baddest };

  Type type_;

  constexpr Err(Type type) : type_(type) {}

 public:
  Err(Err const&) = default;
  Err(Err&&) = default;
  Err& operator=(Err const&) = default;
  Err& operator=(Err&&) = default;

  friend bool operator==(Err a, Err b) {
    return a.type_ == b.type_;
  }
  friend bool operator!=(Err a, Err b) {
    return a.type_ != b.type_;
  }

  static constexpr Err bad() {
    return Type::Bad;
  }
  static constexpr Err badder() {
    return Type::Badder;
  }
  static constexpr Err baddest() {
    return Type::Baddest;
  }
};

Expected<int, Err> f1() {
  return 7;
}

Expected<double, Err> f2(int x) {
  return 2.0 * x;
}

// error result
Expected<int, Err> f4(int, double, Err err) {
  return makeUnexpected(err);
}

// exception
Expected<int, Err> throws() {
  throw Exn{};
}

} // namespace

#if FOLLY_HAS_COROUTINES

TEST(Expected, CoroutineFailure) {
  auto r1 = []() -> Expected<int, Err> {
    auto x = co_await f1();
    auto y = co_await f2(x);
    auto z = co_await f4(x, y, Err::badder());
    ADD_FAILURE();
    co_return z;
  }();
  EXPECT_TRUE(r1.hasError());
  EXPECT_EQ(Err::badder(), r1.error());
}

TEST(Expected, CoroutineException) {
  EXPECT_THROW(
      ([]() -> Expected<int, Err> {
        auto x = co_await throws();
        ADD_FAILURE();
        co_return x;
      }()),
      Exn);
}

// this test makes sure that the coroutine is destroyed properly
TEST(Expected, CoroutineCleanedUp) {
  int count_dest = 0;
  auto r = [&]() -> Expected<int, Err> {
    SCOPE_EXIT {
      ++count_dest;
    };
    auto x = co_await Expected<int, Err>(makeUnexpected(Err::badder()));
    ADD_FAILURE() << "Should not be resuming";
    co_return x;
  }();
  EXPECT_FALSE(r.hasValue());
  EXPECT_EQ(1, count_dest);
}

Expected<int, Err> f5(int x) {
  return makeExpected<Err>(x * 3);
}

Expected<int, Err> f6(int n) {
  auto x = co_await f5(n);
  co_return x + 9;
}

TEST(Optional, CoroutineDanglingReference) {
  EXPECT_EQ(*f6(1), 12);
}

#endif
