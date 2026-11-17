# Building blur

Two supported ways: the normal Visual Studio + vcpkg route (what CI uses), or a
fully portable toolchain that needs no administrator rights (how the released
binaries were produced).

---

## 0. Common requirements

| | |
| --- | --- |
| OS | Windows 10 1803+ / 11, x64 |
| Compiler | MSVC 14.4x (Visual Studio 2022, "Desktop development with C++") |
| Windows SDK | 10.0.22621 or newer |
| CMake | ≥ 3.25 |
| Ninja | any recent |
| vcpkg | with the manifest in `vcpkg.json` |

> **Two things will bite you:**
> 1. **Use a static triplet.** `CMakeLists.txt` defines `BOOST_FILESYSTEM_STATIC_LINK=1`
>    and sets `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`. With the dynamic
>    `x64-windows` triplet the compiler refuses the conflicting
>    `BOOST_FILESYSTEM_DYN_LINK` define.
> 2. **No non-ASCII characters in the path.** `rc.exe` is an ANSI program; a path
>    like `D:\源码\blur` fails at link time with `RC Pass 1 ... no such file or directory`.

---

## 1. Normal route (Visual Studio + vcpkg)

```powershell
git clone --recursive https://github.com/SORA123k/blur.git
cd blur

# install the dependencies once (this also builds boost, openssl, sdl3, ...)
vcpkg install --triplet x64-windows-static

cmake -S . -B build -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static

cmake --build build --target blur blur-cli
```

Output: `build\blur-gui.exe` (GUI) and `build\blur-cli.exe` (CLI).

`CMakePresets.json` has `win-release` / `win-debug` presets if you prefer
`cmake --preset win-release`, they expect `VCPKG_ROOT` to be set.

### Running from the build tree

`blur-gui.exe` looks for its runtime files in `<exe dir>\lib`:

```
blur-gui.exe
lib\ffmpeg\ffmpeg.exe, ffprobe.exe
lib\vapoursynth\vspipe.exe, vs-plugins\*.dll, ...
lib\models\rife-v4.26_ensembleFalse\
lib\blur.py, lib\blur\*.py
```

The build step already copies `src/vapoursynth` into `bin\Release\lib`; the
FFmpeg / VapourSynth / RIFE payload comes from the upstream release archive and is
not built here.

---

## 2. Portable route (no administrator, everything under `D:\apps`)

This is how the published binaries were built on a machine that had MSVC but no
registered Windows SDK/CMake/vcpkg at all.

```
D:\apps\cmake            portable CMake zip
D:\apps\ninja            ninja.exe
D:\apps\vcpkg            vcpkg clone (+ custom triplet, see below)
D:\apps\winsdk\sdk       Windows SDK assembled from NuGet packages
D:\apps\blur-build       build tree (source mirror + deps + build/)
```

1. **SDK from NuGet** — the machine had `cl.exe` but no `windows.h`:

   ```
   nuget: Microsoft.Windows.SDK.CPP            (headers)
   nuget: Microsoft.Windows.SDK.CPP.x64        (import libs)
   nuget: Microsoft.Windows.SDK.BuildTools     (rc.exe / mt.exe)
   ```

   `tools/assemble-sdk.ps1` (see the script in this repo's history) copies them
   into `sdk\Include\10.0.x`, `sdk\Lib\10.0.x\{ucrt,um}\x64`, `sdk\References\
   10.0.x\<contract>\4.0.0.0\*.winmd` and `sdk\bin\10.0.x\x64`, then
   `env.cmd` exports `INCLUDE`, `LIB`, `WindowsSdkDir`, `WindowsSDKVersion`,
   `CMAKE_RC_COMPILER`, `CMAKE_MT` and `VCPKG_ROOT`.

2. **Custom triplet** `x64-windows-static-portable.cmake` with
   `VCPKG_CRT_LINKAGE static` / `VCPKG_LIBRARY_LINKAGE static`.

3. **cppwinrt overlay port** — the `cppwinrt` port reads `%WindowsSDKDir%`, which
   vcpkg strips from the port environment, so an overlay port with the SDK path
   baked in is needed (`ports-extra/cppwinrt`).

4. Build:

   ```bat
   call D:\apps\winsdk\env.cmd
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
     -DCMAKE_TOOLCHAIN_FILE=D:/apps/vcpkg/scripts/buildsystems/vcpkg.cmake ^
     -DVCPKG_TARGET_TRIPLET=x64-windows-static-portable ^
     -DVCPKG_INSTALLED_DIR=D:/apps/blur-build/vcpkg_installed_static
   cmake --build build --target blur blur-cli
   ```

---

## 3. Linux / macOS

Upstream supports both; the UI changes here are portable (SDL3 + OpenGL + imgui),
only `src/gui/os/desktop_notification*.cpp` is platform specific.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --target blur blur-cli
```

Not tested by this fork's CI (Windows only); please open an issue if it breaks.

---

## 4. Regenerating the generated assets

The repo already contains the generated files; regenerate them only when you
change translations or want different flags.

| What | Command | Output |
| --- | --- | --- |
| translation tables | `python tools/gen_i18n.py` | `src/common/i18n_table.h` |
| font subset | `python tools/build_font.py` (needs `fonttools`, Noto Sans SC/KR) | `src/gui/fonts/cn_sans.h` |
| flags | `python tools/build_flags.py` | `src/gui/ui/flags.h` |

---

## 5. Packaging

```powershell
# portable zip
Compress-Archive -Path out\* -DestinationPath blur-portable.zip

# installer: tools/installer/build.bat, then tools/installer/package.ps1
# the exe is a small stub + the payload zip + a 16 byte footer:
#   8 bytes  offset of the zip inside the exe
#   8 bytes  ascii magic "BLURZIP1"
# On run it shows a folder picker (default %LOCALAPPDATA%\Programs\blur), extracts
# with the built-in tar.exe, writes shortcuts, an uninstall.ps1 and the
# "Apps & features" registry entry.
cd tools\installer
build.bat
.\package.ps1 -PayloadZip ..\..\blur-payload.zip -OutFile ..\..\blur-setup.exe
```
