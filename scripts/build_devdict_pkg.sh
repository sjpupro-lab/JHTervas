#!/usr/bin/env bash
# build_devdict_pkg.sh
# Bundles all developer-dictionary files into a standalone zip archive
# that can be dropped into any other repository.
#
# Output: devdict_canvasos_<VERSION>.zip  (in repository root)
#
# Layout inside the zip:
#   devdict_canvasos_<VERSION>/
#   ├── devdict_site/       <- searchable HTML/JS/CSS reference UI
#   ├── ssot/               <- SSOT .def source files
#   ├── tools/              <- gen_devdict.py generator
#   ├── docs/               <- phase-specific HTML dictionaries
#   └── README_DEVDICT.md   <- quick-start instructions

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="$(cat "$ROOT/VERSION" | tr -d '\r\n')"
PKG_NAME="devdict_canvasos_${VERSION}"
PKG_DIR="/tmp/${PKG_NAME}"
ZIP_OUT="$ROOT/${PKG_NAME}.zip"

echo "[devdict-pkg] VERSION=$VERSION"
echo "[devdict-pkg] Building package -> $ZIP_OUT"

# ── Clean previous temp build ──────────────────────────────────────
rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR"

# ── 1. Searchable web UI ───────────────────────────────────────────
if [ -d "$ROOT/devdict_site" ]; then
    cp -r "$ROOT/devdict_site" "$PKG_DIR/devdict_site"
    echo "[devdict-pkg]   + devdict_site/"
else
    echo "[devdict-pkg] WARNING: devdict_site/ not found, skipping"
fi

# ── 2. SSOT definition files ───────────────────────────────────────
mkdir -p "$PKG_DIR/ssot"
for def_file in "$ROOT"/include/*.def; do
    [ -f "$def_file" ] || continue
    cp "$def_file" "$PKG_DIR/ssot/"
    echo "[devdict-pkg]   + ssot/$(basename "$def_file")"
done

# ── 3. Generator tool ──────────────────────────────────────────────
mkdir -p "$PKG_DIR/tools"
if [ -f "$ROOT/tools/gen_devdict.py" ]; then
    cp "$ROOT/tools/gen_devdict.py" "$PKG_DIR/tools/"
    echo "[devdict-pkg]   + tools/gen_devdict.py"
fi

# ── 4. Phase HTML dictionaries ─────────────────────────────────────
mkdir -p "$PKG_DIR/docs"
for html in "$ROOT"/docs/devdict_*.html; do
    [ -f "$html" ] || continue
    cp "$html" "$PKG_DIR/docs/"
    echo "[devdict-pkg]   + docs/$(basename "$html")"
done

# ── 5. Embedded README ─────────────────────────────────────────────
cat > "$PKG_DIR/README_DEVDICT.md" << 'EOF'
# CanvasOS Developer Dictionary Package

This archive contains the standalone developer dictionary for CanvasOS.
It can be used in any repository without the full CanvasOS source tree.

## Contents

| Path | Description |
|------|-------------|
| `devdict_site/` | Searchable HTML/JS/CSS reference UI — open `index.html` in a browser |
| `ssot/` | SSOT `.def` source files (opcodes, regions, bindings) |
| `tools/gen_devdict.py` | Regenerate the JSON data files from updated `.def` sources |
| `docs/` | Phase-specific HTML reference dictionaries |

## Quick Start

1. Open `devdict_site/index.html` in any modern browser — no server required.
2. Use the search box to look up opcodes, regions, and bindings by name or keyword.

## Regenerating JSON Data

If you update the `.def` files in `ssot/`, run:

```bash
python3 tools/gen_devdict.py \
    --opcodes  ssot/canvasos_opcodes.def \
    --regions  ssot/canvasos_regions.def \
    --bindings ssot/canvasos_bindings.def \
    --out      devdict_site/data
```

## Version
EOF
cat "$ROOT/VERSION" >> "$PKG_DIR/README_DEVDICT.md"
echo "[devdict-pkg]   + README_DEVDICT.md"

# ── 6. Create zip ──────────────────────────────────────────────────
rm -f "$ZIP_OUT"
(cd /tmp && zip -qr "$ZIP_OUT" "$PKG_NAME")

echo "[devdict-pkg] OK -> $ZIP_OUT"
echo "[devdict-pkg] Size: $(du -sh "$ZIP_OUT" | cut -f1)"

# ── Clean up temp dir ──────────────────────────────────────────────
rm -rf "$PKG_DIR"
