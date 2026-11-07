# To do

Where things stand. The last Windows test build was made from commit
`42a40b3`; everything below is in it. Next step: play-testing in a window.

## For review

1. **Right-stick camera**: up/down orbits Chase all the way to straight
   down, also while moving (`overrides/camera_right_stick.c`,
   `docs/camera.md`).
2. **Nav point**: in map mode (D-pad Up), A places a green point; far away
   it sits on the minimap's edge with an arrow; A on it again clears it.
   Clearing it by arriving there not yet tested (`overrides/nav_point.c`).
3. **Map cursor**: the game's own HUD crosshair, read from the ROM at run
   time (Zakuro patch 0024). Seen working in a window on the language page.
4. **Motion controls**: the right stick moves the virtual 3DS at
   observation points and in the wiretap (Zakuro patch 0025). Either axis
   may need flipping.
5. **Bumpers**: LB tap = disguise switch, LB hold = the game's Disguise
   Selector (right stick moves, release picks), RB tap = Scan, RB hold =
   wheel of touch buttons (Zakuro patch 0026). Changes the bumper layout.
6. **Pause menu**: the menu fills the window, Y flips to the progress
   board (L/R turn its areas), Y or B back. Options > Language: D-pad /
   left stick + A pick a flag, "Select (A)" printed (patches 0027-0028).
7. **Boot to gameplay**: save notices (Checking save data, Saving Game,
   ...) fill the window, copyright text hidden, no corner flashes; layouts
   appear about a quarter second later than before (patches 0030-0031).
8. **Draw distance doubled**: far clip, fog and model detail
   (`overrides/draw_distance.c`). Watch for pop-in or slowdown.

## Built, never seen running

9. Mission card on missions after the first ("Donut Disaster").
10. Wheel's look, L3 exits, portraits hidden in map mode, bumper zoom in
    the scanner and binoculars, safe-cracking dial.

## Optional / later

11. "Y Progress" prompt is English only, and also shows on the save screen
    inside pause.
12. Texture replacement / upscaling.
13. Final local Windows build, once the test builds are signed off.

## Dropped

- Unused voice lines.
- LT/RT for driving.
