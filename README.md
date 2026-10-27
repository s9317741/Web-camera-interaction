# Web-camera-interaction

A C++Builder (VCL) desktop app that plays with a live webcam feed through
Video for Windows: color effects, mirroring, virtual objects, background
difference detection, and two small interactions driven by motion.

## Features

| Menu | What it does |
|---|---|
| Preview Callback Function → Color Mode | Gray, or tint the whole picture with a chosen color |
| Preview Callback Function → Mirror | Flip the picture horizontally |
| Preview Callback Function → Object | Draw a colored box, a spotted gecko, or a dragon ball (click a ball picture to switch) |
| Preview Callback Function → Color Filtering | Tint pixels inside the Y/U/V range set on the form |
| Preview Callback Function → Background | Average the next 30 frames into a background image |
| Preview Callback Function → Difference Detection | Show what differs from the background: color, binary, or 8x8 blocks |
| Interaction → Virtual Spotted Gecko | The gecko disappears when you touch it; Replay brings it back elsewhere |
| Interaction → TakePicture | Put a virtual background behind you (no green screen needed); touch the camera icon to save a picture |
| Interaction → Color Key Background | Make TakePicture replace the Y/U/V color range instead (for a plain colored backdrop) |

Effects can be switched on together; checked menu items are active.
Off turns everything off.

## Usage

1. WebCam → AutoDisplay. (Step by step: Capture Window → Create,
   Driver - Capture Window → Connect, Display Mode: Preview → Enable.)
2. The effects need the YUY2 video format. If the frame counter says otherwise,
   change it under Dialog → Video Format.
3. For the difference views, the gecko game, and TakePicture, step out of view
   and run Background first; everything is measured against it.
4. Threshold (x noise) sets how far, in multiples of each pixel's own noise,
   a pixel must differ to count as foreground. Lower finds more, higher is quieter.
   Brightness and white-balance drift and shadows are compensated, but turning
   off auto exposure and auto white balance under Dialog → Video Source still helps.
   If the room changes for good (a moved chair), run Background again.
5. Optional: put `僕が死のうと思ったのは.mp3` next to the executable to enable the music player.

## Building

Open `WebcamInteraction.cbproj` in RAD Studio 10.4 or later and build Win32 or Win64.
Use the Release configuration for live video: unoptimized builds are too slow
for background detection above 640x480.

Up to 1920x1080 works as long as the camera delivers YUY2 at that size
(many only offer MJPG there, or YUY2 at a few frames per second over USB 2.0).
Large frames are analysed at reduced resolution, so the cut-out edge is about
2 pixels coarse at 1080p.

The frame processing in `src/core` has no VCL dependency and is tested
separately with CMake and any C++11 compiler:

```bash
cmake -S tests -B build/tests
cmake --build build/tests
./build/tests/core_tests
```

## Layout

```
WebcamInteraction.cbproj / .cpp / .res   project
src/MainForm.*                           the form: capture window, menus, frame callback
src/core/                                YUY2 frames, change detection, sprites, effect chain
tests/                                   tests for src/core
assets/images/                           source bitmaps (copies are embedded in MainForm.dfm)
```
