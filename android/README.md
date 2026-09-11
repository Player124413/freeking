# Freeking — Android port

Two parts:

- `app/src/main/jni/` — the engine as `libfreeking.so`
  (same `Source/` tree as desktop, built with the NDK as
  OpenGL ES 3.0 + SDL2 audio; see `CMakeLists.txt`).
- `app/src/main/java/org/freeking/` — the app:
  - `launcher/LauncherActivity.java` — first-run setup + Play button,
  - `launcher/FileSetup.java` — APK-asset extraction, game-file import,
  - `GameActivity.java` — boots the native engine via SDL.

## File contract (launcher ↔ engine)

| What | Where | Notes |
|------|-------|-------|
| Engine assets (`Fonts/`, `Shaders/`, `Textures/`) | APK assets root → copied to `getFilesDir()/` on first run | `Paths::AssetsDir()` on Android |
| Game files (`main/*.pak`) | `getExternalFilesDir("kingpin")` | `Paths::KingpinDir()` on Android (`SDL_AndroidGetExternalStoragePath() + "/kingpin"`) |
| Settings / saves | `getFilesDir()/` | `Paths::UserDir()` on Android |

Both activities agree on these paths — if you change one side,
change the other (`FileSetup.java` ↔ `Source/Core/Paths.cpp`).

## Build

Prereqs (exact versions pinned in `app/build.gradle`):

- JDK 17, Gradle 8.x
- Android SDK: platform 34, build-tools 34.0.0
- NDK `26.3.11579264`, CMake 3.22.1

```sh
cd android
gradle assembleDebug    # app/build/outputs/apk/debug/app-debug.apk
```

SDL2 sources are downloaded automatically by the `fetchSdl` task
(pinned release tarballs from github.com/libsdl-org/SDL) into
`app/build/sdl-src/` and used for both the Java (`SDLActivity`)
and native (`libSDL2.so`) sides. Clean builds need network access
for that one download; nothing else leaves the machine.

ABIs: `arm64-v8a`, `armeabi-v7a`, `x86_64`. Min SDK 21, GLES 3.0.

## CI

`.github/workflows/android.yml` builds the debug APK on every push/PR
and uploads it as the `freeking-android-debug` artifact.
