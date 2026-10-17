// SPDX-License-Identifier: Unlicense

#include <stringview/stringview.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static bool is_ascii_whitespace(unsigned char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
         c == '\v';
}

StringView sv_slice(StringView sv, size_t start, size_t end) {
  if (start > sv.len) {
    start = sv.len;
  }

  if (end > sv.len) {
    end = sv.len;
  }

  return sv_from_parts(sv.data ? sv.data + start : NULL,
                       start < end ? end - start : 0);
}

MutableStringView msv_slice(MutableStringView msv, size_t start, size_t end) {
  auto sliced = sv_slice(msv_as_view(msv), start, end);

  return msv_from_parts((char *)sliced.data, sliced.len);
}

StringView sv_substr(StringView sv, size_t start, size_t len) {
  if (start > sv.len) {
    start = sv.len;
  }

  if (len > sv.len - start) {
    len = sv.len - start;
  }

  return sv_slice(sv, start, start + len);
}

MutableStringView msv_substr(MutableStringView msv, size_t start, size_t len) {
  auto substr = sv_substr(msv_as_view(msv), start, len);

  return msv_from_parts((char *)substr.data, substr.len);
}

// TODO: Consider implementing Knuth–Morris–Pratt for O(n+m) instead of O(nm)?
size_t sv_find(StringView sv, StringView needle) {
  if (needle.len == 0) {
    return 0;
  }

  if (needle.len > sv.len) {
    return SV_NPOS;
  }

  for (size_t i = 0; i <= sv.len - needle.len; i++) {
    if (memcmp(sv.data + i, needle.data, needle.len) == 0) {
      return i;
    }
  }

  return SV_NPOS;
}

int sv_compare(StringView a, StringView b) {
  size_t min_len = a.len < b.len ? a.len : b.len;

  if (min_len > 0) {
    int result = memcmp(a.data, b.data, min_len);

    if (result != 0) {
      return result;
    }
  }

  return (a.len > b.len) - (a.len < b.len);
}

bool sv_equals(StringView a, StringView b) {
  return a.len == b.len && sv_compare(a, b) == 0;
}

bool sv_starts_with(StringView sv, StringView prefix) {
  return prefix.len <= sv.len &&
         (prefix.len == 0 || memcmp(sv.data, prefix.data, prefix.len) == 0);
}

bool sv_ends_with(StringView sv, StringView suffix) {
  return suffix.len <= sv.len &&
         (suffix.len == 0 ||
          memcmp(sv.data + sv.len - suffix.len, suffix.data, suffix.len) == 0);
}

StringView sv_trim(StringView sv) {
  size_t start = 0;
  size_t end = sv.len;

  while (start < end && is_ascii_whitespace((unsigned char)sv.data[start])) {
    start++;
  }

  while (end > start &&
         is_ascii_whitespace((unsigned char)sv.data[end - 1])) {
    end--;
  }

  return sv_slice(sv, start, end);
}

MutableStringView msv_trim(MutableStringView msv) {
  auto trimmed = sv_trim(msv_as_view(msv));

  return msv_from_parts((char *)trimmed.data, trimmed.len);
}

size_t sv_write(FILE *stream, StringView sv) {
  if (sv.len == 0) {
    return 0;
  }

  return fwrite(sv.data, 1, sv.len, stream);
}
