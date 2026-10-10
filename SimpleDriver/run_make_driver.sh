#!/bin/bash

set -euo pipefail

trap cleanup HUP INT QUIT ABRT

cleanup() {
    echo "Caught signal. Exiting script." >&2
    exit 1
}

usage() {
    echo "Usage: $0 {build|clean}"
}

main() {
    if [[ $# -ne 1 ]]; then
        usage
        exit 1
    fi

    local action="$1"
    local driver_dir
    local kernel_dir

    # Resolve the directory containing this script
    driver_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
    kernel_dir="/lib/modules/$(uname -r)/build"

    if [[ ! -d "$kernel_dir" ]]; then
        echo "Error: Kernel build directory not found: $kernel_dir" >&2
        exit 1
    fi

    case "$action" in
        build)
            echo "Building kernel module..."
            make -C "$kernel_dir" M="$driver_dir" modules
            echo "Build completed successfully."
            ;;

        clean)
            echo "Cleaning kernel module build artifacts..."
            make -C "$kernel_dir" M="$driver_dir" clean
            echo "Clean completed successfully."
            ;;

        *)
            echo "Error: Invalid argument '$action'." >&2
            usage
            exit 1
            ;;
    esac
}

main "$@"
