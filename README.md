# aparse

**aparse** is a lightweight argument parsing library written in C, designed to provide a flexible and structured way to parse command-line arguments.

> [!WARNING]
> **aparse is under active development, and its API may change at any time without notice.**
>
> If you intend to use it in a project, consider pinning a specific version to avoid unexpected breaking changes. Alternatively, you can vendor the source and header files directly into your project to maintain a stable copy.

## Features

- **Argument parsing** — Support for command-line options and positional arguments.
- **Short and long options** — Define arguments using short (`-v`) and long (`--verbose`) forms.
- **Type conversion** — Parse argument values into their corresponding types.
- **Subcommands** — Organize commands into nested scopes with support for subcommand dispatch.
- **Argument validation** — Detect missing arguments, invalid values, unexpected arguments, and other parsing errors.
- **Special actions** — Support built-in actions such as help and version.
- **Structured results** — Retrieve parsed values, parsing information, and diagnostic information through structured result objects.
- **Customizable diagnostics** — Let applications decide how parsing errors are presented.

## Getting Started

### Usage

A basic example demonstrating how to define arguments, configure a parser, and process command-line input:

**[Basic usage example](./example/example.c)**

### Error Handling

`aparse` distinguishes between library-level errors and user-facing parsing errors.

- `aparse_error_t` represents errors encountered while executing library operations, such as invalid arguments or memory allocation failures.
- `aparse_user_error_t` represents errors caused by user input, such as missing required arguments or invalid option values.

A parsing operation can return `APARSE_ERR_OK` while still reporting a user-facing parsing error through `result->user_error`.

The library also provides diagnostic helpers:

- `aparse_print_error()` — Print a parsing diagnostic to a specified stream.
- `aparse_user_error_string()` — Retrieve a textual description of a user-facing error.

## License

See [LICENSE](./LICENSE) for details.