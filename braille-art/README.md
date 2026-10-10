# Generative sketches as the Omarchy screensaver

Omarchy's screensaver is a fullscreen terminal that runs random text effects
over whatever is in `~/.config/omarchy/branding/screensaver.txt`. This package
draws a Perlin-noise flow field or a fractal tree into that file as Braille
art, with fresh random parameters every time, so the screensaver changes by
itself: a new picture at every boot and every hour.

## Install

```
bash install.sh
```

Everything lands under `~/.local` and `~/.config`, so Omarchy updates never
touch it. `bash install.sh --uninstall` removes it all.

## Use

```
omarchy-sketch-screensaver            # random sketch, written to the screensaver file
omarchy-sketch-screensaver tree --show   # a tree, and start the screensaver to look at it
omarchy-sketch-screensaver field --preview   # print to the terminal, change nothing
omarchy-sketch-screensaver --seed 42  # reproduce a picture you liked
omarchy-sketch-screensaver --width 160 --height 44   # bigger art for a large display
```

The menu gets a row at Style > Screensaver > New Sketch that does the same as
the first line and shows the result. Style > Screensaver > Restore Default
puts the Omarchy logo back at any time.

## Making it yours

The two sketches live in `draw_field()` and `draw_tree()` in the script, and
they read exactly like the Processing versions. The random ranges at the top
of each function are the knobs: zoom and turns for the field, spread, ratio
and wind for the tree. Add a third sketch by writing another `draw_*`
function and listing it in `SKETCHES`.
