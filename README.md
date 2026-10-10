# Live Flow Field Screensaver

Generative-art screensavers for [Omarchy](https://omarchy.org), ported from
Processing sketches.

| Directory | What it is |
| --- | --- |
| [`flowfield/`](flowfield/) | A live Perlin-noise flow field in C. Drawn with Braille characters in the terminal Omarchy already uses for its screensaver. About 2% of one CPU core at 1080p. |
| [`braille-art/`](braille-art/) | A Python generator that draws a flow field or a fractal tree as Braille art into the Omarchy screensaver file, with fresh random parameters at every boot and every hour, animated by Omarchy's own ttfx effects. |

## Install

Omarchy ships with everything these need (a C compiler and Python). Clone the
repo, then run the installer for the one you want:

```
git clone https://github.com/murphybradyluke/Live-Flow-Field-Screensaver
cd Live-Flow-Field-Screensaver
```

The live flow field (the main one):

```
cd flowfield
bash install.sh
```

Then log out and back in once. After that the idle screensaver and
System > Screensaver (Super+Esc) run the flow field. To watch it right away in
any terminal, run `~/.local/bin/omarchy-flowfield`; any key exits.

The Braille art generator (a fresh still picture at every boot and every hour,
animated by Omarchy's own text effects):

```
cd braille-art
bash install.sh
```

Each directory has its own README with settings and details. Both install only
under `~/.local` and `~/.config`, so Omarchy updates never touch them, and both
uninstall with `bash install.sh --uninstall`.

## The sketches

The flow field: a few thousand particles each ask a smooth noise function
which way to turn, step one dot, and leave a fading trail. Zoom, turbulence
and colour drift slowly on their own noise so the picture never repeats.

The tree: one rule, draw a branch then draw two smaller branches from its
tip, applied to itself until the twigs are tiny.

Both started life as 25-line Processing sketches. The Processing versions are
in the comments and READMEs for anyone who wants to play with them in the
Processing editor.
