#include "gfx.h"
#include <SDL.h>

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static int tex_width, tex_height;

int
gfx_init(int width, int height, const char *title)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        return -1;

    // Scale down the window (1/2 size)
    int scale = 2;
    int win_width = width / scale;
    int win_height = height / scale;

    window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, win_width, win_height, SDL_WINDOW_RESIZABLE);
    if (!window)
        return -1;

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
        return -1;

    // Enable linear filtering for smooth scaling
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    // Create streaming texture for pixel updates (PlutoVG uses premultiplied ARGB)
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!texture)
        return -1;

    // No blending needed - we're displaying the final composited image
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);

    tex_width = width;
    tex_height = height;
    return 0;
}

void
gfx_update(const unsigned char *pixels, int stride)
{
    SDL_UpdateTexture(texture, NULL, pixels, stride);
}

void
gfx_present(void)
{
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

int
gfx_poll_quit(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT)
            return 1;
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_q)
            return 1;
    }
    return 0;
}

void
gfx_cleanup(void)
{
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
