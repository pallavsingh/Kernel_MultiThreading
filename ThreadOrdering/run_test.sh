#!/bin/bash

set -euo pipefail

DRIVER_NAME="kthread_driver"
MODULE_FILE="./kthread_driver.ko"
SOURCE_FILE="./kthread_driver.c"

CLASS_NAME="kthreadclass"
DEVICE_NAME="kthread_dev"

DEVICE="/dev/${DEVICE_NAME}"
CLASS_DEVICE="/sys/class/${CLASS_NAME}/${DEVICE_NAME}"
LOG_FILE="/tmp/${DRIVER_NAME}_test.log"

usage() {
    echo "Usage: sudo bash $0 {load|test|unload|reload}"
    exit 1
}

require_root() {
    if [[ "$(id -u)" -ne 0 ]]; then
        echo "Error: Run this script as root."
        echo "Example: sudo bash $0 load"
        exit 1
    fi
}

is_loaded() {
    grep -q "^${DRIVER_NAME} " /proc/modules
}

build_driver() {
    echo "Building kernel module..."

    if [[ ! -f "$SOURCE_FILE" ]]; then
        echo "Error: Source file not found: $SOURCE_FILE"
        exit 1
    fi

    make

    if [[ ! -f "$MODULE_FILE" ]]; then
        echo "Error: Module file not found: $MODULE_FILE"
        exit 1
    fi

    echo "Build completed successfully."
}

load_driver() {
    if is_loaded; then
        echo "Driver is already loaded."
    else
        build_driver

        echo "Loading ${MODULE_FILE}..."
        insmod "$MODULE_FILE"
        sleep 1

        if ! is_loaded; then
            echo "Error: Driver failed to load."
            dmesg | tail -n 30
            exit 1
        fi

        echo "Driver loaded successfully."
    fi

    if [[ ! -e "$CLASS_DEVICE/dev" ]]; then
        echo "Error: Device was not created: $CLASS_DEVICE"
        echo "Check CLASS_NAME and DEVICE_NAME in ${SOURCE_FILE}."
        dmesg | tail -n 30
        exit 1
    fi

    IFS=: read -r MAJOR MINOR < "$CLASS_DEVICE/dev"

    if [[ ! -c "$DEVICE" ]]; then
        if [[ -e "$DEVICE" ]]; then
            echo "Error: $DEVICE exists but is not a character device."
            exit 1
        fi

        mknod "$DEVICE" c "$MAJOR" "$MINOR"
    fi

    chmod 666 "$DEVICE"

    echo "Device ready: $DEVICE"
    echo "Major: $MAJOR, Minor: $MINOR"
}

test_driver() {
    if ! is_loaded; then
        echo "Error: Driver is not loaded. Run '$0 load' first."
        exit 1
    fi

    if [[ ! -c "$DEVICE" ]]; then
        echo "Error: Character device not found: $DEVICE"
        echo "Run '$0 load' first."
        exit 1
    fi

    echo
    echo "Testing kernel thread driver..."
    echo "Reading messages from $DEVICE"
    echo "----------------------------------------"

    # Give the kernel threads time to generate messages.
    sleep 1

    if cat "$DEVICE" | tee "$LOG_FILE"; then
        :
    else
        echo "Warning: Reading from the device returned an error."
        exit 1
    fi

    echo "----------------------------------------"

    if [[ -s "$LOG_FILE" ]]; then
        echo "PASS: Received data from the kernel driver."
    else
        echo "FAIL: No data received from the kernel driver."
        exit 1
    fi

    echo
    echo "Recent kernel messages:"
    dmesg | tail -n 20
}

unload_driver() {
    if is_loaded; then
        echo "Unloading ${DRIVER_NAME}..."
        rmmod "$DRIVER_NAME"
        echo "Driver unloaded successfully."
    else
        echo "Driver is not loaded."
    fi

    if [[ -e "$DEVICE" ]]; then
        rm -f "$DEVICE"
        echo "Removed device node: $DEVICE"
    fi
}

require_root

case "${1:-}" in
    load)
        load_driver
        ;;
    test)
        test_driver
        ;;
    unload)
        unload_driver
        ;;
    reload)
        unload_driver
        load_driver
        ;;
    *)
        usage
        ;;
esac
