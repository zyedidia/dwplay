#pragma once

#include <stdint.h>

int gfx_init(int width, int height, const char *title);
void gfx_update(const unsigned char *pixels, int stride);
void gfx_present(void);
int gfx_poll_quit(void);
void gfx_cleanup(void);
