# Documentation

Short notes on each problem found while making the game run, and what was done about it. Each one gives
the symptom, the cause, the fix and how to turn the fix off, so that you can check it on your own machine.

Guitar Hero: Smash Hits runs on the same engine as Guitar Hero: Metallica (its executable is `gh4_2_Final.exe`,
Metallica's `gh4_1_burn_Final.exe`) with the same XDK, so it has the same problems. GHMetalRecomp's fixes were ported
with every address translated; the investigations are GHMetalRecomp's.

To build the project and work on it: [Building and developing](development.md).

| Note | Status |
|---|---|
| [The fixes ported from GH Metallica](fixes-from-gh-metallica.md), and what differs here | Working, played with an Xbox 360 guitar: the threads' waits, the song clock, the audio and Low latency audio, frame rates above 60, ultrawide, the screen effects' switches, quitting from the main menu, the languages, the game's name by its territory. Ported, not tried here yet: the keyboard and a controller as a guitar, the second controller, singing with a PC microphone (experimental) |
| [NativeXRenderer in this game](native-x-renderer-ghsmashhits.md) | Working: GH Metallica's map and profile, translated (the default renderer) |
| [Stutters the first time something is shown](compiling-shaders.md) | Fixed: a pipeline list ships with the game; the launcher compiles it for your graphics card at the first Play ("Compiling shaders"; NativeXRenderer) |
