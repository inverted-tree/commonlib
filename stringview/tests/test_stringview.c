#include "stringview/stringview.h"
#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SV(text) sv_from_cstr(text)
#define VIEW_IS(view, text) CHECK(sv_equals((view), SV(text)))

static int test_construction(void) {
  char buffer[] = "hello";
  StringView a = sv_from_cstr(buffer);
  CHECK(a.data == buffer && a.len == 5);
  CHECK(sv_from_cstr("").len == 0);

  const char bytes[] = {'a', '\0', 'b'};
  StringView binary = sv_from_parts(bytes, sizeof bytes);
  CHECK(binary.data == bytes && binary.len == 3);
  CHECK(sv_from_parts(NULL, 0).len == 0);

  MutableStringView mutable = msv_from_cstr(buffer);
  CHECK(mutable.data == buffer && mutable.len == 5);
  CHECK(msv_from_parts(NULL, 0).len == 0);
  StringView read_only = msv_as_view(mutable);
  CHECK(read_only.data == buffer && read_only.len == 5);

  return 0;
}

static int test_slices(void) {
  StringView source = SV("abcdef");
  VIEW_IS(sv_slice(source, 4, SIZE_MAX), "ef");
  CHECK(sv_slice(source, 4, 2).len == 0);
  CHECK(sv_slice(source, SIZE_MAX, SIZE_MAX).len == 0);
  CHECK(sv_slice(sv_from_parts(NULL, 0), 0, 1).len == 0);

  VIEW_IS(sv_substr(source, 2, 2), "cd");
  VIEW_IS(sv_substr(source, 2, SIZE_MAX), "cdef");
  CHECK(sv_substr(source, SIZE_MAX, 2).len == 0);
  CHECK(sv_substr(sv_from_parts(NULL, 0), 0, SIZE_MAX).len == 0);
  return 0;
}

static int test_find(void) {
  CHECK(sv_find(SV("abracadabra"), SV("cad")) == 4);
  CHECK(sv_find(SV("aaaa"), SV("aa")) == 0);
  CHECK(sv_find(SV("aaab"), SV("aab")) == 1); /* Overlap */
  CHECK(sv_find(SV("abc"), SV("xyz")) == SV_NPOS);
  CHECK(sv_find(SV("ab"), SV("abc")) == SV_NPOS);
  CHECK(sv_find(SV("abc"), SV("")) == 0);
  CHECK(sv_find(sv_from_parts(NULL, 0), sv_from_parts(NULL, 0)) == 0);

  const char haystack[] = {'a', '\0', 'b', '\0', 'c'};
  const char needle[] = {'b', '\0'};
  CHECK(sv_find(sv_from_parts(haystack, sizeof haystack),
                sv_from_parts(needle, sizeof needle)) == 2);
  return 0;
}

static int test_comparison(void) {
  CHECK(sv_compare(SV("abc"), SV("abc")) == 0);
  CHECK(sv_compare(SV("abc"), SV("abd")) < 0);
  CHECK(sv_compare(SV("abd"), SV("abc")) > 0);
  CHECK(sv_compare(SV("ab"), SV("abc")) < 0);
  CHECK(sv_compare(SV("abc"), SV("ab")) > 0);
  CHECK(sv_compare(sv_from_parts(NULL, 0), SV("")) == 0);
  CHECK(sv_compare(sv_from_parts(NULL, 0), SV("a")) < 0);

  const char high[] = {(char)0xff};
  const char low[] = {(char)0x01};
  CHECK(sv_compare(sv_from_parts(high, 1), sv_from_parts(low, 1)) > 0);
  CHECK(sv_equals(SV("same"), SV("same")));
  CHECK(!sv_equals(SV("same"), SV("Same")));
  CHECK(!sv_equals(SV("abc"), SV("ab")));
  return 0;
}

static int test_prefix_suffix(void) {
  StringView a = SV("testing");
  CHECK(sv_starts_with(a, SV("test")));
  CHECK(!sv_starts_with(a, SV("Test")));
  CHECK(!sv_starts_with(a, SV("testing!")));
  CHECK(sv_starts_with(a, sv_from_parts(NULL, 0)));
  CHECK(sv_ends_with(a, SV("ing")));
  CHECK(!sv_ends_with(a, SV("in")));
  CHECK(!sv_ends_with(a, SV("!testing")));
  CHECK(sv_ends_with(a, sv_from_parts(NULL, 0)));
  CHECK(sv_starts_with(sv_from_parts(NULL, 0), SV("")));
  CHECK(sv_ends_with(sv_from_parts(NULL, 0), SV("")));
  return 0;
}

static int test_trim(void) {
  StringView source = SV(" \t\n\v\f\r A B \r\f\v\n\t ");
  VIEW_IS(sv_trim(source), "A B");
  VIEW_IS(sv_trim(SV("already trim")), "already trim");
  CHECK(sv_trim(SV(" \t\n\r\v\f ")).len == 0);
  CHECK(sv_trim(sv_from_parts(NULL, 0)).len == 0);

  char mutable[] = "  xy \n";
  MutableStringView msv = msv_trim(msv_from_cstr(mutable));
  CHECK(msv.len == 2 && msv.data == mutable + 2);
  msv.data[0] = 'X';
  CHECK(mutable[2] == 'X');
  return 0;
}

static int test_mutable_wrappers(void) {
  char data[] = "ABCDE";
  char pattern[] = "BCD";
  MutableStringView msv = msv_from_cstr(data);
  MutableStringView needle = msv_from_cstr(pattern);
  CHECK(msv_find(msv, msv_as_view(needle)) == 1);
  CHECK(msv_compare(needle, msv_as_view(needle)) == 0);
  CHECK(msv_equals(needle, msv_as_view(needle)));
  CHECK(msv_starts_with(msv_substr(msv, 1, 3), msv_as_view(needle)));
  CHECK(msv_ends_with(msv_slice(msv, 0, 4), msv_as_view(needle)));

  MutableStringView part = msv_substr(msv, 2, SIZE_MAX);
  CHECK(part.len == 3);
  part.data[0] = 'x';
  CHECK(data[2] == 'x');
  MutableStringView slice = msv_slice(msv, 0, 2);
  CHECK(slice.len == 2);
  slice.data[0] = 'a';
  CHECK(data[0] == 'a');
  CHECK(msv_slice(msv_from_parts(NULL, 0), 0, 1).len == 0);
  CHECK(msv_substr(msv, SIZE_MAX, 1).len == 0);
  return 0;
}

static int test_write(void) {
  const char bytes[] = {'A', '\0', 'B', '\n'};
  StringView binary = sv_from_parts(bytes, sizeof bytes);
  FILE *stream = tmpfile();
  CHECK(stream != NULL);
  CHECK(sv_write(stream, binary) == sizeof bytes);
  CHECK(sv_write(stream, sv_from_parts(NULL, 0)) == 0);
  CHECK(fflush(stream) == 0);
  CHECK(fseek(stream, 0, SEEK_SET) == 0);
  char result[sizeof bytes + 1] = {0};
  CHECK(fread(result, 1, sizeof result, stream) == sizeof bytes);
  CHECK(memcmp(result, bytes, sizeof bytes) == 0);
  CHECK(fclose(stream) == 0);

  char mutable[] = {'x', '\0', 'y'};
  stream = tmpfile();
  CHECK(stream != NULL);
  CHECK(msv_write(stream, msv_from_parts(mutable, sizeof mutable)) ==
        sizeof mutable);
  CHECK(fflush(stream) == 0);
  CHECK(fseek(stream, 0, SEEK_SET) == 0);
  char actual[sizeof mutable];
  CHECK(fread(actual, 1, sizeof actual, stream) == sizeof actual);
  CHECK(memcmp(actual, mutable, sizeof mutable) == 0);
  CHECK(fclose(stream) == 0);
  return 0;
}

int main(void) {
  RUN(test_construction);
  RUN(test_slices);
  RUN(test_find);
  RUN(test_comparison);
  RUN(test_prefix_suffix);
  RUN(test_trim);
  RUN(test_mutable_wrappers);
  RUN(test_write);
  puts("StringView tests passed");
  return 0;
}
