#!/bin/bash
### Build -> flash -> monitor the standalone TFT + touch pin-discovery test.
###
### Usage:
###   ./flash.sh                # build, flash, then open the serial monitor
###   ./flash.sh --no-monitor   # build + flash only
###   SERIAL_PORT=/dev/ttyUSB1 ./flash.sh
###
### Pre-reqs: ESP-IDF v4.4 installed; board attached. Exit the monitor with Ctrl-].
set -euo pipefail

IDF_PATH=${IDF_PATH:-$HOME/esp/esp-idf}
SERIAL_PORT=${SERIAL_PORT:-/dev/ttyUSB0}      # CH340 on this board enumerates as ttyUSB*
DO_MONITOR=1

for arg in "$@"; do
    case "$arg" in
        --no-monitor) DO_MONITOR=0 ;;
        -h|--help)    sed -rn 's/^### ?//;T;p' "$0"; exit 0 ;;
        *) echo "unknown arg: $arg"; exit 2 ;;
    esac
done

if [[ ! -f "${IDF_PATH}/export.sh" ]]; then
    for alt in "$HOME/esp/esp-idf" "$HOME/esp/esp-idf-v4.4.6"; do
        if [[ -f "$alt/export.sh" ]]; then IDF_PATH="$alt"; break; fi
    done
fi
if [[ ! -f "${IDF_PATH}/export.sh" ]]; then
    echo "error: ESP-IDF not found. Set IDF_PATH." >&2; exit 2
fi

IDF_PYTHON_ENV_PATH=${IDF_PYTHON_ENV_PATH:-$HOME/.espressif/python_env/idf4.4_py3.8_env}
if [[ -d "$IDF_PYTHON_ENV_PATH/bin" ]]; then
    export IDF_PYTHON_ENV_PATH
    export PATH="$IDF_PYTHON_ENV_PATH/bin:$PATH"
fi

. "${IDF_PATH}/export.sh" > /dev/null

# export.sh on this install adds the s3 Xtensa toolchain but not the classic
# esp32 one, even though it's installed -- add it back if the compiler is missing.
if ! command -v xtensa-esp32-elf-g++ >/dev/null 2>&1; then
    for tc in "$HOME"/.espressif/tools/xtensa-esp32-elf/*/xtensa-esp32-elf/bin; do
        [[ -d "$tc" ]] && export PATH="$tc:$PATH" && break
    done
fi

idf.py -p "${SERIAL_PORT}" flash -b 1500000

if [[ $DO_MONITOR -eq 1 ]]; then
    idf.py -p "${SERIAL_PORT}" monitor
fi
