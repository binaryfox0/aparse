#!/usr/bin/env python3

import argparse
import sys


class HelpFormatter(
    argparse.ArgumentDefaultsHelpFormatter,
    argparse.RawDescriptionHelpFormatter
):
    pass


def add_common_options(parser):
    """Options shared by multiple commands."""

    parser.add_argument(
        "-v", "--verbose",
        action="count",
        default=0,
        help="Increase verbosity"
    )

    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Suppress non-error output"
    )

    parser.add_argument(
        "--no-color",
        action="store_true",
        help="Disable colored output"
    )


def create_parser():
    parser = argparse.ArgumentParser(
        prog="aparse-demo",
        description="Reference CLI for testing aparse features.",
        formatter_class=HelpFormatter,
        allow_abbrev=False
    )

    parser.add_argument(
        "--version",
        action="version",
        version="aparse-demo 1.0.0"
    )

    # ---------------------------------------------------------
    # Root options
    # ---------------------------------------------------------

    root_options = parser.add_argument_group("Root options")

    root_options.add_argument(
        "-v", "--verbose",
        action="count",
        default=0,
        help="Increase verbosity"
    )

    root_options.add_argument(
        "-q", "--quiet",
        action="store_true",
        help="Suppress output"
    )

    root_options.add_argument(
        "--config",
        type=str,
        metavar="FILE",
        default=None,
        help="Configuration file"
    )

    root_options.add_argument(
        "--color",
        choices=["auto", "always", "never"],
        default="auto",
        metavar="WHEN",
        help="Color output policy"
    )

    # ---------------------------------------------------------
    # Mutually exclusive options
    # ---------------------------------------------------------

    exclusive = root_options.add_mutually_exclusive_group()

    exclusive.add_argument(
        "--debug",
        action="store_true",
        help="Enable debug mode"
    )

    exclusive.add_argument(
        "--release",
        action="store_true",
        help="Enable release mode"
    )

    # ---------------------------------------------------------
    # Subcommands
    # ---------------------------------------------------------

    commands = parser.add_subparsers(
        dest="command",
        metavar="COMMAND",
        required=True
    )

    # =========================================================
    # build
    # =========================================================

    build = commands.add_parser(
        "build",
        help="Build the project",
        description="Build the project using the selected configuration.",
        formatter_class=HelpFormatter,
        allow_abbrev=False
    )

    add_common_options(build)

    build.add_argument(
        "source",
        type=str,
        metavar="SOURCE",
        help="Source directory"
    )

    build.add_argument(
        "-o", "--output",
        type=str,
        metavar="DIR",
        default="build",
        help="Output directory"
    )

    build.add_argument(
        "-j", "--jobs",
        type=int,
        metavar="N",
        default=1,
        help="Number of parallel jobs"
    )

    build.add_argument(
        "--optimization",
        type=int,
        choices=range(0, 4),
        metavar="LEVEL",
        default=2,
        help="Optimization level (0-3)"
    )

    build.add_argument(
        "--fast-math",
        action="store_true",
        help="Enable fast floating-point math"
    )

    build.add_argument(
        "--warnings-as-errors",
        action="store_true",
        help="Treat warnings as errors"
    )

    build.add_argument(
        "--define",
        action="append",
        metavar="KEY=VALUE",
        default=[],
        help="Add a build definition (repeatable)"
    )

    build.add_argument(
        "--include",
        action="append",
        metavar="DIR",
        default=[],
        help="Add an include directory (repeatable)"
    )

    build.add_argument(
        "--feature",
        nargs="+",
        metavar="NAME",
        default=[],
        help="Enable one or more features"
    )

    # =========================================================
    # clean
    # =========================================================

    clean = commands.add_parser(
        "clean",
        help="Clean build artifacts",
        formatter_class=HelpFormatter,
        allow_abbrev=False
    )

    clean.add_argument(
        "--all",
        action="store_true",
        help="Remove all generated files"
    )

    clean.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would be removed"
    )

    # =========================================================
    # network
    # =========================================================

    network = commands.add_parser(
        "network",
        help="Manage network configuration",
        formatter_class=HelpFormatter,
        allow_abbrev=False
    )

    network_commands = network.add_subparsers(
        dest="network_command",
        metavar="COMMAND",
        required=True
    )

    # ---------------------------------------------------------
    # network list
    # ---------------------------------------------------------

    network_list = network_commands.add_parser(
        "list",
        help="List network interfaces",
        formatter_class=HelpFormatter
    )

    network_list.add_argument(
        "-a", "--all",
        action="store_true",
        help="Include inactive interfaces"
    )

    network_list.add_argument(
        "-f", "--format",
        choices=["table", "json", "yaml"],
        default="table",
        metavar="FORMAT",
        help="Output format"
    )

    network_list.add_argument(
        "--limit",
        type=int,
        metavar="N",
        default=0,
        help="Maximum number of results (0 means unlimited)"
    )

    # ---------------------------------------------------------
    # network add
    # ---------------------------------------------------------

    network_add = network_commands.add_parser(
        "add",
        help="Add a network interface",
        formatter_class=HelpFormatter
    )

    network_add.add_argument(
        "name",
        type=str,
        metavar="NAME",
        help="Interface name"
    )

    network_add.add_argument(
        "--address",
        type=str,
        metavar="IP",
        required=True,
        help="IP address"
    )

    network_add.add_argument(
        "--prefix",
        type=int,
        metavar="BITS",
        default=24,
        help="Network prefix length"
    )

    network_add.add_argument(
        "--gateway",
        type=str,
        metavar="IP",
        default=None,
        help="Default gateway"
    )

    network_add.add_argument(
        "--enabled",
        action="store_true",
        help="Enable the interface immediately"
    )

    # ---------------------------------------------------------
    # network remove
    # ---------------------------------------------------------

    network_remove = network_commands.add_parser(
        "remove",
        help="Remove a network interface"
    )

    network_remove.add_argument(
        "name",
        metavar="NAME",
        help="Interface name"
    )

    network_remove.add_argument(
        "-f", "--force",
        action="store_true",
        help="Force removal"
    )

    # =========================================================
    # service
    # =========================================================

    service = commands.add_parser(
        "service",
        help="Manage system services",
        formatter_class=HelpFormatter
    )

    service_commands = service.add_subparsers(
        dest="service_command",
        metavar="COMMAND",
        required=True
    )

    # ---------------------------------------------------------
    # service list
    # ---------------------------------------------------------

    service_list = service_commands.add_parser(
        "list",
        help="List services"
    )

    service_list.add_argument(
        "--running",
        action="store_true",
        help="Show only running services"
    )

    service_list.add_argument(
        "--format",
        choices=["table", "json"],
        default="table",
        metavar="FORMAT",
        help="Output format"
    )

    # ---------------------------------------------------------
    # service start
    # ---------------------------------------------------------

    service_start = service_commands.add_parser(
        "start",
        help="Start a service"
    )

    service_start.add_argument(
        "name",
        metavar="NAME",
        help="Service name"
    )

    service_start.add_argument(
        "--timeout",
        type=float,
        metavar="SECONDS",
        default=30.0,
        help="Startup timeout"
    )

    service_start.add_argument(
        "--wait",
        action="store_true",
        help="Wait until the service is ready"
    )

    # ---------------------------------------------------------
    # service stop
    # ---------------------------------------------------------

    service_stop = service_commands.add_parser(
        "stop",
        help="Stop a service"
    )

    service_stop.add_argument(
        "name",
        metavar="NAME",
        help="Service name"
    )

    service_stop.add_argument(
        "--timeout",
        type=float,
        metavar="SECONDS",
        default=30.0,
        help="Shutdown timeout"
    )

    return parser


def main():
    parser = create_parser()

    args = parser.parse_args()

    print("Parsed arguments:")
    for key, value in vars(args).items():
        print(f"  {key}: {value!r}")


if __name__ == "__main__":
    main()
