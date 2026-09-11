# freeking

Note: The code quality of this right now is very poor, rushing through things as this is just a hobby project.

freeking is an open source reimplementation of [Kingpin: Life of Crime](https://en.wikipedia.org/wiki/Kingpin:_Life_of_Crime)
written in modern C++ and modern OpenGL.

You still need to have the original game assets in order to use this.

---

<a href="https://i.imgur.com/8knsNuQ.jpg">
    <img src="https://i.imgur.com/8knsNuQ.jpg" width="100%">
</a>
<a href="https://i.imgur.com/TcHdpTi.jpg">
    <img src="https://i.imgur.com/TcHdpTi.jpg" width="100%">
</a>

## Android port

A full Android port ships in this repo: an OpenGL ES 3.0 build of the engine
plus a launcher app that sets up your original game files.

**Install (no PC build needed)**

1. Open the [Actions](../../actions) tab, pick the latest green *Android* run,
   download the `freeking-android-debug` artifact and install the APK.
2. Copy your Kingpin game folder (the one containing `main/*.pak`) to the phone.
3. Open the **Freeking** app:
   - tap **Choose game folder** and pick the copied folder
     (or **Choose .pak files** to pick archives one by one),
   - wait for the import to finish, then tap **Play**.

The launcher stores game files in the app-specific folder
(`Android/data/org.freeking/files/kingpin`), so no storage permissions needed.
Works on Russian and English phones — the launcher UI follows the system language.

**Touch controls**

- Left stick: move · drag right side: look · buttons: fire / jump / crouch /
  use / reload / weapon slots / flashlight / objectives / menu.
- Open the pause menu (☰) → **Touch Layout (EDIT)** to move, resize,
  show/hide every control, or disable the touch UI entirely
  (for gamepad / mouse-and-keyboard play).

**Build the APK yourself**

Prereqs: JDK 17, Android SDK (platform 34, build-tools 34),
NDK r26 (`26.3.11579264`), CMake 3.22, Gradle 8.x.
SDL2 sources are downloaded automatically at build time.

```sh
cd android
gradle assembleDebug
# APK: app/build/outputs/apk/debug/app-debug.apk
```

Details: [android/README.md](./android/README.md).

## Desktop build

Prereqs: CMake 3.10+, a C++17 compiler, SDL2, OpenAL.

| OS | Install dependencies |
|----|----------------------|
| Windows | `vcpkg install sdl2 openal-soft` (with `VCPKG_ROOT` set) |
| Ubuntu/Debian | `sudo apt install libsdl2-dev libopenal-dev cmake g++` |
| macOS | `brew install sdl2 openal-soft` |

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

The binary lands in `build/bin/` together with the engine `Assets/`.
Point the game at your Kingpin install in any of these ways:

- install Kingpin via Steam/GOG (auto-detected),
- put a `kingpin/` folder next to the binary,
- set the `FREEKING_KINGPIN_DIR` environment variable.

## Controls (desktop)

WASD move · mouse look · LMB fire · E use · Space jump · C crouch ·
R reload · 1–8 weapons · F flashlight · Tab objectives · Esc menu.

## License
freeking is released as open source software under the [GPL v3](https://opensource.org/licenses/gpl-3.0.html)
license, see the [LICENSE](./LICENSE) file in the project root for the full license text.
