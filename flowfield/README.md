# Flow field screensaver for Omarchy

A live Perlin-noise flow field, drawn with Braille characters in the terminal
Omarchy already uses for its screensaver. Particles follow a smooth noise
field that drifts over time, leaving trails that fade. Zoom, turbulence and
colour all wander slowly on their own, so it never repeats and never jumps.

It is one C file. At a 1080p-sized terminal it uses about 2% of one CPU core
at 20 frames per second, and about 5% at 4K.

## Install

    bash install.sh

Then log out and back in once. From then on the idle screensaver and
System > Screensaver (Super+Esc) run the flow field. Any key, mouse movement
or focus change exits it, exactly like the stock one.

To watch it right away in any terminal:

    ~/.local/bin/omarchy-flowfield

`bash install.sh --uninstall` puts everything back.

## How it hooks in

Omarchy's launcher opens a fullscreen terminal per monitor and runs the
command `omarchy-screensaver` inside it. The installer puts a replacement
with that name in `~/.local/lib/omarchy-flowfield/` and adds a one-line file
under `~/.config/uwsm/env.d/` that puts that directory first on the session
PATH. Nothing under `/usr/share/omarchy` is touched, so updates cannot break
it, and if the binary is ever missing the replacement runs the stock
screensaver instead.

## Settings

Create `~/.config/omarchy-flowfield.conf` with one line of options:

    --fps 15 --density 12 --fade 97

| Option | Default | What it does |
| ------ | ------- | ------------ |
| `--fps N` | 20 | frames per second; lower is cheaper |
| `--density N` | 20 | particles per 1000 dots; fewer means sparser strands |
| `--fade N` | 96 | trail persistence from 90 (short) to 99 (very long) |

## Making it yours

`step()` is the Processing `draw()` loop: the three lines at the top are the
drifting knobs, and the loop below moves each particle one dot along the
field. `render()` turns the dot grid into Braille characters and colours.
Rebuild with `bash install.sh` after a change.
