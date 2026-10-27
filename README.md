# Web-camera-interaction

An interactive photo booth for a webcam: an AI cuts you out and puts a new
or blurred background behind you, stickers follow your face, filters and
frames dress up the picture, a gecko game reacts to your hand, and pictures
are taken with a countdown, a wave at an on-screen camera icon, or as a
four-shot strip. Built with C++17, OpenCV (including its DNN module), and
Dear ImGui; runs on Windows, Linux, and macOS. The interface is in
Traditional Chinese.

## Using it

1. **相機** (camera) opens by itself. If it cannot, press 重試 or pick another
   camera under 進階設定 (Advanced).
2. **開始玩** (play) with the buttons along the bottom.

Without the AI models (see Building) a "記住背景" (learn the background) step
appears in between: press it and step out of view for three seconds.

| Button | What it does |
|---|---|
| 虛擬背景 | Replace or blur what is behind you. Cut-out by AI (default), by comparing with a learned empty scene, or by color for a plain backdrop (綠幕) |
| 貼圖 | Stickers that follow your face (sunglasses, party hat, mustache, cat ears, crown), and stickers placed on the picture (gecko, dragon balls, a colored box): drag to move, mouse wheel to resize |
| 濾鏡 | Looks (beauty, cartoon, sketch, vintage), brightness / contrast / saturation, vignette, mirror, black and white, single color tone, mark skin or a chosen color, frames (simple, instant photo, film) and a date stamp |
| 抓壁虎 | 30-second game: touch the geckos to catch them. Easy (1 still), normal (2 moving), hard (3 fast); best scores are kept |
| 恢復預設 | Switch every effect, sticker, and game off (mirror stays on) |
| 揮手拍照 | Reach into the camera icon in the bottom-right corner to take a picture |
| 暫停畫面 | Freeze the picture; 拍照 then saves the frozen picture at once |
| 拍照 | Three-second countdown with beeps, then a picture without the camera icon. Right-click: shoot now, or copy the current picture to the clipboard |
| 四格拍貼 | Four pictures, two seconds apart, saved as one 2 x 2 strip |
| 錄影 | Record what is on screen |

"Touching" (the camera icon, the geckos) means moving into a place: a body
standing still next to the icon does not take a picture.

- **Keys:** Space takes a picture, F11 toggles full screen (just the picture
  and the buttons, for events), Esc leaves full screen.
- **Your own pictures:** drop image files on the window. Pictures with a
  transparent background (PNG) become stickers, others become backgrounds.
  Copies are kept in `user/` next to the program for next time.
- **Photos** appear along the bottom; click one to open it, right-click to
  copy it to the clipboard (Windows only) or open its folder. They are saved
  in `captures/` next to the program.
- **Settings** (camera, effects, sticker positions, best scores, ...) are
  saved in `settings.ini` next to the program when it closes.

進階設定 (Advanced) holds:

- **相機**: which camera, size (up to 1920 x 1080), capture backend, frame
  rate to ask for (default / 30 / 60 / 120), MJPG, the GPU switch, preview
  rate (auto = as fast as frames arrive), and the driver's own settings
  (exposure, white balance, focus, ...).
- **背景偵測**: learning the empty scene (with a preview of it), sensitivity,
  how fast it adapts, and debug views of what the detector sees.
- **顏色範圍**: the color range for 綠幕 and for marking a chosen color.
- **照片與影片**: also keeping an unedited camera picture, recording format,
  and opening the photo folder.
- **音樂與音效**: sound effects, and music: put `僕が死のうと思ったのは.mp3` or
  `music.mp3` next to the program to load it on start.
- **資訊**: camera format, camera / processing / screen frame rates, time per
  frame, AI time, GPU, and which models are loaded.
- **紀錄**: the log.

### Speed

The AI person mask and face detection run on their own thread and the picture
uses their newest result, so they do not hold up the frame (about 10 ms and
3 ms per run on a 2022 desktop CPU, on the CPU because OpenCV's OpenCL path is
slower for these small models). The beauty and cartoon looks use the GPU
through OpenCV's OpenCL when there is one (about 3 ms instead of 8-12 ms at
1280 x 720); turn it off under 進階設定 if a driver misbehaves. With that, a
640 x 480 frame with a new background and a look takes about 7 ms, so 100 fps
is within reach there; 1280 x 720 takes about 13-17 ms (60-75 fps). The
screen updates at the monitor's refresh rate (vsync).

The camera is usually the limit: most webcams send 30 fps. To try for more,
pick 640 x 480, MJPG and 120 fps under 進階設定 → 相機, press 套用並重新開啟相機,
and compare the camera, processing and screen rates under 資訊. If the camera
still reports 30, that is its maximum.

The GPU path needs an OpenCL driver (most Intel, AMD and NVIDIA graphics
drivers have one); without it everything runs on the CPU.

## Building

Requires CMake 3.21+, a C++17 compiler, OpenCV 4 with the `dnn` and
`objdetect` modules, and an internet connection on the first configure:
GLFW, Dear ImGui, and miniaudio are downloaded, and so are two models from
the [OpenCV Zoo](https://github.com/opencv/opencv_zoo):
[PP-HumanSeg](https://github.com/opencv/opencv_zoo/tree/main/models/human_segmentation_pphumanseg)
(person segmentation, Apache-2.0) and
[YuNet](https://github.com/opencv/opencv_zoo/tree/main/models/face_detection_yunet)
(face detection, MIT). They are checked by SHA-256 and copied next to the
program. Configure with `-DWEBCAM_DOWNLOAD_MODELS=OFF` to build without them;
the app then learns the background instead and has no face stickers.

**Linux** (Debian/Ubuntu)

```bash
sudo apt install build-essential cmake git libopencv-dev libgl1-mesa-dev \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
    libwayland-dev libxkbcommon-dev fonts-noto-cjk
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/WebcamInteraction
```

**macOS**

```bash
brew install cmake opencv
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/WebcamInteraction
```

The first run asks for camera access for the terminal that starts it.

**Windows** (Visual Studio 2019 or later)

Download `opencv-4.x-windows.exe` from the
[OpenCV releases](https://github.com/opencv/opencv/releases) and unpack it so
that `third_party/opencv/build/OpenCVConfig.cmake` exists (it is a 7-Zip
archive). Or point `OpenCV_DIR` at an OpenCV you already have.

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
build\Release\WebcamInteraction.exe
```

With Visual Studio 2026 use `-G "Visual Studio 18 2026"`. OpenCV's prebuilt
package does not know that compiler yet, so the build uses its VS 2019 (vc16)
binaries, which are compatible.

The OpenCV DLL, the `assets` folder and the models (in `assets/models`) are
copied next to the executable.

Build Release: background detection is several times slower unoptimized.

## Checking a build

```bash
./build/WebcamInteraction --self-test              # no window or camera needed
./build/WebcamInteraction --screenshot ui.png 1    # draw a scene (0-7) to a PNG, hidden window
ctest --test-dir build -C Release                  # tests for src/core
```

The self-test loads every picture and both models, checks OpenCV's YUY2
conversion against the core's, runs the full picture-taking pipeline, and
times segmentation, face detection, every look (with and without the GPU),
frame, and sticker at three sizes. Neither it nor `--screenshot` opens the
camera or touches `settings.ini`.

The core and its tests need no third-party libraries:

```bash
cmake -S . -B build-core -DWEBCAM_BUILD_APP=OFF
cmake --build build-core && ctest --test-dir build-core
```

## Layout

```
src/core/       frame processing: YUY2 frames, background detection, sprites, effect chain
src/app/        the application
  main          window, fonts, --self-test and --screenshot
  App           the photo booth: pipeline order and the Dear ImGui interface
  Camera        a camera owned by a background thread
  Vision        AI person segmentation and face detection, and the thread they run on
  Style         looks (CPU or GPU), adjustments, frames, date stamp, collage, background compositing
  Stickers      face stickers drawn in code, BGRA overlay
  Sound, Music  synthesized sound effects, music playback (miniaudio)
  Settings      settings.ini
  Assets        finding and loading the pictures and models
  Texture       OpenGL textures for the picture and thumbnails
  Platform      UTF-8 paths, opening files, clipboard (per OS)
tests/          tests for src/core
assets/images/  pictures for the virtual objects and backgrounds
```

Next to the program at run time: `assets/` (pictures and `models/`),
`captures/` (photos and videos), `user/` (dropped stickers and backgrounds)
and `settings.ini`.

## History

This started as a C++Builder course project built on Video for Windows. That
version, with the core extracted and its bugs fixed, is commit `e32c155` on
`main`. Everything it did is still here: the camera settings dialog, preview
rate, background learning and difference views, color highlights, the gecko
and dragon-ball objects, the camera-icon trigger, recording, and music.
