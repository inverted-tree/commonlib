#pragma once

#include <stdio.h>

// A non-zero exit status makes 'make test' and CI fail, even with -DNDEBUG.
#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #expr); \
      return 1;                                                                \
    }                                                                          \
  } while (0)

#define RUN(test_fn)                                                           \
  do {                                                                         \
    if ((test_fn)() != 0) {                                                    \
      return 1;                                                                \
    }                                                                          \
  } while (0)
