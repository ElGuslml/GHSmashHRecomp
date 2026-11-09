# NativeXRenderer in Guitar Hero: Smash Hits

The game's side of NativeXRenderer (`native_x_renderer/`, the library that hooks a game's Direct3D calls and draws
on the host). The hooks and the profile are in
[`src/native_x_renderer_ghsmashhits.cpp`](../src/native_x_renderer_ghsmashhits.cpp); the library's checklist is
`native_x_renderer/docs/integrating-a-game.md`.

**Status (2026-11-05):** GH Metallica's side, translated. All these games link the same XDK Direct3D library (2.0.7978)
and the same engine, so GH Metallica's map (GHMetalRecomp's `docs/native-x-renderer-ghmetallica.md`: the device layout,
Dante's Inferno's up to the register shadow; the current objects POTCAWE's + 144; the resource objects POTCAWE's) holds
here; only the addresses differ. `--native_x_renderer=false` or the launcher's Xenos card draws with the Xenos path.

## The addresses

- Each of GH Metallica's 36 hooked functions was found here by a function matcher over both executables' instruction
  indexes (`ppc_index.py`), and every one has the same instructions.
- The library is at `0x821C02E8`-`0x821DE5D0`, 0xEE0 further than in GH Metallica, the draws inside it
  (`0x821C1E78`-`0x821C3158`).
- The device pointer is at `0x82AA1304` (Metallica `0x82AA1574`): the renderer's block moved by `-0x270` with the rest of
  `.data`. 1579 instructions build it in each game, and the 400 compared are the same ones.
- `scene_resolve_caller = upscale_resolve_caller = 0x8251FE2C` (Metallica `0x8251F04C`): the return address of the
  resolve of the 1280x720 scene that the post-processing reads once a frame.
- As in GH Metallica, DrawIndexedVerticesUP has no call id of its own: the profile names the inline draw end
  (`inline_draw_end = 13444`) and EndVertices is not hooked.

## Checked

2026-11-05, `--native_x_renderer=true`, a 1720x720 window, 120 FPS, ultrawide, the observer on
(`--native_x_renderer_verify=true`): the main menu 120 of 120 draw calls match the command processor's state, a song
387 of 387; no render state, fetch or constant mismatch. In a song 540 to 750 draws a frame, none failed; 8.33 ms frames
(8.85 ms the longest in 5 s), 1.52 cores; no placeholder texture. The stage widened, the highway and the HUD at 16:9.

## The bloom's reduced copies (D127)

GH Metallica's post-processing, the same here: the scene resolved to 640x360, then 320x180 targets for the glow and
bright passes and the bloom's and depth of field's 16-tap blurs, whose tap offsets are computed for 320x180 texels. At a
render scale above 1x those offsets skip texels, and GH Metallica's bloom showed a lattice of dots (its
`docs/bloom-lattice.md`). The profile sets `unscaled_screen_fractions` (2026-11-08): the surfaces of the output divided
by a whole number stay at the game's resolution, as on the console. Checked: at 2560x1440 the log names the 320x180
targets as reduced copies drawn at 320x180, no error; the glow not yet looked at closely in a song.
`--native_x_renderer_unscaled_screen_fractions=false` scales them again.

## Not checked

The note highway's two shaders named in `AdjustHighwayMotion` (frame generation's motion, D124, hidden in the launcher)
have GH Metallica's hashes; whether this game's are the same was not looked at. If they differ the board's motion is
simply not adjusted.
