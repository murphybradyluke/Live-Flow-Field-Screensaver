#!/bin/bash
# Installs omarchy-sketch-screensaver into an Omarchy system.
#
#   1. puts the generator in ~/.local/bin
#   2. draws a fresh sketch now and shows the screensaver
#   3. adds a post-boot hook so every boot gets a new picture
#   4. adds a user timer so a new picture is drawn every hour
#   5. adds "Style > Screensaver > New Sketch" to the Omarchy menu
#
# Run it from the folder that holds omarchy-sketch-screensaver:
#   bash install.sh
# Everything it touches lives under ~/.config, ~/.local, so nothing is lost on
# an Omarchy update. Remove with: bash install.sh --uninstall

set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
bin="$HOME/.local/bin/omarchy-sketch-screensaver"
hook="$HOME/.config/omarchy/hooks/post-boot.d/sketch-screensaver"
unit_dir="$HOME/.config/systemd/user"
menu="$HOME/.config/omarchy/extensions/omarchy-menu.jsonc"
menu_key='"style.screensaver.sketch"'

if [[ ${1:-} == "--uninstall" ]]; then
  systemctl --user disable --now sketch-screensaver.timer 2>/dev/null || true
  rm -f "$bin" "$hook" "$unit_dir/sketch-screensaver.service" "$unit_dir/sketch-screensaver.timer"
  [[ -f $menu ]] && sed -i "/$menu_key/d" "$menu"
  echo "Removed. Restore the Omarchy logo with: omarchy branding screensaver reset"
  exit 0
fi

# 1. the generator
mkdir -p "$(dirname "$bin")"
install -m 755 "$here/omarchy-sketch-screensaver" "$bin"
echo "installed $bin"

# 3. new picture at every boot
mkdir -p "$(dirname "$hook")"
cat >"$hook" <<'HOOK'
#!/bin/bash
# Draw a fresh generative sketch for the screensaver at boot.
"$HOME/.local/bin/omarchy-sketch-screensaver" random >/dev/null 2>&1 || true
HOOK
chmod +x "$hook"
echo "installed post-boot hook"

# 4. and every hour
mkdir -p "$unit_dir"
cat >"$unit_dir/sketch-screensaver.service" <<'UNIT'
[Unit]
Description=Draw a new generative sketch for the Omarchy screensaver

[Service]
Type=oneshot
ExecStart=%h/.local/bin/omarchy-sketch-screensaver random
UNIT
cat >"$unit_dir/sketch-screensaver.timer" <<'UNIT'
[Unit]
Description=Redraw the Omarchy screensaver sketch every hour

[Timer]
OnCalendar=hourly
Persistent=true

[Install]
WantedBy=timers.target
UNIT
if systemctl --user daemon-reload 2>/dev/null && systemctl --user enable --now sketch-screensaver.timer 2>/dev/null; then
  echo "enabled hourly timer"
else
  echo "could not enable the hourly timer (no user systemd session?); the boot hook still works"
fi

# 5. menu entry: Style > Screensaver > New Sketch
if [[ -f $menu ]] && ! grep -q "$menu_key" "$menu"; then
  # Insert the row just before the file's closing brace.
  line=$(grep -n '^}' "$menu" | tail -1 | cut -d: -f1)
  if [[ -n $line ]]; then
    sed -i "${line}i\\  $menu_key: {\"icon\":\"\",\"label\":\"New Sketch\",\"action\":\"$bin random --show\"}," "$menu"
    echo "added Style > Screensaver > New Sketch to the Omarchy menu"
  fi
fi

# 2. draw one now
if [[ -t 1 ]]; then
  "$bin" random --show
else
  "$bin" random
fi
