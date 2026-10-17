// SPDX-License-Identifier: Unlicense

#include <argparse/argparse.h>

#include <stdio.h>
#include <string.h>

static ArgParseResult arg_result(ArgParseStatus status, StringView subject,
                                 int argv_index) {
  return (ArgParseResult){
      .status = status,
      .subject = subject,
      .argv_index = argv_index,
  };
}

static ArgParseResult arg_success(ArgParseStatus status) {
  return arg_result(status, (StringView){0}, -1);
}

static ArgParseResult arg_token_error(ArgParseStatus status, char const *token,
                                      int argv_index) {
  return arg_result(status, sv_from_cstr(token), argv_index);
}

static StringView arg_option_name(ArgOption const *option) {
  if (option->long_name != NULL) {
    return sv_from_cstr(option->long_name);
  }

  if (option->short_name != '\0') {
    return sv_from_parts(&option->short_name, 1);
  }

  return (StringView){0};
}

static ArgParseResult arg_invalid_spec(StringView subject) {
  return arg_result(ARGPARSE_INVALID_SPEC, subject, -1);
}

static bool arg_valid_long_name(char const name[]) {
  if (name == NULL || name[0] == '\0' || name[0] == '-') {
    return false;
  }

  for (unsigned char const *p = (unsigned char const *)name; *p != 0; p++) {
    if (*p <= ' ' || *p == 127 || *p == '=') {
      return false;
    }
  }

  return strcmp(name, "help") != 0;
}

static bool arg_valid_short_name(char name) {
  unsigned char c = (unsigned char)name;

  return c > ' ' && c != 127 && c != '-' && c != '=' && c != 'h';
}

static ArgParseResult arg_validate_spec(ArgParser const *parser) {
  if (parser == NULL || parser->program_name == NULL ||
      (parser->option_count != 0 && parser->options == NULL) ||
      (parser->positional_count != 0 && parser->positionals == NULL)) {
    return arg_invalid_spec((StringView){0});
  }

  for (size_t i = 0; i < parser->option_count; i++) {
    ArgOption const *option = &parser->options[i];
    StringView name = arg_option_name(option);

    if ((option->long_name == NULL && option->short_name == '\0') ||
        (option->long_name != NULL &&
         !arg_valid_long_name(option->long_name)) ||
        (option->short_name != '\0' &&
         !arg_valid_short_name(option->short_name)) ||
        (option->kind != ARG_FLAG && option->kind != ARG_STRING) ||
        (option->kind == ARG_FLAG && option->value.flag == NULL) ||
        (option->kind == ARG_STRING && option->value.string == NULL)) {
      return arg_invalid_spec(name);
    }

    for (size_t j = 0; j < i; j++) {
      ArgOption const *earlier = &parser->options[j];

      if ((option->long_name != NULL && earlier->long_name != NULL &&
           strcmp(option->long_name, earlier->long_name) == 0) ||
          (option->short_name != '\0' &&
           option->short_name == earlier->short_name)) {
        return arg_invalid_spec(name);
      }
    }
  }

  bool saw_optional = false;
  for (size_t i = 0; i < parser->positional_count; i++) {
    ArgPositional const *positional = &parser->positionals[i];
    StringView name = positional->name == NULL ? (StringView){0}
                                               : sv_from_cstr(positional->name);

    if (positional->name == NULL || positional->name[0] == '\0' ||
        positional->value == NULL || (saw_optional && positional->required)) {
      return arg_invalid_spec(name);
    }

    if (!positional->required) {
      saw_optional = true;
    }
  }

  return arg_success(ARGPARSE_OK);
}

static ArgOption *arg_find_long(ArgParser *parser, char const *name,
                                size_t len) {
  for (size_t i = 0; i < parser->option_count; i++) {
    ArgOption *option = &parser->options[i];

    if (option->long_name != NULL && strlen(option->long_name) == len &&
        memcmp(option->long_name, name, len) == 0) {
      return option;
    }
  }

  return NULL;
}

static ArgOption *arg_find_short(ArgParser *parser, char name) {
  for (size_t i = 0; i < parser->option_count; i++) {
    if (parser->options[i].short_name == name) {
      return &parser->options[i];
    }
  }

  return NULL;
}

static ArgParseResult arg_parse_long(ArgParser *parser, int argc,
                                     char *const argv[], int *index) {
  char const *token = argv[*index];
  char const *name = token + 2;
  char const *equal = strchr(name, '=');
  size_t name_len = equal == NULL ? strlen(name) : (size_t)(equal - name);

  if (name_len == 4 && memcmp(name, "help", 4) == 0) {
    return equal == NULL
               ? arg_success(ARGPARSE_HELP_REQUESTED)
               : arg_token_error(ARGPARSE_UNEXPECTED_VALUE, token, *index);
  }

  ArgOption *option = arg_find_long(parser, name, name_len);
  if (option == NULL) {
    return arg_token_error(ARGPARSE_UNEXPECTED_VALUE, token, *index);
  }

  if (option->kind == ARG_FLAG) {
    if (equal != NULL) {
      return arg_token_error(ARGPARSE_UNEXPECTED_VALUE, token, *index);
    }
    *option->value.flag = true;
  } else if (equal != NULL) {
    *option->value.string = sv_from_cstr(equal + 1);
  } else {
    if (*index == argc - 1) {
      return arg_token_error(ARGPARSE_MISSING_VALUE, token, *index);
    }
    ++*index;
    *option->value.string = sv_from_cstr(argv[*index]);
  }

  option->seen = true;
  return arg_success(ARGPARSE_OK);
}

static ArgParseResult arg_parse_short(ArgParser *parser, int argc,
                                      char *const argv[], int *index) {
  char const *token = argv[*index];

  for (char const *name = token + 1; *name != '\0'; name++) {
    if (*name == 'h') {
      return arg_success(ARGPARSE_HELP_REQUESTED);
    }

    ArgOption *option = arg_find_short(parser, *name);
    if (option == NULL) {
      return arg_token_error(ARGPARSE_UNKNOWN_OPTION, token, *index);
    }

    if (option->kind == ARG_FLAG) {
      *option->value.flag = true;
      option->seen = true;
      continue;
    }

    // A value-taking option consumes the rest of its group, if present.
    // Supports -o=value as well as -ovalue.
    char const *attached = name + 1;
    if (*attached != '\0') {
      if (*attached == '=') {
        attached++;
      }
      *option->value.string = sv_from_cstr(attached);
    } else {
      if (*index == argc - 1) {
        return arg_token_error(ARGPARSE_MISSING_VALUE, token, *index);
      }
      ++*index;
      *option->value.string = sv_from_cstr(argv[*index]);
    }

    option->seen = true;
    return arg_success(ARGPARSE_OK);
  }

  return arg_success(ARGPARSE_OK);
}

ArgParseResult arg_parse(ArgParser *parser, int argc, char *const argv[]) {
  ArgParseResult valid = arg_validate_spec(parser);

  if (valid.status != ARGPARSE_OK) {
    return valid;
  }

  if (argc < 1 || argv == NULL || argv[0] == NULL) {
    return arg_invalid_spec((StringView){0});
  }

  for (size_t i = 0; i < parser->option_count; i++) {
    parser->options[i].seen = false;
  }

  size_t positional_index = 0;
  bool positional_only = false;

  for (int i = 0; i < argc; i++) {
    char const *token = argv[i];

    if (token == NULL) {
      return arg_invalid_spec((StringView){0});
    }

    if (!positional_only && strcmp(token, "--") == 0) {
      positional_only = true;
      continue;
    }

    if (!positional_only && token[0] == '-' && token[1] == '-' &&
        token[2] != '\0') {
      ArgParseResult result = arg_parse_long(parser, argc, argv, &i);

      if (result.status != ARGPARSE_OK) {
        return result;
      }
      continue;
    }

    if (!positional_only && token[0] == '-' && token[1] != '\0') {
      ArgParseResult result = arg_parse_short(parser, argc, argv, &i);

      if (result.status != ARGPARSE_OK) {
        return result;
      }
      continue;
    }

    if (positional_index == parser->positional_count) {
      return arg_token_error(ARGPARSE_TOO_MANY_POSITIONALS, token, i);
    }

    *parser->positionals[positional_index++].value = sv_from_cstr(token);
  }

  for (size_t i = 0; i < parser->option_count; i++) {
    ArgOption const *option = &parser->options[i];

    if (option->required && !option->seen) {
      return arg_result(ARGPARSE_MISSING_REQUIRED_OPTION,
                        arg_option_name(option), -1);
    }
  }

  if (positional_index < parser->positional_count &&
      parser->positionals[positional_index].required) {
    return arg_result(ARGPARSE_MISSING_REQUIRED_POSITIONAL,
                      sv_from_cstr(parser->positionals[positional_index].name),
                      -1);
  }

  return arg_success(ARGPARSE_OK);
}

static void arg_print_option_name(FILE *stream, ArgOption const *option) {
  if (option->short_name != '\0') {
    fprintf(stream, "-%c", option->short_name);
  }

  if (option->long_name != NULL) {
    if (option->short_name != '\0') {
      fputs(", ", stream);
    }
    fprintf(stream, "--%s", option->long_name);
  }

  if (option->kind == ARG_STRING) {
    fprintf(stream, " %s",
            option->value_name != NULL ? option->value_name : "VALUE");
  }
}

void arg_print_help(FILE *stream, ArgParser const *parser) {
  fprintf(stream, "Usage: %s [-h]", parser->program_name);

  for (size_t i = 0; i < parser->option_count; i++) {
    ArgOption const *option = &parser->options[i];
    fputc(' ', stream);

    if (!option->required) {
      fputc('[', stream);
    }
    if (option->short_name != '\0') {
      fprintf(stream, "-%c", option->short_name);
    } else {
      fprintf(stream, "--%s", option->long_name);
    }
    if (option->kind == ARG_STRING) {
      fprintf(stream, " %s",
              option->value_name != NULL ? option->value_name : "VALUE");
    }
    if (!option->required) {
      fputc(']', stream);
    }
  }

  for (size_t i = 0; i < parser->positional_count; i++) {
    ArgPositional const *positional = &parser->positionals[i];
    fprintf(stream, positional->required ? " %s" : "[%s]", positional->name);
  }

  fputc('\n', stream);

  if (parser->description != NULL && parser->description[0] != '\0') {
    fprintf(stream, "\n%s\n", parser->description);
  }

  if (parser->positional_count != 0) {
    fputs("\nPositional arguments:\n", stream);

    for (size_t i = 0; i < parser->positional_count; i++) {
      ArgPositional const *positional = &parser->positionals[i];
      fprintf(stream, " %s", positional->name);

      if (positional->help != NULL) {
        fprintf(stream, "\t%s", positional->help);
      }

      fputc('\n', stream);
    }
  }

  fputs("\nOptions:\n  -h, --help\tShow this help message\n", stream);

  for (size_t i = 0; i < parser->option_count; i++) {
    ArgOption const *option = &parser->options[i];
    fputs(" ", stream);

    arg_print_option_name(stream, option);
    if (option->help != NULL) {
      fprintf(stream, "\t%s", option->help);
    }

    if (option->required) {
      fputs(" (required)", stream);
    }

    fputc('\n', stream);
  }
}

static void arg_print_subject(FILE *stream, StringView subject) {
  if (subject.len != 0) {
    (void)fwrite(subject.data, 1, subject.len, stream);
  }
}

void arg_print_error(FILE *stream, ArgParseResult result) {
  char const *description = NULL;

  switch (result.status) {
  case ARGPARSE_UNKNOWN_OPTION:
    description = "unknown option";
    break;
  case ARGPARSE_MISSING_VALUE:
    description = "option requires a value";
    break;
  case ARGPARSE_UNEXPECTED_VALUE:
    description = "option does not accept a value";
    break;
  case ARGPARSE_MISSING_REQUIRED_OPTION:
    description = "missing required option";
    break;
  case ARGPARSE_MISSING_REQUIRED_POSITIONAL:
    description = "missing required positional argument";
    break;
  case ARGPARSE_TOO_MANY_POSITIONALS:
    description = "unexpected positional argument";
    break;
  case ARGPARSE_INVALID_SPEC:
    description = "invalid parser specification";
    break;
  case ARGPARSE_OK:
  case ARGPARSE_HELP_REQUESTED:
    return;
  }

  fprintf(stream, "error: %s", description);
  if (result.subject.len != 0) {
    fputs(": '", stream);
    arg_print_subject(stream, result.subject);
    fputc('\'', stream);
  }

  fputc('\n', stream);
}
