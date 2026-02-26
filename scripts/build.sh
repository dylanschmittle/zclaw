#!/bin/bash
# Build zclaw firmware
#
# Usage:
#   ./scripts/build.sh                              # Default build (generic_esp32c3)
#   ./scripts/build.sh --board xiao_esp32s3_sense    # Build for XIAO S3 Sense
#   ./scripts/build.sh --board generic_esp32s3       # Build for generic ESP32-S3
#   ./scripts/build.sh --list-boards                 # Show available boards

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

BOARD=""
PAD_TARGET_BYTES=""

usage() {
    cat << USAGE
Usage: $0 [--board BOARD] [--pad-to-888kb] [--list-boards]

Options:
  --board BOARD     Build for a specific board (see boards/ directory)
  --list-boards     List available board configurations
  --pad-to-888kb    Create padded binary to exactly 888 KiB (909312 bytes)
USAGE
}

list_boards() {
    echo "Available boards:"
    echo ""
    for dir in "$PROJECT_DIR"/boards/*/; do
        local name
        name="$(basename "$dir")"
        if [ -f "$dir/board.conf" ]; then
            local board_name target flash
            board_name="$(grep '^BOARD_NAME=' "$dir/board.conf" | head -1 | cut -d'"' -f2)"
            target="$(grep '^IDF_TARGET=' "$dir/board.conf" | head -1 | cut -d= -f2)"
            flash="$(grep '^FLASH_SIZE=' "$dir/board.conf" | head -1 | cut -d= -f2)"
            printf "  %-28s %s (%s, %s)\n" "$name" "$board_name" "$target" "$flash"
        fi
    done
    echo ""
    echo "Usage: $0 --board <name>"
}

while [ $# -gt 0 ]; do
    case "$1" in
        --board)
            BOARD="$2"
            shift 2
            ;;
        --list-boards)
            list_boards
            exit 0
            ;;
        --pad-to-888kb)
            PAD_TARGET_BYTES=$((888 * 1024))
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Error: Unknown argument '$1'"
            usage
            exit 1
            ;;
    esac
done

file_size_bytes() {
    local path="$1"
    if stat -f %z "$path" >/dev/null 2>&1; then
        stat -f %z "$path"
    else
        stat -c %s "$path"
    fi
}

source_idf_env() {
    local candidates=(
        "$HOME/esp/esp-idf/export.sh"
        "$HOME/esp/v5.4/esp-idf/export.sh"
    )
    if [ -n "${IDF_PATH:-}" ]; then
        candidates+=("$IDF_PATH/export.sh")
    fi

    local script
    local found=0
    for script in "${candidates[@]}"; do
        [ -f "$script" ] || continue
        found=1
        if source "$script" > /dev/null 2>&1; then
            return 0
        fi
    done

    if [ "$found" -eq 1 ]; then
        echo "Error: ESP-IDF found but failed to activate."
        echo "Run:"
        echo "  cd ~/esp/esp-idf && ./install.sh esp32c3,esp32s3"
    else
        echo "Error: ESP-IDF not found. Install it first:"
        echo "  mkdir -p ~/esp && cd ~/esp"
        echo "  git clone -b v5.4 --recursive https://github.com/espressif/esp-idf.git"
        echo "  cd esp-idf && ./install.sh esp32c3,esp32s3"
    fi
    return 1
}

source_idf_env || exit 1

# --- Resolve board configuration ---
if [ -n "$BOARD" ]; then
    BOARD_DIR="$PROJECT_DIR/boards/$BOARD"
    if [ ! -f "$BOARD_DIR/board.conf" ]; then
        echo "Error: Board '$BOARD' not found in boards/ directory."
        echo ""
        list_boards
        exit 1
    fi

    # Load board config
    IDF_TARGET="$(grep '^IDF_TARGET=' "$BOARD_DIR/board.conf" | cut -d= -f2)"
    BOARD_NAME="$(grep '^BOARD_NAME=' "$BOARD_DIR/board.conf" | cut -d'"' -f2)"

    BUILD_DIR="build-$BOARD"

    # Build SDKCONFIG_DEFAULTS: base defaults + board overrides
    SDKCONFIG_DEFAULTS="sdkconfig.defaults"
    if [ -f "$BOARD_DIR/sdkconfig.board" ]; then
        SDKCONFIG_DEFAULTS="sdkconfig.defaults;$BOARD_DIR/sdkconfig.board"
    fi

    echo "Building zclaw for: $BOARD_NAME"
    echo "  Target:     $IDF_TARGET"
    echo "  Build dir:  $BUILD_DIR"
    echo "  Config:     $SDKCONFIG_DEFAULTS"
    echo ""

    idf.py \
        -B "$BUILD_DIR" \
        -D IDF_TARGET="$IDF_TARGET" \
        -D SDKCONFIG_DEFAULTS="$SDKCONFIG_DEFAULTS" \
        build
else
    # Default build (generic esp32c3, uses sdkconfig.defaults as-is)
    BUILD_DIR="build"

    echo "Building zclaw (default: esp32c3)..."
    idf.py build
fi

if [ -n "$PAD_TARGET_BYTES" ]; then
    SRC_BIN="$PROJECT_DIR/$BUILD_DIR/zclaw.bin"
    OUT_BIN="$PROJECT_DIR/$BUILD_DIR/zclaw-888kb.bin"

    if [ ! -f "$SRC_BIN" ]; then
        echo "Error: Expected firmware binary not found at $SRC_BIN"
        exit 1
    fi

    cp "$SRC_BIN" "$OUT_BIN"
    CUR_SIZE="$(file_size_bytes "$OUT_BIN")"

    if [ "$CUR_SIZE" -gt "$PAD_TARGET_BYTES" ]; then
        echo "Error: zclaw.bin is ${CUR_SIZE} bytes, larger than 888 KiB target (${PAD_TARGET_BYTES} bytes)."
        exit 1
    fi

    PAD_BYTES=$((PAD_TARGET_BYTES - CUR_SIZE))
    if [ "$PAD_BYTES" -gt 0 ]; then
        dd if=/dev/zero bs=1 count="$PAD_BYTES" 2>/dev/null | LC_ALL=C tr '\000' '\377' >> "$OUT_BIN"
    fi

    FINAL_SIZE="$(file_size_bytes "$OUT_BIN")"
    if [ "$FINAL_SIZE" -ne "$PAD_TARGET_BYTES" ]; then
        echo "Error: Padded binary size mismatch ($FINAL_SIZE bytes)."
        exit 1
    fi

    echo "Padded binary created: $BUILD_DIR/zclaw-888kb.bin ($FINAL_SIZE bytes)"
fi

echo ""
echo "Build complete! To flash: ./scripts/flash.sh"
