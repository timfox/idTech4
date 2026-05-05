# FFmpeg video (`videomap`)

Materials can use **`videoMap`** with a file under **`video/`** (or a full qpath). The engine still prefers **ROQ** (original Doom 3 format). If the file is **not** a ROQ, **`idCinematic::Alloc()`** tries **FFmpeg** when the game is built with **`ID_HAVE_FFMPEG`** (see `neo/renderer/Cinematic.cpp` and `neo/renderer/CinematicFFmpeg.cpp`).

## What works

- Decode from **memory** (entire clip is read via `fileSystem->ReadFile`, including from **`.pk4`**).
- Output is **RGBA8** for the existing **`cinematicImage`** upload path.
- Common container extensions: **`.mp4`**, **`.webm`**, **`.mkv`**, **`.avi`**, **`.mov`**, … (anything FFmpeg can probe).

## Windows (Visual Studio)

1. Install or build **FFmpeg** with **shared** or **static** import libraries and headers (e.g. **vcpkg**: `vcpkg install ffmpeg:x64-windows` or `x64-windows-static`).
2. Set MSBuild property **`FFmpegRoot`** to the prefix that contains **`include/libavformat/avformat.h`** and **`lib/*.lib`** (for vcpkg: `installed\x64-windows` or `installed\x64-windows-static`).
3. Import **`neo/_FFmpeg.props`** is already referenced from **`neo/doomdll.vcxproj`**; it defines **`ID_HAVE_FFMPEG=1`** and links **`avformat`**, **`avcodec`**, **`avutil`**, **`swscale`** when **`FFmpegRoot`** is set and the header exists.

Example (Developer Command Prompt, from repo root):

```bat
msbuild neo\Doom3.sln /p:Configuration=Release /p:Platform=x64 /p:FFmpegRoot=C:\path\to\vcpkg\installed\x64-windows
```

Some static FFmpeg builds also require **`swresample`**, **`avfilter`**, or zlib/bzip2 — add those **`.lib`** names to **`neo/_FFmpeg.props`** or your user `.props` if the linker reports unresolved symbols.

## Linux / Steam Deck

There is **no CMake** build for the game in this repository; integrate the same two source files (**`Cinematic.cpp`**, **`CinematicFFmpeg.cpp`**) into your build and add **`pkg-config --cflags --libs libavformat libavcodec libavutil libswscale`** (and any extra libs your distro’s FFmpeg package needs).

## Materials

Same as ROQ: reference the basename as shipped under `video/`, e.g.

```
videomap video/intro_loop.mp4 loop
```

Console **`testVideo`** tab completion lists **`video/`** files with **`.roq`** and common video extensions (see `neo/framework/CmdSystem.h`).

## Notes

- **Audio** from the clip is **not** routed into the Doom 3 sound system; this path is **video texture only** (same category as ROQ cinematics on surfaces / testVideo).
- **Performance**: the whole file is loaded into RAM; prefer short loops or reasonable bitrates for in-world screens.
