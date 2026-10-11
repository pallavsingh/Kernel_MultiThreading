#!/bin/bash

set -euo pipefail

cleanup() {
    echo "Caught signal. Cleaning up..."
    echo "Exiting script."
    exit 1
}

trap cleanup HUP INT QUIT ABRT

usage() {
    echo "Usage: $0 {build|clean}"
}

main() {
    if [[ $# -ne 1 ]]; then
        usage
        exit 1
    fi

    local action="$1"
    local build_dir="build"

    case "$action" in
        build)
            echo "Building simple_char_driver kernel module..."

            mkdir -p "$build_dir"
            cmake -S . -B "$build_dir"
            cmake --build "$build_dir" --target build_module

            echo "Build completed successfully."
            ;;

        clean)
            echo "Cleaning simple_char_driver build artifacts..."

            if [[ -d "$build_dir" ]]; then
                cmake -S . -B "$build_dir"
                cmake --build "$build_dir" --target clean_module
            else
                echo "Build directory does not exist. Nothing to clean."
            fi

            echo "Clean completed."
            ;;

        *)
            echo "Error: Invalid argument '$action'." >&2
            usage
            exit 1
            ;;
    esac
}

main "$@" 
