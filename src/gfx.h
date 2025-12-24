#pragma once

#include <stdint.h>

int
gfx_init(int width, int height, const char *title);

void
gfx_fill_rect(int x, int y, int w, int h, uint32_t color);

void
gfx_present(void);

void
gfx_clear(uint32_t color);

int
gfx_poll_quit(void); // returns 1 if user closed window

void
gfx_cleanup(void);
