# stringview

A small, allocation-free library for non-owning views over contiguous byte sequences.
Supports read-only and mutable views, slicing, substring search, comparison, prefix and suffix checks, ASCII whitespace trimming, and stream output without requiring null-terminated strings.

## Build

From the repository root:
- release mode: `make stringview`
- debug mode: `make stringview BUILD=debug`
- unit tests: `make test-stringview BUILD=debug`

The packaged archive and headers are in `dist/<build>/stringview/`.

## Example

```c
#include "stringview/stringview.h"

#include <stdio.h>

int main(void) {
    char text[] = "  hello, world!  ";
    MutableStringView line = msv_trim(msv_from_cstr(text));
    size_t separator = msv_find(line, sv_from_cstr(", "));

    if (separator == SV_NPOS) {
        fputs("separator not found\n", stderr);
        return 1;
    }

    MutableStringView greeting = msv_slice(line, 0, separator);
    MutableStringView subject = msv_substr(line, separator + 2, SIZE_MAX);
    subject.data[0] = 'W';

    msv_write(stdout, greeting);
    fputs(", ", stdout);
    msv_write(stdout, subject);
    fputc('\n', stdout);
    return 0;
}
```

Save the above code as `example.c` in the repository root and run:

```sh
cc -std=c23 -Idist/release/stringview/include example.c \
  dist/release/stringview/lib/libstringview.a -o example
./example
```

The program prints `hello, World!`.

> [!IMPORTANT]
> String views do not own, copy, resize, or free their data.
> The underlying buffer must remain valid for the lifetime of every view, and mutable views require writable storage.
> Lengths and indices are measured in bytes, and views are not necessarily null-terminated; use `sv_write` / `msv_write` rather than passing a view directly to APIs that expect a C string.
