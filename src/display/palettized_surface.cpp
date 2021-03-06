#include "palettized_surface.h"

#include <assert.h>

namespace display
{

PalettizedSurface::PalettizedSurface(unsigned int width, unsigned int height, const PaletteInterface &palette)
    : Surface(width, height), _palette(palette)
{
    // scan the palette for transparency
    int transparent_color = -1;

    for (int i = 0; i < 256; i++) {
        if (palette.get(i).a != 255) {
            transparent_color = i;
            break;
        }
    }

    if (transparent_color == -1) {
        // we're not actually transparent
        // disable color keying
        SDL_SetColorKey(_screen, 0, 0);
    } else {
        // enable color keying on this color
        SDL_SetColorKey(_screen, SDL_SRCCOLORKEY, transparent_color);
    }

    // TODO: expose our palette to SDL
    // This will be great once we have a truecolor surface onto which to blit
    // Until then, the blitting routines are perceptual, which does the Wrong Thing™
    //SDL_SetPalette(_screen, SDL_LOGPAL, _palette.sdl_colors, 0, 256);
}

PalettizedSurface::~PalettizedSurface()
{
}

void PalettizedSurface::copyRow(uint32_t row, const void *pixelData)
{
    assert(!SDL_MUSTLOCK(_screen)); // make sure not locking is okay

    char *pixels = (char *)_screen->pixels;
    memcpy(pixels + (_screen->pitch * row), pixelData, _screen->w);
}

} // namespace display
