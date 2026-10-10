#!/bin/bash
# Installs the live flow field screensaver into Omarchy.
#
#   bash install.sh              compile, install, and run it once in this terminal
#   bash install.sh --uninstall  remove everything it installed
#
# Everything lands under ~/.local and ~/.config, so Omarchy updates leave it
# alone. Log out and back in once so the Omarchy launcher picks it up.

set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
bin="$HOME/.local/bin/omarchy-flowfield"
libdir="$HOME/.local/lib/omarchy-flowfield"
envfile="$HOME/.config/uwsm/env.d/flowfield"

if [[ ${1:-} == "--uninstall" ]]; then
  rm -f "$bin" "$libdir/omarchy-screensaver" "$envfile"
  rmdir "$libdir" 2>/dev/null || true
  echo "Removed. The stock screensaver is back after your next login."
  exit 0
fi

command -v cc >/dev/null || { echo "No C compiler found. On Omarchy: omarchy-pkg-add base-devel" >&2; exit 1; }

mkdir -p "$(dirname "$bin")" "$libdir" "$(dirname "$envfile")"
cc -O2 -o "$bin" "$here/flowfield.c" -lm
echo "built    $bin"

install -m 755 "$here/omarchy-screensaver" "$libdir/omarchy-screensaver"
echo "installed $libdir/omarchy-screensaver"

install -m 644 "$here/uwsm-env" "$envfile"
echo "installed $envfile"

cat <<MSG

Done. The idle screensaver and Super+Esc will use the flow field after your
next login. To see it right now in this terminal (any key exits):

    $bin

Settings go in ~/.config/omarchy-flowfield.conf on one line, for example:
    --fps 15 --density 12 --fade 97
MSG
