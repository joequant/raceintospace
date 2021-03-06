
#include "surface.h"

#include <SDL.h>
#include <algorithm>
#include <assert.h>

namespace display
{

Surface::Surface(unsigned int width, unsigned int height):
    _screenBuffer(new uint8_t[width *height]),
    _screen(SDL_CreateRGBSurfaceFrom(_screenBuffer, width, height, 8, width, 0, 0, 0, 0)),
    _dirty(false)
{
    // Ideally we'd use SDL_CreateRGBSurface here
    // However, we instead allocate our own buffer and CreateRGBSurfaceFrom() on it
    // This ensures that width == pitch
    // We should switch back once we stop fread()ing into pixel buffers
}

Surface::~Surface()
{
    SDL_FreeSurface(_screen);
    _screen = NULL;
    delete[] _screenBuffer;
}

SDL_Surface *Surface::surface() const
{
    return _screen;
}

char *Surface::pixels() const
{
    return (char *)_screen->pixels;
}

void Surface::clear(char colour)
{
    _dirty = true;
    SDL_FillRect(_screen, NULL, colour);
}

void Surface::fillRect(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, char color)
{
    _dirty = true;
    SDL_Rect r;
    unsigned int left = std::min(x1, x2);
    unsigned int right = std::max(x1, x2);
    unsigned int top = std::min(y1, y2);
    unsigned int bottom = std::max(y1, y2);

    r.x = left;
    r.y = top;

    // Original used inclusive coordinates.  The +1 was in the original.
    r.w = (right - left) + 1;
    r.h = (bottom - top) + 1;
    fillRect(r, color);
}

void Surface::fillRect(const SDL_Rect &area, char color)
{
    _dirty = true;
    SDL_FillRect(_screen, const_cast<SDL_Rect *>(&area), color);
}

void Surface::setPixel(unsigned int x, unsigned int y, char color)
{
    _dirty = true;
    *((char *)(_screen->pixels) + (y * _screen->pitch) + x) = color;
}

char Surface::getPixel(unsigned int x, unsigned int y)
{
    _dirty = true;
    assert(x >= 0 && x < width());
    assert(y >= 0 && y < height());

    return ((char *)_screen->pixels)[(y * _screen->pitch) + x];
}

void Surface::outlineRect(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, char color)
{
    _dirty = true;
    line(x1, y1, x2, y1, color);
    line(x2, y1, x2, y2, color);
    line(x2, y2, x1, y2, color);
    line(x1, y2, x1, y1, color);
}

#define swap(a,b) (t = a, a = b, b = t)
void Surface::line(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, char color)
{
    _dirty = true;
    int deltax, deltay;
    int error;
    int ystep;
    int x, y;
    int steep;
    int t;

    steep = std::abs((int)(y2 - y1)) > std::abs((int)(x2 - x1));

    if (steep) {
        swap(x1, y1);
        swap(x2, y2);
    }

    if (x1 > x2) {
        swap(x1, x2);
        swap(y1, y2);
    }

    deltax = x2 - x1;
    deltay = std::abs((int)(y2 - y1));
    error = 0;

    y = y1;

    if (y1 < y2) {
        ystep = 1;
    } else {
        ystep = -1;
    }

    for (x = (int)x1; x <= (int)x2; x++) {
        if (steep) {
            setPixel(y, x, color);
        } else {
            setPixel(x, y, color);
        }

        error = error + deltay;

        if (2 * error >= deltax) {
            y = y + ystep;
            error = error - deltax;
        }
    }
}

void Surface::copyFrom(Surface *surface, unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2)
{
    _dirty = true;

    SDL_Rect src;
    SDL_Rect dst;

    src.x = x1;
    src.y = y1;
    src.w = (x2 - x1) + 1;
    src.h = (y2 - y1) + 1;

    dst.x = 0;
    dst.y = 0;
    dst.w = src.w;
    dst.h = src.h;
    SDL_BlitSurface(surface->_screen, &src, _screen, &dst);
}

void Surface::copyTo(Surface *surface, unsigned int x, unsigned int y, Surface::Operation operation)
{
    surface->_dirty = true;

    int row, col, from_idx, to_idx;
    int clip_x, clip_y;

    assert(surface);
    assert(operation == Surface::Set || operation == Surface::Xor);
    assert(0 <= x && x < surface->width());
    assert(0 <= y && y < surface->height());

    clip_y = std::min(height() + y, surface->height()) - y;
    clip_x = std::min(width() + x, surface->width()) - x;

    switch (operation) {
    case Surface::Set:
        for (row = 0; row < clip_y; row++) {
            from_idx = row * width();
            to_idx = (y + row) * surface->width() + x;

            void *dst = (void *)((char *)surface->_screen->pixels + to_idx);
            void *src = (void *)(((char *)_screen->pixels) + from_idx);
            memcpy(dst, src, clip_x);
        }

        break;

    case Surface::Xor:
        for (row = 0; row < clip_y; row++) {
            from_idx = row * width();
            to_idx = (y + row) * surface->width() + x;

            for (col = 0; col < clip_x; col++) {
                char *dst = (char *)surface->_screen->pixels + (to_idx + col);
                char *src = ((char *)_screen->pixels) + (from_idx + col);
                *dst ^= *src;
            }
        }

        break;
    }
}

void Surface::copyTo(Surface *surface, unsigned int srcX, unsigned int srcY, unsigned int destX1, unsigned int destY1, unsigned int destX2, unsigned int destY2)
{
    int row, from_idx, to_idx;
    int copy_width, copy_height;
    int clip_x, clip_y;

    assert(surface);
    assert(0 <= srcX && srcX < width());
    assert(0 <= srcY && srcY < height());
    assert(destX1 <= destX2);
    assert(destY1 <= destY2);
    assert(0 <= destX1 && destX1 < surface->width());
    assert(0 <= destX2 && destX2 < surface->width());
    assert(0 <= destY1 && destY1 < surface->height());
    assert(0 <= destY2 && destY2 < surface->height());

    copy_width  = destX2 - destX1 + 1;
    copy_height = destY2 - destY1 + 1;

    clip_y = std::min(copy_height + destY1, surface->height()) - destY1;
    clip_x = std::min(copy_width  + destX1, surface->width()) - destX1;

    for (row = 0; row < clip_y; row++) {
        from_idx = (srcY + row) * width() + srcX;
        to_idx = (destY1 + row) * surface->width() + destX1;

        void *dst = (char *)surface->_screen->pixels + to_idx;
        void *src = ((char *)_screen->pixels) + from_idx;
        memcpy(dst, src, clip_x);
    }
}

void Surface::scaleTo(Surface *surface)
{
    int dest_row, dest_col, dest_idx;
    int src_row, src_col, src_idx;

    for (dest_row = 0; dest_row < (int)surface->height(); dest_row++) {
        src_row = (dest_row * (int)height()) / (int)surface->height();

        for (dest_col = 0; dest_col < (int)surface->width(); dest_col++) {
            src_col  = (dest_col * (int)width()) / (int)surface->width();

            src_idx  = src_row  * width()  + src_col;
            dest_idx = dest_row * surface->width() + dest_col;

            char *dst = (char *)surface->_screen->pixels + dest_idx;
            char *src = (char *)_screen->pixels + src_idx;
            *dst = *src;
        }
    }
}

void Surface::copyFrom(Surface *surface, unsigned int srcX1, unsigned int srcY1, unsigned int srcX2, unsigned int srcY2, unsigned int dstX, unsigned int dstY)
{
    int row, from_idx, to_idx;
    int copy_width, copy_height;

    assert(surface);
    assert(0 <= srcX1 && srcX1 < surface->width());
    assert(0 <= srcX2 && srcX2 < surface->width());
    assert(0 <= srcY1 && srcY1 < surface->height());
    assert(0 <= srcY2 && srcY2 < surface->height());
    assert(srcX1 <= srcX2);
    assert(srcY1 <= srcY2);
    assert(0 <= dstX && dstX < width());
    assert(0 <= dstY && dstY < height());

    copy_width  = srcX2 - srcX1 + 1;
    copy_height = srcY2 - srcY1 + 1;

    assert(copy_width  <= (int)width());
    assert(copy_height <= (int)height());

    for (row = 0; row < copy_height; row++) {
        from_idx = (srcY1 + row) * surface->width() + srcX1;
        to_idx = (dstY + row) * width() + dstX;

        void *dst = (void *)(((char *)_screen->pixels) + to_idx);
        void *src = (void *)((char *)surface->_screen->pixels + from_idx);
        memcpy(dst, src, copy_width);
    }
}

void Surface::maskCopy(Surface *source, char maskValue, Surface::MaskSource maskSource, char offset)
{
    assert(source->width() == width());
    assert(source->height() == height());

    unsigned int size = (width() * height());

    for (unsigned int i = 0; i < size; i++) {
        char *src = (char *)source->_screen->pixels + i;
        char *dst = (char *)_screen->pixels + i;

        switch (maskSource) {
        case Surface::SourceEqual:
            if (*src == maskValue) {
                *dst = (*src) + offset;
            }

            break;

        case Surface::SourceNotEqual:
            if (*src != maskValue) {
                *dst = (*src) + offset;
            }

            break;

        case Surface::DestinationEqual:
            if (*dst == maskValue) {
                *dst = (*src) + offset;
            }

            break;

        case Surface::DestinationNotEqual:
            if (*dst != maskValue) {
                *dst = (*src) + offset;
            }

            break;
        }
    }
}

void Surface::filter(char testValue, char offset, Surface::FilterTest filterTest)
{
    unsigned int size = (width() * height());

    for (unsigned int i = 0; i < size; i++) {
        char *src = (char *)_screen->pixels + i;

        switch (filterTest) {
        case Surface::Equal:
            if (*src == testValue) {
                *src += offset;
            }

            break;

        case Surface::NotEqual:
            if (*src != testValue) {
                *src += offset;
            }

            break;

        case Surface::Any:
            *src += offset;
            break;
        }
    }
}

void Surface::draw(Surface *surface, unsigned int srcX, unsigned int srcY, unsigned int srcW, unsigned int srcH, unsigned int x, unsigned int y)
{
    _dirty = true;

    SDL_Rect src;
    SDL_Rect dst;

    src.x = srcX;
    src.y = srcY;
    src.w = srcW;
    src.h = srcH;

    dst.x = x;
    dst.y = y;
    dst.w = src.w;
    dst.h = src.h;
    SDL_BlitSurface(surface->_screen, &src, _screen, &dst);
}


} // namespace display
