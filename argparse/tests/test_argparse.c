// SPDX-License-Identifier: Unlicense

#include "argparse/argparse.h"
#include "test.h"

#include <string.h>

#define COUNT(xs) ((int)(sizeof(xs) / sizeof((xs)[0])))
#define PARSE(parser, args) arg_parse(&(parser), COUNT(args), (args))

static bool is(StringView sv, const char *text) {
  size_t len = strlen(text);
  return sv.len == len && (len == 0 || memcmp(sv.data, text, len) == 0);
}

static int test_long_options_and_defaults(void) {
  bool verbose = false;
  StringView output = sv_from_cstr("default"), input = {0}, extra = {0};
  ArgOption options[] = {
      {.long_name = "verbose", .short_name = 'v', .kind = ARG_FLAG,
       .value.flag = &verbose},
      {.long_name = "output", .short_name = 'o', .kind = ARG_STRING,
       .value.string = &output, .required = true},
  };
  ArgPositional positionals[] = {
      {.name = "input", .value = &input, .required = true},
      {.name = "extra", .value = &extra},
  };
  ArgParser p = {.program_name = "tool", .options = options, .option_count = 2,
                 .positionals = positionals, .positional_count = 2};
  char *args[] = {"tool", "--verbose", "--output=first", "--output", "-second",
                  "input.c", "optional"};
  ArgParseResult r = PARSE(p, args);
  CHECK(r.status == ARGPARSE_OK && r.subject.len == 0 && r.argv_index == -1);
  CHECK(verbose && options[0].seen && options[1].seen);
  CHECK(is(output, "-second") && output.data == args[4]);
  CHECK(is(input, "input.c") && input.data == args[5] && is(extra, "optional"));

  options[0].seen = true;
  char *again[] = {"tool", "--output="};
  CHECK(PARSE(p, again).status == ARGPARSE_MISSING_REQUIRED_POSITIONAL);
  CHECK(!options[0].seen && options[1].seen && output.len == 0);
  CHECK(verbose && is(input, "input.c")); // Missing args retain destinations.
  return 0;
}

static int test_short_options(void) {
  bool a = false, b = false;
  StringView out = {0};
  ArgOption options[] = {
      {.short_name = 'a', .kind = ARG_FLAG, .value.flag = &a},
      {.short_name = 'b', .kind = ARG_FLAG, .value.flag = &b},
      {.short_name = 'o', .kind = ARG_STRING, .value.string = &out},
  };
  ArgParser p = {.program_name = "tool", .options = options, .option_count = 3};
  char *attached[] = {"tool", "-abofile"};
  CHECK(PARSE(p, attached).status == ARGPARSE_OK);
  CHECK(a && b && is(out, "file"));
  char *equal[] = {"tool", "-o=other"};
  CHECK(PARSE(p, equal).status == ARGPARSE_OK && is(out, "other"));
  CHECK(!options[0].seen && !options[1].seen && options[2].seen);
  char *separate[] = {"tool", "-o", "-literal"};
  CHECK(PARSE(p, separate).status == ARGPARSE_OK && is(out, "-literal"));
  char *empty[] = {"tool", "-o="};
  CHECK(PARSE(p, empty).status == ARGPARSE_OK && out.len == 0);
  char *missing[] = {"tool", "-o"};
  CHECK(PARSE(p, missing).status == ARGPARSE_MISSING_VALUE);
  char *unknown[] = {"tool", "-az"};
  ArgParseResult r = PARSE(p, unknown);
  CHECK(r.status == ARGPARSE_UNKNOWN_OPTION && r.argv_index == 1);
  CHECK(is(r.subject, "-az"));
  return 0;
}

static int test_positionals_and_help(void) {
  bool flag = false;
  StringView a = {0}, b = {0}, c = {0};
  ArgOption option = {.short_name = 'f', .kind = ARG_FLAG,
                      .value.flag = &flag, .required = true};
  ArgPositional pos[] = {
      {.name = "first", .value = &a, .required = true},
      {.name = "second", .value = &b},
      {.name = "third", .value = &c},
  };
  ArgParser p = {.program_name = "tool", .options = &option, .option_count = 1,
                 .positionals = pos, .positional_count = 3};
  char *args[] = {"tool", "-f", "-", "--", "--help", "-x"};
  CHECK(PARSE(p, args).status == ARGPARSE_OK);
  CHECK(flag && is(a, "-") && is(b, "--help") && is(c, "-x"));
  char *help[] = {"tool", "--help"};
  CHECK(PARSE(p, help).status == ARGPARSE_HELP_REQUESTED);
  char *short_help[] = {"tool", "-fh"};
  CHECK(PARSE(p, short_help).status == ARGPARSE_HELP_REQUESTED);
  char *help_value[] = {"tool", "--help=yes"};
  CHECK(PARSE(p, help_value).status == ARGPARSE_UNEXPECTED_VALUE);
  char *too_many[] = {"tool", "-f", "1", "2", "3", "4"};
  ArgParseResult r = PARSE(p, too_many);
  CHECK(r.status == ARGPARSE_TOO_MANY_POSITIONALS && r.argv_index == 5);
  CHECK(is(r.subject, "4"));
  char *no_args[] = {"tool"};
  r = PARSE(p, no_args);
  CHECK(r.status == ARGPARSE_MISSING_REQUIRED_OPTION && is(r.subject, "f"));
  char *no_input[] = {"tool", "-f"};
  r = PARSE(p, no_input);
  CHECK(r.status == ARGPARSE_MISSING_REQUIRED_POSITIONAL && is(r.subject, "first"));
  return 0;
}

static int test_errors_and_spec(void) {
  bool enabled = false;
  StringView output = {0};
  ArgOption options[] = {
      {.long_name = "enabled", .short_name = 'e', .kind = ARG_FLAG,
       .value.flag = &enabled},
      {.long_name = "output", .short_name = 'o', .kind = ARG_STRING,
       .value.string = &output},
  };
  ArgParser p = {.program_name = "tool", .options = options, .option_count = 2};
  char *unknown[] = {"tool", "--unknown"};
  ArgParseResult r = PARSE(p, unknown);
  CHECK(r.status == ARGPARSE_UNKNOWN_OPTION && r.argv_index == 1);
  CHECK(is(r.subject, "--unknown"));
  char *missing[] = {"tool", "--output"};
  CHECK(PARSE(p, missing).status == ARGPARSE_MISSING_VALUE);
  char *unexpected[] = {"tool", "--enabled=yes"};
  CHECK(PARSE(p, unexpected).status == ARGPARSE_UNEXPECTED_VALUE);

  char *empty[] = {"tool"};
#define INVALID() CHECK(PARSE(p, empty).status == ARGPARSE_INVALID_SPEC)
  options[1].long_name = "enabled"; INVALID(); // Duplicate long name.
  options[1].long_name = "output"; options[1].short_name = 'e'; INVALID();
  options[1].short_name = 'h'; INVALID(); // Reserved short name.
  options[1].short_name = 'o'; options[1].long_name = "help"; INVALID();
  options[1].long_name = "-bad"; INVALID();
  options[1].long_name = "bad=name"; INVALID();
  options[1].long_name = "bad name"; INVALID();
  options[1].long_name = "output"; options[1].value.string = NULL; INVALID();
  options[1].value.string = &output; options[1].kind = (ArgOptionKind)42; INVALID();
  options[1].kind = ARG_STRING;
  options[1].long_name = NULL; options[1].short_name = '\0'; INVALID();
  options[1].short_name = 'o';
  p.options = NULL; INVALID(); p.options = options;
  p.program_name = NULL; INVALID(); p.program_name = "tool";
  p.positional_count = 1; INVALID(); // Missing positional declarations.
  StringView first = {0}, second = {0};
  ArgPositional pos[] = {{.name = "optional", .value = &first},
                         {.name = "required", .value = &second, .required = true}};
  p.positionals = pos; p.positional_count = 2; INVALID();
  pos[1].required = false; pos[0].name = NULL; INVALID();
  pos[0].name = "optional"; pos[0].value = NULL; INVALID();
  p.positionals = NULL; p.positional_count = 0;
  CHECK(arg_parse(NULL, 1, empty).status == ARGPARSE_INVALID_SPEC);
  CHECK(arg_parse(&p, 0, empty).status == ARGPARSE_INVALID_SPEC);
  CHECK(arg_parse(&p, 1, NULL).status == ARGPARSE_INVALID_SPEC);
  char *bad_argv0[] = {NULL};
  CHECK(PARSE(p, bad_argv0).status == ARGPARSE_INVALID_SPEC);
  char *bad_token[] = {"tool", NULL};
  CHECK(PARSE(p, bad_token).status == ARGPARSE_INVALID_SPEC);
#undef INVALID
  return 0;
}

static int test_output(void) {
  bool verbose = false;
  StringView input = {0}, path = {0};
  ArgOption opts[] = {
      {.long_name = "verbose", .short_name = 'v', .kind = ARG_FLAG,
       .value.flag = &verbose, .help = "Verbose"},
      {.long_name = "output", .kind = ARG_STRING, .value.string = &path,
       .required = true, .value_name = "FILE"},
  };
  ArgPositional pos[] = {{.name = "input", .value = &input,
                          .help = "Input filename", .required = true}};
  ArgParser p = {.program_name = "tool", .description = "Tool description",
                 .options = opts, .option_count = 2,
                 .positionals = pos, .positional_count = 1};
  FILE *stream = tmpfile();
  CHECK(stream != NULL);
  arg_print_help(stream, &p);
  CHECK(fflush(stream) == 0 && fseek(stream, 0, SEEK_SET) == 0);
  char buf[2048] = {0};
  size_t n = fread(buf, 1, sizeof(buf) - 1, stream);
  CHECK(n > 0 && strstr(buf, "Usage: tool") && strstr(buf, "--output FILE"));
  CHECK(strstr(buf, "-h, --help") && strstr(buf, "Tool description"));
  CHECK(strstr(buf, "\n  -v, --verbose\tVerbose\n"));
  CHECK(strstr(buf, "Input filename") && strstr(buf, "(required)"));
  CHECK(fclose(stream) == 0);

  const struct { ArgParseStatus status; const char *message; } cases[] = {
      {ARGPARSE_UNKNOWN_OPTION, "unknown option"},
      {ARGPARSE_MISSING_VALUE, "option requires a value"},
      {ARGPARSE_UNEXPECTED_VALUE, "option does not accept a value"},
      {ARGPARSE_MISSING_REQUIRED_OPTION, "missing required option"},
      {ARGPARSE_MISSING_REQUIRED_POSITIONAL, "missing required positional"},
      {ARGPARSE_TOO_MANY_POSITIONALS, "unexpected positional"},
      {ARGPARSE_INVALID_SPEC, "invalid parser specification"},
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    stream = tmpfile();
    CHECK(stream != NULL);
    arg_print_error(stream, (ArgParseResult){cases[i].status,
                         sv_from_cstr("TEST"), 1});
    CHECK(fflush(stream) == 0 && fseek(stream, 0, SEEK_SET) == 0);
    memset(buf, 0, sizeof(buf));
    CHECK(fread(buf, 1, sizeof(buf) - 1, stream) > 0);
    CHECK(strstr(buf, cases[i].message) && strstr(buf, "TEST"));
    CHECK(fclose(stream) == 0);
  }
  stream = tmpfile();
  CHECK(stream != NULL);
  arg_print_error(stream, (ArgParseResult){.status = ARGPARSE_OK});
  arg_print_error(stream, (ArgParseResult){.status = ARGPARSE_HELP_REQUESTED});
  CHECK(fflush(stream) == 0 && ftell(stream) == 0);
  CHECK(fclose(stream) == 0);
  return 0;
}

int main(void) {
  RUN(test_long_options_and_defaults);
  RUN(test_short_options);
  RUN(test_positionals_and_help);
  RUN(test_errors_and_spec);
  RUN(test_output);
  puts("argparse: all tests passed");
  return 0;
}
