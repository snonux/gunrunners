package org.buetow.gunrunners;

import org.libsdl.app.SDLActivity;

/**
 * SDL2's activity runs the game's native main() (libmain.so) on its own
 * thread; all the game needs from Java is the library list.
 */
public class GunrunnersActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }
}
