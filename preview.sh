#!/bin/bash
# Screenshot de uma tela no emulador com dados mock (semana 7).
# usage: preview.sh <screen 0home|1match|2starters|3table> [sel=0] [scroll=0] [out.png]
set -e
SCR=${1:-0}; SEL=${2:-0}; SCROLL=${3:-0}
OUT=$(realpath -m "${4:-preview/screen-$SCR.png}")
SRC=$(cd "$(dirname "$0")" && pwd)
TMP=/tmp/pfantasy-preview
PLAT=emery
rm -rf "$TMP" && mkdir -p "$TMP" && cp -r "$SRC/package.json" "$SRC/wscript" "$SRC/src" "$TMP/"
mkdir -p "$TMP/resources"
# drop PKJS so emulator doesn't XHR
rm -rf "$TMP/src/pkjs"
python3 - "$TMP/package.json" <<'PY'
import json,sys
p=json.load(open(sys.argv[1])); p["dependencies"]={}; p["pebble"]["enableMultiJS"]=False
json.dump(p,open(sys.argv[1],"w"),indent=2)
PY
# inject preview defines
DEFS="#define PREVIEW_MOCK 1\n#define PREVIEW_SCREEN $SCR\n#define PREVIEW_SEL $SEL\n#define PREVIEW_SCROLL $SCROLL\n"
for f in data.c main.c; do
  printf "$DEFS" | cat - "$SRC/src/c/$f" > "$TMP/src/c/$f"
done
cd "$TMP"
pebble build > /tmp/pfantasy-preview-build.log 2>&1 || { grep -E "error:|Error" /tmp/pfantasy-preview-build.log | head -40; exit 1; }
pebble install --emulator $PLAT 2>&1 | tee /tmp/pfantasy-install.log | grep -q "App install succeeded\|install succeeded" || {
  sleep 4
  pebble install --emulator $PLAT
}
sleep 5
mkdir -p "$(dirname "$OUT")"
pebble screenshot --emulator $PLAT --no-open "$OUT" >/dev/null
echo "saved $OUT"
