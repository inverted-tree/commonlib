// SPDX-License-Identifier: Unlicense

/**
 * @file stringview.h
 * @brief Non-owning views over contiguous byte sequences.
 *
 * All lengths, positions, and indices in this API are measured in bytes, not
 * characters or Unicode code points.
 *
 * StringView objects are encoding-agnostic. They can reference ASCII, UTF-8,
 * binary data, or other byte sequences.
 *
 * Operations do not validate character encodings. Slicing a UTF-8 string may
 * split a multibyte character and produce a view that does not contain
 * well-formed UTF-8.
 *
 * All views are non-owning. The caller is responsible for ensuring that
 * referenced memory remains valid for the lifetime of each view.
 *
 * Empty views may have a NULL data pointer and a length of zero.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Sentinel value indicating that a substring was not found.
 */
#define SV_NPOS SIZE_MAX

/**
 * @brief A non-owning, read-only view over a sequence of bytes.
 *
 * A StringView does not allocate, copy, or free memory. The underlying buffer
 * must remain valid for the lifetime of the view.
 *
 * The buffer does not need to be null-terminated.
 */
typedef struct {
  char const *data; /**< Pointer to the underlying buffer. */
  size_t len;       /**< Number of bytes in the view. */
} StringView;

/**
 * @brief A non-owning, mutable view over a sequence of bytes.
 *
 * Unlike StringView, MutableStringView allows modification of the underlying
 * buffer.
 *
 * The view does not own the memory and cannot resize its buffer. The buffer
 * must remain valid and writable for the lifetime of the view.
 */
typedef struct {
  char *data; /**< Pointer to the underlying mutable buffer. */
  size_t len; /**< Number of bytes in the view. */
} MutableStringView;

/**
 * @brief Creates a StringView from an existing buffer.
 *
 * The returned view does not own or copy the underlying memory.
 *
 * @param data Pointer to the beginning of the buffer.
 * @param len Number of bytes in the view.
 *
 * @return A StringView referencing the supplied buffer.
 *
 * @pre data must point to at least len readable bytes, unless len is zero.
 * @note The buffer must remain valid for the lifetime of the view.
 */
static inline StringView sv_from_parts(char const data[], size_t len) {
  return (StringView){
      .data = data,
      .len = len,
  };
}

/**
 * @brief Creates a MutableStringView from an existing writable buffer.
 *
 * The returned view does not own or copy the underlying memory.
 *
 * @param data Pointer to the beginning of the writable buffer.
 * @param len Number of bytes in the view.
 *
 * @return A MutableStringView referencing the supplied buffer.
 *
 * @pre data must point to at least len writable bytes, unless len is zero.
 * @note The buffer must remain valid and writable for the lifetime of the view.
 */
static inline MutableStringView msv_from_parts(char data[], size_t len) {
  return (MutableStringView){
      .data = data,
      .len = len,
  };
}

/**
 * @brief Creates a StringView from a null-terminated C string.
 *
 * Determines the length using strlen(). The terminating null byte is not
 * included in the resulting view.
 *
 * @param cstr Pointer to a null-terminated C string.
 *
 * @return A StringView covering the characters before the null terminator.
 *
 * @pre cstr must point to a valid null-terminated string.
 * @note The operation has O(n) time complexity.
 * @note No memory is allocated or copied.
 */
static inline StringView sv_from_cstr(char const cstr[]) {
  return sv_from_parts(cstr, strlen(cstr));
}

/**
 * @brief Creates a MutableStringView from a writable C string.
 *
 * Determines the length using strlen(). The terminating null byte is not
 * included in the resulting view.
 *
 * @param cstr Pointer to a writable, null-terminated C string.
 *
 * @return A MutableStringView covering the characters before the null
 * terminator.
 *
 * @pre cstr must point to a valid, writable, null-terminated string.
 * @note The operation has O(n) time complexity.
 * @note Modifications through the resulting view may introduce embedded null
 * bytes.
 */
static inline MutableStringView msv_from_cstr(char cstr[]) {
  return msv_from_parts(cstr, strlen(cstr));
}

/**
 * @brief Converts a MutableStringView into a read-only StringView.
 *
 * The returned view references the same underlying memory. No allocation or
 * copying occurs.
 *
 * @param msv Mutable view to convert.
 *
 * @return A read-only StringView over the same buffer.
 *
 * @note Changes to the underlying buffer remain visible through the returned
 * view.
 */
static inline StringView msv_as_view(MutableStringView msv) {
  return (StringView){
      .data = msv.data,
      .len = msv.len,
  };
}

/**
 * @brief Extracts a view over a range of bytes.
 *
 * Returns the half-open range [start, end) without copying memory. Indices
 * exceeding the source length are clamped to its length.
 *
 * @param sv Source view.
 * @param start Inclusive starting byte offset.
 * @param end Exclusive ending byte offset.
 *
 * @return A StringView referencing the specified range. Returns an empty view
 * if start >= end after clamping.
 *
 * @note The returned view shares the lifetime of the source buffer.
 */
StringView sv_slice(StringView sv, size_t start, size_t end);

/**
 * @brief Extracts a mutable view over a range of bytes.
 *
 * Returns the half-open range [start, end) without copying memory. Indices
 * exceeding the source length are clamped to its length.
 *
 * @param msv Source mutable view.
 * @param start Inclusive starting byte offset.
 * @param end Exclusive ending byte offset.
 *
 * @return A MutableStringView referencing the specified range. Returns an empty
 * view if start >= end after clamping.
 *
 * @note Modifications through the returned view affect the original buffer.
 */
MutableStringView msv_slice(MutableStringView msv, size_t start, size_t end);

/**
 * @brief Extracts a substring of a specified maximum length.
 *
 * Returns a view starting at the given byte offset and containing at most len
 * bytes. The result is truncated if the requested length exceeds the remaining
 * bytes.
 *
 * @param sv Source view.
 * @param start Starting byte offset.
 * @param len Maximum number of bytes to include.
 *
 * @return A StringView referencing the requested substring. Returns an empty
 * view if start is beyond the source length.
 *
 * @note No memory is allocated or copied.
 */
StringView sv_substr(StringView sv, size_t start, size_t len);

/**
 * @brief Extracts a mutable substring of a specified maximum length.
 *
 * Returns a mutable view starting at the given byte offset and containing at
 * most len bytes. The result is truncated if the requested length exceeds the
 * remaining bytes.
 *
 * @param msv Source mutable view.
 * @param start Starting byte offset.
 * @param len Maximum number of bytes to include.
 *
 * @return A MutableStringView referencing the requested substring. Returns an
 * empty view if start is beyond the source length.
 *
 * @note Modifications through the returned view affect the original buffer.
 */
MutableStringView msv_substr(MutableStringView msv, size_t start, size_t len);

/**
 * @brief Finds the first occurrence of a substring.
 *
 * Searches the view for the given sequence of bytes. The search is
 * case-sensitive and supports embedded null bytes.
 *
 * @param sv View to search in.
 * @param needle Substring to search for.
 *
 * @return A byte offset of the first occurrence, or SV_NPOS if the substring is
 * not found.
 *
 * @note An empty needle is considered to match at index zero.
 */
size_t sv_find(StringView sv, StringView needle);

/**
 * @brief Finds the first occurrence of a substring in a mutable view.
 *
 * Convenience wrapper around sv_find(). Neither input is modified.
 *
 * @param msv View to search in.
 * @param needle Substring to search for.
 *
 * @return A byte offset of the first occurrence, or SV_NPOS if the substring is
 * not found.
 *
 * @note An empty needle is considered to match at index zero.
 */
static inline size_t msv_find(MutableStringView msv, StringView needle) {
  return sv_find(msv_as_view(msv), needle);
}

/**
 * @brief Lexicographically compares two string views.
 *
 * Compares the contents byte by byte, treating bytes as unsigned values. If one
 * view is a prefix of the other, the shorter view is considered smaller.
 *
 * @param a First view.
 * @param b Second view.
 *
 * @return A negative value if a < b, zero if a == b, or a positive value if a >
 * b.
 *
 * @note Comparison is case-sensitive and locale-independent.
 */
int sv_compare(StringView a, StringView b);

/**
 * @brief Lexicographically compares a mutable view with a read-only view.
 *
 * Convenience wrapper around sv_compare(). Neither input is modified.
 *
 * @param a First, mutable view.
 * @param b Second, read-only view.
 *
 * @return A negative value if a < b, zero if a == b, or a positive value if a >
 * b.
 *
 * @note Comparison is case-sensitive and locale-independent.
 */
static inline int msv_compare(MutableStringView a, StringView b) {
  return sv_compare(msv_as_view(a), b);
}

/**
 * @brief Checks whether two string views contain identical bytes.
 *
 * @param a First view.
 * @param b Second view.
 *
 * @return true if both views have the same length and contents, false
 * otherwise.
 *
 * @note Comparison is case-sensitive.
 */
bool sv_equals(StringView a, StringView b);

/**
 * @brief Checks whether a mutable view and a read-only view contain identical
 * bytes.
 *
 * Convenience wrapper around sv_equals(). Neither input is modified.
 *
 * @param a First, mutable view.
 * @param b Second, read-only view.
 *
 * @return true if both views have the same length and contents, false
 * otherwise.
 */
static inline bool msv_equals(MutableStringView a, StringView b) {
  return sv_equals(msv_as_view(a), b);
}

/**
 * @brief Checks whether a view begins with a given prefix.
 *
 * @param sv View to inspect.
 * @param prefix Prefix to search for.
 *
 * @return true if sv begins with prefix, false otherwise.
 *
 * @note An empty prefix always matches.
 */
bool sv_starts_with(StringView sv, StringView prefix);

/**
 * @brief Checks whether a mutable view begins with a given prefix.
 *
 * Convenience wrapper around sv_starts_with(). Neither input is modified.
 *
 * @param msv View to inspect.
 * @param prefix Prefix to search for.
 *
 * @return true if msv begins with prefix, false otherwise.
 *
 * @note An empty prefix always matches.
 */
static inline bool msv_starts_with(MutableStringView msv, StringView prefix) {
  return sv_starts_with(msv_as_view(msv), prefix);
}

/**
 * @brief Checks whether a view ends with a given suffix.
 *
 * @param sv View to inspect.
 * @param suffix Suffix to search for.
 *
 * @return true if sv ends with suffix, false otherwise.
 *
 * @note An empty suffix always matches.
 */
bool sv_ends_with(StringView sv, StringView suffix);

/**
 * @brief Checks whether a mutable view ends with a given suffix.
 *
 * Convenience wrapper around sv_ends_with(). Neither input is modified.
 *
 * @param msv View to inspect.
 * @param suffix Suffix to search for.
 *
 * @return true if msv ends with suffix, false otherwise.
 *
 * @note An empty suffix always matches.
 */
static inline bool msv_ends_with(MutableStringView msv, StringView suffix) {
  return sv_ends_with(msv_as_view(msv), suffix);
}

/**
 * @brief Removes leading and trailing ASCII whitespace from a view.
 *
 * Returns a new view excluding leading and trailing ASCII whitespace: space,
 * tab, newline, carriage return, form feed, and vertical tab.
 *
 * @param sv View to trim.
 *
 * @return A StringView containing the trimmed range. Returns an empty view if
 * no non-whitespace bytes remain.
 *
 * @note The underlying memory is not modified.
 * @note This operation does not perform Unicode whitespace handling.
 */
StringView sv_trim(StringView sv);

/**
 * @brief Removes leading and trailing ASCII whitespace from a mutable view.
 *
 * Returns a mutable view excluding leading and trailing ASCII whitespace. The
 * original buffer is not modified.
 *
 * @param msv Mutable view to trim.
 *
 * @return A MutableStringView containing the trimmed range. Returns an empty
 * view if no non-whitespace bytes remain.
 *
 * @note The returned view references the original writable buffer.
 * @note This operation does not perform Unicode whitespace handling.
 */
MutableStringView msv_trim(MutableStringView msv);

/**
 * @brief Writes the contents of a StringView to a stream.
 *
 * Writes up to sv.len bytes, including any embedded null bytes. The view does
 * not need to be null-terminated.
 *
 * @param stream Output stream.
 * @param sv View containing the bytes to write.
 *
 * @return Number of bytes successfully written. May be less than sv.len if a
 * write error occurs.
 *
 * @pre stream must be a valid, writable FILE pointer.
 * @note Does not append a newline or null terminator.
 * @note Use ferror(stream) to check for a stream error.
 */
size_t sv_write(FILE *stream, StringView sv);

/**
 * @brief Writes the contents of a MutableStringView to a stream.
 *
 * Convenience wrapper around sv_write(). Writes up to msv.len bytes without
 * modifying the buffer.
 *
 * @param stream Output stream.
 * @param msv Mutable view containing the bytes to write.
 *
 * @return Number of bytes successfully written. May be less than msv.len if a
 * write error occurs.
 *
 * @pre stream must be a valid, writable FILE pointer.
 * @note Does not append a newline or null terminator.
 */
static inline size_t msv_write(FILE *stream, MutableStringView msv) {
  return sv_write(stream, msv_as_view(msv));
}
