#include <SDL.h>

static SDL_Window *window;
static SDL_Renderer *renderer;
static int win_width, win_height;

int
gfx_init(int width, int height, const char *title)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        return -1;

    window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, width, height, 0);
    if (!window)
        return -1;

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
        return -1;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    win_width = width;
    win_height = height;
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
    SDL_RenderPresent(renderer);
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
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
