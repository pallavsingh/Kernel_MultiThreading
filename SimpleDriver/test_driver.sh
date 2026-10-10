#!/bin/bash

set -euo pipefail

DRIVER_NAME="simple_char_driver"
MODULE_FILE="./${DRIVER_NAME}.ko"
DEVICE="/dev/mychardev"
MAJOR=239
MINOR=0

usage() {
    echo "Usage: $0 {load|unload}"
}

check_root() {
    if [[ "$EUID" -ne 0 ]]; then
        echo "Error: Run as root: sudo $0 {load|unload}" >&2
        exit 1
    fi
}

load_driver() {
    echo "=== Loading ${DRIVER_NAME} ==="

    if [[ ! -f "$MODULE_FILE" ]]; then
        echo "Error: Module not found: $MODULE_FILE" >&2
        echo "Build the module first." >&2
        exit 1
    fi

    if ! lsmod | grep -q "^${DRIVER_NAME} "; then
        insmod "$MODULE_FILE"
        echo "Driver loaded successfully."
    else
        echo "Driver is already loaded."
    fi

    echo
    echo "=== Kernel log after loading ==="
    dmesg | tail -n 20

    echo
    echo "=== Creating device node ==="

    if [[ ! -c "$DEVICE" ]]; then
        if [[ -e "$DEVICE" ]]; then
            echo "Error: $DEVICE exists but is not a character device." >&2
            exit 1
        fi

        mknod "$DEVICE" c "$MAJOR" "$MINOR"
        echo "Created $DEVICE"
    else
        echo "$DEVICE already exists."
    fi

    chmod 666 "$DEVICE"

    echo
    echo "=== Testing write ==="
    echo "Hello Kernel!" > "$DEVICE"
    echo "Write test completed."

    echo
    echo "=== Testing read ==="
    timeout 3 cat "$DEVICE" || {
        echo "Read test failed or timed out." >&2
        exit 1
    }

    echo
    echo "=== Kernel log after testing ==="
    dmesg | tail -n 20

    echo
    echo "Driver load and test sequence completed."
}

unload_driver() {
    echo "=== Unloading ${DRIVER_NAME} ==="

    if lsmod | grep -q "^${DRIVER_NAME} "; then
        rmmod "$DRIVER_NAME"
        echo "Driver unloaded successfully."
    else
        echo "Driver is not loaded."
    fi

    if [[ -c "$DEVICE" ]]; then
        rm -f "$DEVICE"
        echo "Removed device node $DEVICE"
    fi

    echo
    echo "=== Kernel log after unloading ==="
    dmesg | tail -n 20
}

main() {
    if [[ $# -ne 1 ]]; then
        usage
        exit 1
    fi

    case "$1" in
        load|unload)
            check_root
            ;;
        *)
            echo "Error: Invalid option '$1'." >&2
            usage
            exit 1
            ;;
    esac

    case "$1" in
        load)   load_driver ;;
        unload) unload_driver ;;
    esac
}

main "$@"
