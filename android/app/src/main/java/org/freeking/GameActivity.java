package org.freeking;

import org.libsdl.app.SDLActivity;

/**
 * The game itself. Started by the launcher once the original game files
 * are in place; SDLActivity boots our native main() on its own thread.
 */
public class GameActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {
            "SDL2",
            // "freeking" matches the jni/CMakeLists target (libfreeking.so).
            "freeking"
        };
    }
}
