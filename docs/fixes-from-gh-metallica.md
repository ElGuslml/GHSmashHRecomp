# The fixes ported from GH Metallica

**Status: ported on 2026-11-05; played with an Xbox 360 guitar (2026-11-09: songs at 160 FPS, NativeXRenderer, ultrawide; the
languages, Low latency audio and the installation from an ISO tried too). Drums and singing not tried yet.** Guitar Hero: Smash Hits is Guitar Hero:
Metallica's engine (the executable `gh4_2_Final.exe`, between Metallica's `gh4_1_burn_Final.exe` and Van Halen's
`gh4_3_Final.exe`; all XDK 2.0.7978), by another studio: 99.8 % of its functions have the same instructions as in GH
Metallica (98.9 % between GH Metallica and GH Van Halen), and 97 % of its scripts exist there, 91 % of those
byte for byte. Its first boot had GH Metallica's problems (4.85 cores busy at the title, among others).

GH Metallica's fixes, through GH Van Halen's sources, were copied with their names changed and every address
translated: the function matcher over the executables' instruction indexes found each hooked function, and each of the
47 hooks was checked against GH Metallica's: the same instruction with the same six around it on each side, but for the
data they name (two of them with a different structure offset in that instruction: the place, not the registers it
reads). Data moved by `-0x270` in `.data` and about `-0x290` in `.rdata`.

**Two things differ from GH Metallica and GH Van Halen:** the crowd's claps need no fix here (below), and the game's name
depends on its territory (below, "The game's name").

Each section says what the fix does and how to turn it off; the investigations (the measurements, the evidence) are in
GHMetalRecomp's `docs/` and `CLAUDE.md`, and what is said to be measured was measured there unless it says "here".

## The threads' waits (CPU)

**Symptom:** about five cores at 100 % at the title (4.85 measured here). **Cause:** the game's threads wait by checking
a condition again and again, which costs nothing on the console: the job system's three workers (an empty queue,
`sub_824268D8`), the main thread at the start of each frame (`sub_822E5498`, `sub_822E1B20`), the render thread waiting
for the vertical blank (`sub_8241D640`), the main thread waiting for a group of jobs (`sub_822BBBD0`), and the D3D
library's waits for the GPU (`sub_821C6928`). **Fix:** `src/thread_waits.cpp`, seven hooks: a wait spins briefly, then
sleeps; the workers block on the job event the game pulses, the render thread sleeps until shortly before the vertical
blank it waits for. Measured here: the title 0.42 cores, the menus 0.24, a song 0.88 at 60 FPS. **Off:** `ghsmashhits_wait_smart =
false` (slices only), `ghsmashhits_wait_spin_us` / `ghsmashhits_wait_sleep_us`.

The generated code also keeps the registers in local variables (the codegen's `*_as_local` options, as GH Metallica:
setjmp `sub_82530470` and longjmp `sub_8252FF30`, used only by libjpeg and libpng, share their registers;
non-volatile vector registers stay in the context because this compiler passes values in them).

## The audio drifts out of sync during a song

**Cause:** the engine's timer divides the timebase by its frequency, replacing 50,000,000 by 49,875,000 (the old
consoles' real timebase); ReXGlue's timebase runs at 50 MHz, so the game's clock ran 0.25 % fast and the audio fell
behind the notes. **Fix:** `src/timer.cpp`, a hook in `sub_822D1ED0` (0x822D1F70) keeps the reported frequency.
**Off:** `ghsmashhits_timer_fix = false`. `ghsmashhits.toml` also sets `audio_maxqframes = 4`, the SDK's shortest output
queue (21 ms less audio delay).

**After a stutter** (GH Metallica's of 2026-10-30, its `docs/song-stutters.md`): the notes follow real time, the music
the frames of sound played, so a thread held up on the PC left the music behind the notes until a pause (in GH
Metallica, FMOD's mixer 150 ms late every 5 s: 1.5 s behind after a minute). Three fixes, measured there with stalls
forced on purpose:
- `src/fmod_mix.cpp`: FMOD's polled output (`sub_822814F8`) is updated on the SDK's audio worker right after each frame
  the game's render client (`sub_82229850`) reads (the same code as GH Metallica's, 0xEF8 further), so a late FMOD
  thread loses no block (+1495 ms -> +1.2 ms). **Off:** `ghsmashhits_fmod_on_audio_thread = false`.
- `audio_keep_time = true` (SDK change 62): silence played for a late frame is made up by dropping as many of the next
  ones, a short skip in sync (the audio worker 30 ms late every second: +819 ms -> +0.6 ms). **Off:** `false`.
- `audio_thread_priority = true` (SDK change 63): the audio threads in Windows' "Pro Audio" class. **Off:** `false`.

**Crackling, or the sound muting with low latency audio, on slower PCs** (GH Metallica's of 2026-11-04, its
`docs/audio-crackling.md`; a GH Metallica player's report): the audio calls were spaced at least 3 ms apart
(`audio_callback_min_interval_us = 3000`, below), so a frame of sound (5.3 ms) took its work plus 3 ms, and past about
2.2 ms of work the sound never kept up: hundreds of short gaps a second on the normal output, and on the low latency
one the catch-up dropping the frame the device waited for, a silence that fed itself. Now
`audio_callback_min_interval_us = 0` (FMOD mixes on the audio worker, so the spacing protects nothing; a
`ghsmashhits.toml` that still has `3000` is changed once at startup, a value chosen by hand stays), and SDK change 69:
the catch-up never drops a frame the device is about to take. Measured in GH Metallica with its audio thread made
slower on purpose: 3401 gaps in 40 s before, none after; a 150 ms hold-up costs 138 ms of sound once (400 ms and more
gaps before).

## Low latency audio

**Off by default** (GH Metallica's of 2026-10-30, its `docs/audio-latency.md`; SDK change 64): GH Smash Hits Launcher's
Settings → Game → Low latency audio (`audio_wasapi_exclusive`) takes the output device in exclusive mode (WASAPI),
past Windows' audio engine, with 10 ms kept in the device's buffer (`audio_wasapi_exclusive_buffer_ms`) and 2 of the
game's frames queued instead of 4 (`audio_wasapi_exclusive_queue`). Measured in GH Metallica on a Focusrite Scarlett
2i2, from the game's frame to the driver playing it: 77 ms shared, 20 ms exclusive. Other programs cannot play on that
device while the game runs; when it is busy or refuses, the shared output is used (the log says which). Calibrate the
audio again after turning it on or off. Tried in this game by Gustavo (2026-11-09).

## V-Sync

**On by default** (GH Metallica's of 2026-10-29 and 30, its `docs/vsync.md`): GH Smash Hits Launcher's Settings →
Graphics → V-Sync writes NativeXRenderer's `native_x_renderer_present_mode` (`"vsync"`: FIFO, D106/D107; `"immediate"`
for a G-Sync or FreeSync display) and the SDK's `vblank_follows_display` (change 61): the game's vertical blank, its
clock for frames, follows the monitor's where the frame rate is the refresh rate or a whole fraction of it, so the
game never runs ahead of the display. Measured in GH Metallica at 160 FPS on a 160 Hz display: 4 ms from present to
display, no frame dropped (NVIDIA's MAILBOX queued three frames: 16-19 ms); at 60 FPS on a 60 Hz display a steady
16.5 ms, presents 99.8 % regular. Calibrate the video again after changing it.

**With G-Sync or FreeSync** (GH Metallica's of 2026-11-04, its `docs/vsync.md`; Gustavo's frame time graph showed a
saw tooth for a few seconds, then even, over and over, at 120 Hz on a 4K TV and at 160 on a monitor): the SDK keeps its
own timer for a variable refresh rate display (its vertical blanks follow the game's frames), and that timer ticked 6
to 10 ms apart at 120 Hz (a millisecond's sleep at a time) at the panel's maximum, so frames now and then waited in the
swap chain's queue. SDK change 70: a high resolution timer and `vblank_timer_spin_us = 700` (ticks 8.33-8.38 ms apart),
and on such a display the timer 0.25 % under its exact refresh rate (119.70 at 120); the render thread wakes on events
the game's vertical blank and swap callbacks set (`src/thread_waits.cpp`, hooks at 0x822D1BCC in `sub_822D1B98` and
0x822D1C1C in `sub_822D1BE0`), within microseconds of the tick. With V-Sync off, the game runs 0.25 % under the
display's exact rate when the frame rate chosen is that rate (`src/options.cpp`; at the round 160 a G-Sync monitor at
159.962 Hz tore). Gustavo, in GH Metallica: the saw tooth gone. **Off:** `vblank_timer_spin_us = 0` (the spin).

**Alt+Tab from fullscreen** closed GH Metallica (NativeXRenderer D125): minimized, the renderer kept drawing frames but
ended none, and its memory for uploads ran out within two seconds in a song. Those frames are ended now.

## Noise instead of music, and the songs' tracks

The game's audio library (FMOD) mixes when its read position moves to the next block of a short ring, and the songs are
several stereo streams interleaved packet by packet in one XMA file. Both are SDK options set in `ghsmashhits.toml`:
`audio_callback_min_interval_us` (3000 until 2026-11-04: the audio output called at the console's pace, without which
FMOD's own thread missed blocks and the menu music was noise; 0 since FMOD mixes on the audio worker, above) and the SDK's XMA decoder carrying each packet's skip count into the next buffer (always on; without it only the
guitar and the kick drum are heard).

## The guitar and the keyboard

- The game tells controllers apart by their XInput subtype: `gamepad_subtype = 6` reports a gamepad as a guitar
  (with a gamepad's subtype the game asks for a microphone).
- A guitar's whammy bar reads 0 until it is first moved, which the game takes as 43 % pressed:
  `guitar_whammy_rest = true` reports it at rest until then.
- The keyboard as a guitar (`mnk_mode`): frets D F J K L, strum the arrow keys, Star Power Space; GH Smash Hits
  Launcher's Keyboard page changes them. Its switch to strum on each fret (`mnk_tap_target = "dpad_down"`) is active
  only in a song (`src/keyboard.cpp`, a hook at `sub_82137B50`).
- A controller playing guitar (GH Metallica's of 2026-10-28, SDK change 57): its frets (A, B, Y, X, LB) can strum as
  they are pressed too, with the same switch on each controller's page of the launcher's Controls tab
  (`gamepad_tap_target`; `gamepad_tap_sources` in `ghsmashhits.toml`), under the same gate. Its whammy bar, the right
  stick, acted 43 % pressed on every sustain (a centred stick is 0, and the game's bar rests at -0.76 of the axis):
  `gamepad_whammy_rest = true` makes the bar at rest while the stick is centred, and fully pressed with the stick
  pushed to either side; the keyboard's Right arrow the same. **Off:** the switch, or `gamepad_tap_target = ""`;
  `gamepad_whammy_rest = false`.
- Each slot's kind of controller follows the one played (GH Metallica's of 2026-11-03, its `docs/controller-kind.md`, a
  player's report: the keyboard playing guitar while the first controller was set to sing made the keyboard's player
  the singer). The game asks a port for its kind only when its controller connects, and the keyboard shares slot 0
  with the first controller: when the SDK (change 66) finds the slot played on another kind, a hook at the entry of
  the port's update (`sub_8239EAC8`) has the game ask again, outside a song (`src/players.cpp`). **Off:**
  `ghsmashhits_kind_follows_controller = false`.
- Shift, Ctrl and Alt can be keys of their own on the keyboard's controls, each side separate (Left Alt as Star Power;
  SDK change 67), and F1-F12 too: the SDK's developer keys are off (`debug_hotkeys = false`, SDK change 68; the
  launcher's Settings > Game > Debug keys turns them on), and a key on a control plays only that control (GH
  Metallica's of 2026-11-03, two players' requests; its `docs/keyboard-keys.md`).
- Third-party guitars (GH Metallica's of 2026-10-29, reported with CRKD guitars; SDK changes 59 and 60, RecompLauncher
  3.4): each controller keeps its own assignments, by its serial number or GUID (`gamepad_device_remap`; a guitar and
  an Xbox 360 controller shared one layout); while the launcher waits for a button a trigger counts only once pressed
  past 90 % from released (a guitar's accelerometer is on its triggers, at half when still); a gamepad whose right
  stick's X rests at -32768 is a whammy bar, not a stick pushed all the way; and the Controls page's Whammy bar row
  shows how far the game reads the bar, with a calibration (push the bar all the way, hold, let go:
  `gamepad_whammy_calibration`). **Off:** Reset on the row.

## A second controller

Start on a controller other than the first left an empty screen: the game asks that controller to sign in
(`XamShowSigninUI`) and waited for it forever. `signin_ui_cancel = true` answers as a cancel does on the console: the
game shows its "not signed in" message, and Continue without saving goes on.

Since 2026-10-28 (GH Metallica's of 2026-10-26) the profile follows the controller that starts the game, as on the
console: while the title screen is shown (`src/players.cpp`, from the quit prompt's frame hook that finds the title's
text), the SDK swaps the controller that presses a button with the first one, so it plays with the profile and its
saves (the log: "The controller in slot 1 pressed first: it is now slot 0, the profile's"). The other controllers are
the profile's guests (`signin_guests = true`, "Name (1)"): signed in, with the game's default settings and no saves.
**Off:** `--ghsmashhits_profile_follows_press=false` (the first controller connected plays with the profile),
`signin_guests = false` (the "not signed in" message again).

## Frame rates above 60

**Experimental.** `ghsmashhits_frame_rate` (60, 120, 144, 160, 165, 240; the launcher's frame rate row) sets the
refresh rate the game paces itself by; its clock is real time, so the songs keep their speed. What counted frames
assuming 60 a second is corrected by `src/framerate.cpp` (`ghsmashhits_frame_rate_fix`, on): the vblank clocks
(`sub_824B2AD8`, `sub_822D1CF8`), the scripts' `Wait N gameframes` (`sub_8242AE18`, `sub_8242AE68`, `sub_8242B168`), the
button presses the scripts poll every few frames (`sub_8239C5E8`, the script function ControllerMake `sub_823B7D50`),
the whammy on a sustain (`sub_821431A0`, `sub_82146020`, `sub_82146200`: 5 frames become 5/60 s) and the loading
screens' spinning icons (`sub_824C3B68`, `sub_823F94C0`, `sub_824C3EE0`). Star power's drain (script `0x74B936AB`)
is not held to 60 passes a second, as it takes the frame's time off the meter each frame: held, star power lasted
160 / 60 times as long at 160 FPS (Gustavo, 2026-10-27). Nor is the band's strings' placing (script `0x827BA6B0`,
`BandManager_PrepareBandForRender` then `Wait 1 gameframe`; GH Metallica's of 2026-11-04): held, the strings were
placed every second or third frame at 120 or 160 FPS and stayed where the guitar had been in between, shaking around
a moving guitar. Placed every frame, their vibration (62 Hz, which 60 FPS shows as a 2 Hz wobble) is kept to that
wobble and its fade to 0.99 per 60th of a second (hooks at 0x82121E58 in `sub_82121DD8` and 0x8211CCAC in
`sub_8211CC70`). Havok's world, stepped by a fixed 1/60 s each frame (`sub_823B8788`), is stepped by the frame's own
time above 60 (hook at 0x823B87D4). Nor is `button_checker` (script `0x716BADDC`, through `sub_82147788`; GH5's of
2026-11-05): each pass lowers the fret heads, risen after a hit, by the frame's time; held, they fell back in 0.27 s
instead of 0.1 s at 160 FPS (measured in GH5; passes a second at 160 FPS: up to 160 here, 59 held). **Off:** 60 in the launcher.

## The crowd's claps on star power

**No fix here: the game makes it itself.** In GH Metallica and GH Van Halen the crowd claps on the beat while star power
is on, but late by the player's audio calibration, also on the console: the handler of the fretbar events (script
`0x5E4AD8C0`) waits a fixed 0.18 s, and those events come late by the calibration; their `src/crowd.cpp` takes it off
that wait. This game's scripts have no such wait: the handler spawns the crowd's beat script (`0x65DEB5EC`) at once, and
that script waits `$0x99AFF170` ms first, which a new script (`0x0F03CA46`, run as each song starts) sets from the same
calibration: `150 + video - audio` (240 on the PS3). That is GH Metallica's fix, `180 - (audio_offset + video_offset)`,
with another base, so `src/crowd.cpp` is left out: its hook would never find its wait. The claps also fall on beats 2
and 4 here (0 and 2 of the four in GH Metallica's script). Read in the scripts; not listened to yet.

## The game's name

**One disc for every region:** the game calls itself Guitar Hero: Smash Hits in America and Guitar Hero: Greatest Hits in
Europe and Australia, on the title screen and three others, with each region's legal screens. It tells by
`XGetGameRegion`, which the SDK answers with 0xFFFF for every game, the rest of the world: so it said Greatest Hits, whatever
the language. `src/region.cpp`, a hook in `sub_82382C20`, answers as a console in America does. The territory's other
uses change nothing here (they are listed there). **Off:** `ghsmashhits_greatest_hits = true` answers as one in Europe.

## Ultrawide

The same as GH Metallica's (`src/widescreen.cpp`, 8 hooks): every 16:9 perspective is widened by the screen's factor
when the engine builds it (`sub_82356DA8`, Hor+), and the 2D layer (sprites, text, the highway's board, the tails, the
hit sparks, the loading screens, the clip windows) is narrowed to 16:9 in the center; outside a song black bars cover
the sides. With NativeXRenderer the image is rendered at the screen's aspect ratio, or at another one (16:9, 21:9,
32:9; NativeXRenderer D104, `native_x_renderer_aspect`, the launcher's image format), and the menus' bars are placed
at the sides of the image, not of the window. **Off:** 16:9 in the launcher
(`ghsmashhits_ultrawide = false`); `ghsmashhits_ultrawide_hud = false` stretches the 2D layer; `ghsmashhits_menu_bars =
false` shows the room behind the menus at their sides.

## Turning off the screen effects

**Options, all on by default** (GH Metallica's of 2026-10-28): the engine's screen effects work on the 3D image (the
note highway and the HUD are drawn after them), and its final composite (`sub_8251FA60`, the same code as GH
Metallica's) applies six of them in one pass, by a mask with a bit each. `src/screen_fx.cpp` (a hook at `0x8251FC88`)
takes the bits of the effects turned off out of that mask: `ghsmashhits_fx_bloom`, `_depth_of_field`, `_grain`,
`_color`, `_vignette` and `_ink` (the ink blotches a song adds in GH Metallica; not seen in this game yet). With the
color correction off the grain is off too (that pass also tones it down). GH Smash Hits Launcher's switches are under
Settings → Graphics; the F4 settings overlay applies them at once.

## Singing with a PC microphone

**Experimental, not tried in this game yet** (GH Metallica's of 2026-10-28, where Gustavo sang and scored). The game
looks for an Xbox 360 USB microphone; the SDK answers with a recording device of the PC (`usb_microphone_1`, SDK
change 55; `usb_microphone_1_gain = -12` in `ghsmashhits.toml`, as a PC microphone at full level also hears the room),
and the singer's controller gets an ordinary gamepad's subtype, which the game gives the vocals, while the others are
guitars (`gamepad_device_subtypes`, change 56). GH Smash Hits Launcher's Controls tab sets both: the Microphone under
the keyboard (device, channel, gain, level) and each controller's Instrument row (Guitar or Vocals). **Off:**
`usb_microphone_1 = ""` (the default: the launcher's Microphone: None).

## Quitting from the main menu

B on the title screen or the main menu asks whether to quit (A quits, B cancels): `src/quit_prompt.cpp`, a hook in the
main thread's frame (`sub_822E5498`) looks up the title's and the main menu's screen elements (the same names as GH
Metallica's) in the element table at `0x82AAF888`.

## Language

The game has no language menu: it picks its text pack from `XGetLanguage` (French, German, Italian, Spanish, English
otherwise). The launcher's language row sets it. Tried in this game by Gustavo (2026-11-09).

## The job queue's lock and wake-ups (2026-11-08, from GH Metallica)

GH Metallica's trace of a song at 160 FPS (its `docs/cpu-usage.md`): the main thread and the three job workers slept
and woke each other tens of thousands of times a second on the job queue's critical section, which the game creates
with no spin count, and each job queued woke all three waiting workers (a pulse of a manual-reset event) for one of them
to take it. The same engine here, so the same two fixes:
- `critical_section_spin_us = 20` (the SDK's change 75, set in `src/startup.cpp`): a thread finding a game lock taken
  watches it for up to 20 us before it sleeps. GH Metallica: 92,000 -> 31,300 switches a second, 1.84 -> 1.66 cores.
- `ghsmashhits_job_wake_one` (on; `src/thread_waits.cpp`, the job event's pulse hooked): one waiting worker is woken per
  job. GH Metallica: each worker 5,300 -> 3,400 wakes a second, 1.98 -> 1.84 cores.
Not measured here yet. Off: `critical_section_spin_us = 0`, `ghsmashhits_job_wake_one = false`.
