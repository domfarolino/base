#include <stdio.h>

#include "base/build_config.h"

#include "gtest/gtest.h"

TEST(DummyBuildConfigTest, OutputCorrectPlatform) {
#if defined(OS_MACOS)
  printf("On macOS\n");
#elif defined(OS_LINUX)
  printf("On Linux\n");
#endif
}
