#ifndef BASE_CHECK_H_
#define BASE_CHECK_H_

#include <assert.h>

namespace base {

#define CHECK(condition) assert(condition)
#define CHECK_EQ(actual, expected) CHECK(actual == expected)
#define CHECK_NE(actual, expected) CHECK(actual != expected)
#define CHECK_GE(actual, expected) CHECK(actual >= expected)
#define CHECK_LT(actual, expected) CHECK(actual < expected)
#define NOTREACHED() CHECK(false)

} // namespace base

#endif // BASE_CHECK_H_
