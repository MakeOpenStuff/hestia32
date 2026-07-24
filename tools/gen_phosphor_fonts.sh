#!/usr/bin/env bash
# gen_phosphor_fonts.sh — Download Phosphor Regular TTF and generate LVGL bitmap fonts
#
# Generates:
#   src/core/ui/fonts/lv_font_hestia_icons_48.c  (settings menu tiles)
#   src/core/ui/fonts/lv_font_hestia_icons_28.c  (sidebar icons, settings headers)
#   src/core/ui/fonts/lv_font_hestia_icons_20.c  (inline icons, badges)
#
# After running this script:
#   1. Edit src/core/ui/ui_icons.h and set  HESTIA_ICONS_CUSTOM 1
#   2. Rebuild:  source ~/esp/esp-idf-v5.5/export.sh && idf.py build
#
# Requirements: node.js >= 14, npm, curl

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
FONTS_OUT="$REPO_ROOT/src/core/ui/fonts"
TTF_URL="https://github.com/phosphor-icons/web/raw/master/src/regular/Phosphor.ttf"
TTF_FILE="$SCRIPT_DIR/Phosphor.ttf"

echo "=== Hestia32 Phosphor font generator ==="

# --- Check / install lv_font_conv ---
if ! command -v lv_font_conv &>/dev/null; then
    echo "Installing lv_font_conv..."
    npm install -g lv_font_conv
fi

# --- Download Phosphor.ttf ---
if [ ! -f "$TTF_FILE" ]; then
    echo "Downloading $TTF_URL ..."
    curl -L -o "$TTF_FILE" "$TTF_URL"
else
    echo "Using cached $TTF_FILE"
fi

mkdir -p "$FONTS_OUT"

# Glyphs needed (Phosphor Regular Unicode PUA codepoints):
#   thermometer-hot  E5CA   snowflake       E5AA   fan            E9F2
#   drop             E210   bathtub         E81E   gear           E270
#   arrow-left       E058   arrow-right     E05A   wifi-high      E4EA
#   sun              E472   clock           E19A   sliders-horiz  E434
#   palette          E6C8   cpu             E610   plug           E946
#   check-circle     E184   x-circle        E4F8   warning-circle E4E2
#   rocket-launch    E3FE   leaf            E2DA   arrows-cw      E094
#   broadcast        E0F2   cell-signal-high E144  arrows-lr      E086
#   globe            E288   wrench          E5D4

GLYPHS="0xE058,0xE05A,0xE086,0xE094,0xE144,0xE184,0xE19A,0xE210,0xE270,0xE288,0xE2DA,\
0xE3FE,0xE434,0xE472,0xE4E2,0xE4EA,0xE4F8,0xE5AA,0xE5CA,\
0xE5D4,0xE610,0xE6C8,0xE81E,0xE946,0xE9F2,0xE0F2"

echo "Generating 48px font..."
lv_font_conv \
    --font "$TTF_FILE" \
    --size 48 \
    --bpp 4 \
    --format lvgl \
    --output "$FONTS_OUT/lv_font_hestia_icons_48.c" \
    -r "$GLYPHS"

echo "Generating 28px font..."
lv_font_conv \
    --font "$TTF_FILE" \
    --size 28 \
    --bpp 4 \
    --format lvgl \
    --output "$FONTS_OUT/lv_font_hestia_icons_28.c" \
    -r "$GLYPHS"

echo "Generating 20px font..."
lv_font_conv \
    --font "$TTF_FILE" \
    --size 20 \
    --bpp 4 \
    --format lvgl \
    --output "$FONTS_OUT/lv_font_hestia_icons_20.c" \
    -r "$GLYPHS"

echo ""
echo "Done! Font files written to src/core/ui/fonts/"
echo ""
echo "Next steps:"
echo "  1. In src/core/ui/ui_icons.h  →  set  HESTIA_ICONS_CUSTOM 1"
echo "  2. In src/CMakeLists.txt      →  uncomment the font SRCS block"
echo "  3. source ~/esp/esp-idf-v5.5/export.sh && idf.py build"
