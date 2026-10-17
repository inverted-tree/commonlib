# argparse

A small, allocation-free command-line argument parser.
Supports long and short options, grouped flags, string values, positional arguments, `--`, and built-in `-h` / `--help` handling.

## Build

From the repository root:
- release mode: `make argparse`
- debug mode: `make argparse BUILD=debug`
- unit tests: `make test-argparse BUILD=debug`

The packaged archive and headers are in `dist/<build>/argparse/`.
The single `libargparse.a` includes all its dependencies.

## Example

```c
#include "argparse/argparse.h"

#include <stdio.h>

int main(int argc, char **argv) {
    bool verbose = false;
    StringView output = {0};
    StringView input = {0};

    ArgOption options[] = {
        { .long_name = "verbose", .short_name = 'v', .kind = ARG_FLAG,
          .value.flag = &verbose, .help = "Enable verbose output" },
        { .long_name = "output", .short_name = 'o', .kind = ARG_STRING,
          .value.string = &output, .value_name = "FILE",
          .help = "Output file" },
    };
    ArgPositional positionals[] = {
        { .name = "input", .value = &input, .required = true,
          .help = "Input file" },
    };
    ArgParser parser = {
        .program_name = argv[0],
        .options = options,
        .option_count = sizeof options / sizeof options[0],
        .positionals = positionals,
        .positional_count = sizeof positionals / sizeof positionals[0],
    };

    ArgParseResult result = arg_parse(&parser, argc, argv);
    if (result.status == ARGPARSE_HELP_REQUESTED) {
        arg_print_help(stdout, &parser);
        return 0;
    }
    if (result.status != ARGPARSE_OK) {
        arg_print_error(stderr, result);
        return 1;
    }

    if (verbose) fputs("Input: ", stdout);
    sv_write(stdout, input);
    if (output.len != 0) {
        fputs(" -> ", stdout);
        sv_write(stdout, output);
    }
    fputc('\n', stdout);
    return 0;
}

```

Save the above code as `example.c` in the repository root and run:

```sh
cc -std=c23 -Idist/release/argparse/include example.c \
  dist/release/argparse/lib/libargparse.a -o example
./example -v -o result.txt input.txt
./example --output=result.txt input.txt
./example --help
```

> [!IMPORTANT]
> Initialize destination variables before parsing.
> String values are **borrowed views into `argv`**: no copying occurs, and the argument strings must remain valid while the views are used.
> Parsing does not print or exit; handle the returned status in your application.
