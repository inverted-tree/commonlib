// SPDX-License-Identifier: Unlicense

/**
 * @file argparse.h
 * @brief Allocation-free command-line argument parsing for C applications.
 *
 * Options are declared in a caller-owned array. The parser supports long
 * options (--output=file), short options (-o file), grouped short options
 * (-abc), attached short-option values (-ofile), positional arguments, and the
 * end-of-options marker (--).
 *
 * The built-in -h and --help options return ARGPARSE_HELP_REQUESTED; they never
 * print or terminate the process automatically.
 *
 * String values are returned as non-owning StringViews into argv. All lengths
 * and offsets are measured in bytes. No input strings are copied, and parsing
 * requires no heap allocation.
 *
 * The caller owns the option and positional descriptors and all destination
 * variables. They must remain valid throughout the call to arg_parse(). After
 * parsing, StringView destinations remain valid only while the original argv
 * strings remain alive and unchanged.
 *
 * Parsing may update destinations before reporting an error. Applications
 * should use parsed values only if the status is ARGPARSE_OK.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "stringview/stringview.h"

/**
 * @brief Kinds of supported named options.
 */
typedef enum {
  ARG_FLAG,   /**< Presence sets the destination bool to true. */
  ARG_STRING, /**< Consumes a byte string into a StringView destination. */
} ArgOptionKind;

/**
 * @brief Declaration and parsing state for one named option.
 *
 * At least one of long_name or short_name must be supplied. Names must be
 * unique within an ArgParser; the names "help" and 'h' are reserved.
 *
 * The value union member must agree with kind and point to a writable
 * destination. The caller initializes destination values before parsing; absent
 * options leave those values unchanged.
 *
 * arg_parse() resets seen to false at the start of every parse and sets it when
 * the option occurs. If repeated, the last string value wins; flags remain
 * true. Setting required causes absence to be reported as an error.
 */
typedef struct {
  char const *long_name; /**< Name without leading '--'; NULL if absent. */
  char short_name;       /**< Name without leading '-'; '\0' if absent. */
  ArgOptionKind kind;    /**< Whether the option is a flag or takes a value. */
  union {
    bool *flag;         /**< Destination when kind is ARG_FLAG. */
    StringView *string; /**< Destination when kind is ARG_STRING. */
  } value;               /**< Option destination selected by kind. */
  char const *value_name; /**< Help placeholder (e.g., "FILE"); optional. */
  char const *help;       /**< Help description; NULL is permitted. */
  bool required;          /**< Whether the option must appear. */
  bool seen;              /**< Written by arg_parse(); initialize to false. */
} ArgOption;

/**
 * @brief Declaration of one positional byte-string argument.
 *
 * Positional arguments are assigned left to right. All required positionals
 * must precede optional positionals. No variadic positionals are supported. An
 * absent optional position leaves its destination unchanged.
 */
typedef struct {
  char const *name;  /**< Nonempty name used in usage and errors. */
  StringView *value; /**< Destination for the borrowed argument string. */
  char const *help;  /**< Help description; NULL is permitted. */
  bool required;     /**< Whether this positional must be supplied. */
} ArgPositional;

/**
 * @brief Caller-owned parser definition.
 *
 * Set options or positionals to NULL when their respective count is zero.
 * Option descriptors are mutable because arg_parse() maintains their seen
 * fields. program_name is displayed in generated usage text.
 */
typedef struct {
  char const *program_name;   /**< Program name for usage; must not be NULL. */
  char const *description;    /**< Optional explanation for generated help. */
  ArgOption *options;         /**< Array of option_count declarations. */
  size_t option_count;        /**< Number of option declarations. */
  ArgPositional *positionals; /**< Array of positional_count declarations. */
  size_t positional_count;    /**< Number of positional declarations. */
} ArgParser;

/**
 * @brief Outcome of a call to arg_parse().
 */
typedef enum {
  ARGPARSE_OK,                          /**< All arguments parsed successfully. */
  ARGPARSE_HELP_REQUESTED,              /**< -h or --help was encountered. */
  ARGPARSE_UNKNOWN_OPTION,              /**< Option name is not declared. */
  ARGPARSE_MISSING_VALUE,               /**< Value-taking option has no value. */
  ARGPARSE_UNEXPECTED_VALUE,            /**< Flag was given an attached value. */
  ARGPARSE_MISSING_REQUIRED_OPTION,     /**< Required named option not supplied. */
  ARGPARSE_MISSING_REQUIRED_POSITIONAL, /**< Required positional not supplied. */
  ARGPARSE_TOO_MANY_POSITIONALS,        /**< More positionals than declared. */
  ARGPARSE_INVALID_SPEC,                /**< Invalid or ambiguous declaration. */
} ArgParseStatus;

/**
 * @brief Parse result with a borrowed diagnostic subject.
 *
 * On errors, subject references the offending argv token or the name of a
 * missing/invalid declaration. Its bytes are not copied. argv_index is the
 * index of the relevant argv token, or -1 if no token applies.
 *
 * On ARGPARSE_OK and ARGPARSE_HELP_REQUESTED, subject is an empty view.
 */
typedef struct {
  ArgParseStatus status; /**< Parse outcome. */
  StringView subject;    /**< Byte string relevant to an error. */
  int argv_index;        /**< argv index of error, or -1. */
} ArgParseResult;

/**
 * @brief Parses command-line arguments using a caller-defined specification.
 *
 * Supports --name=value, --name value, -n value, -nvalue, and groups of short
 * flags such as -abc. A value-taking short option consumes the rest of its
 * group or, if none remains, the next argv token. A lone '-' is treated as
 * positional; '--' ends option recognition, so later tokens are positional.
 * Long and short names are matched exactly and case-sensitively.
 *
 * Both --help and -h (including a grouped -h) request help. In that case,
 * normal required-argument checks are skipped. Help is not printed here.
 *
 * @param[in,out] parser Parser specification and writable option state.
 * @param[in] argc Number of argv entries, including the program name.
 * @param[in] argv Null-terminated argument strings as passed to main().
 *
 * @return Parse status and context for reporting errors or help requests.
 *
 * @pre @p parser and @p argv must be valid; @p argc must be at least 1.
 * @pre Each option destination and positional destination must be valid.
 * @note
 * The caller must initialize option/positional destinations to their desired
 * defaults before calling this function.
 * @note
 * This function resets only the option seen fields, not destinations.
 * @warning On error, some destination variables may already have changed.
 */
ArgParseResult arg_parse(ArgParser *parser, int argc, char *const argv[]);

/**
 * @brief Prints generated usage and option descriptions.
 *
 * Includes declared options, positional arguments, and the built-in -h/--help
 * option. Does not allocate application-owned memory or terminate.
 *
 * @param[in,out] stream Writable output stream, typically stdout.
 * @param[in] parser Valid parser specification.
 *
 * @pre @p stream and @p parser must be non-NULL.
 * @note
 * Check ferror(stream) to detect stream write errors.
 */
void arg_print_help(FILE *stream, ArgParser const *parser);

/**
 * @brief Prints a human-readable diagnostic for a parsing error.
 *
 * This function prints the error described by result but does not print usage
 * text or terminate the program. It should be called only for error statuses,
 * not ARGPARSE_OK or ARGPARSE_HELP_REQUESTED.
 *
 * @param[in,out] stream Writable output stream, typically stderr.
 * @param[in] result Result returned by arg_parse().
 *
 * @pre @p stream must be non-NULL.
 * @note
 * Check ferror(stream) to detect stream write errors.
 */
void arg_print_error(FILE *stream, ArgParseResult result);
