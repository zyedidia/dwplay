#include <SDL.h>

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *canvas;
static int canvas_width, canvas_height;

int
gfx_init(int width, int height, const char *title)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        return -1;

    // Scale down the window (e.g., 1/2 size)
    int scale = 2;
    int win_width = width / scale;
    int win_height = height / scale;

    window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, win_width, win_height, 0);
    if (!window)
        return -1;

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
        return -1;

    // Create render target texture at full canvas resolution
    canvas = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, width, height);
    if (!canvas)
        return -1;

    SDL_SetTextureBlendMode(canvas, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer, canvas);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    canvas_width = width;
    canvas_height = height;
    return 0;
}

void
gfx_fill_rect(int x, int y, int w, int h, uint32_t color)
{
    SDL_SetRenderDrawColor(renderer,
        (color >> 16) & 0xFF,  // R
        (color >> 8) & 0xFF,   // G
        color & 0xFF,          // B
        (color >> 24) & 0xFF); // A
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderFillRect(renderer, &rect);
}

void
gfx_present(void)
{
    // Switch to window and draw scaled canvas
    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderCopy(renderer, canvas, NULL, NULL);
    SDL_RenderPresent(renderer);

    // Switch back to canvas for next frame
    SDL_SetRenderTarget(renderer, canvas);
}

void
gfx_clear(uint32_t color)
{
    SDL_SetRenderDrawColor(renderer, (color >> 16) & 0xFF, (color >> 8) & 0xFF,
        color & 0xFF, 255);
    SDL_RenderClear(renderer);
}

int
gfx_poll_quit(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT)
            return 1;
    }
    return 0;
}

void
gfx_cleanup(void)
{
    SDL_DestroyTexture(canvas);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
