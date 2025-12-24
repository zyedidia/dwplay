#pragma once

#include <stdint.h>

struct Canvas;
struct Context2D;

// Canvas
struct Canvas *
canvas_new(unsigned width, unsigned height);
void
canvas_destroy(struct Canvas *canvas);
unsigned
canvas_width_get(struct Canvas *canvas);
void
canvas_width_set(struct Canvas *canvas, unsigned val);
unsigned
canvas_height_get(struct Canvas *canvas);
void
canvas_height_set(struct Canvas *canvas, unsigned val);
struct Context2D *
canvas_getContext(struct Canvas *canvas, const char *contextType);

// Context2D
void
ctx2d_fillStyle_set(struct Context2D *ctx2d, uint32_t color);
uint32_t
ctx2d_fillStyle_get(struct Context2D *ctx2d);
void
ctx2d_globalAlpha_set(struct Context2D *ctx2d, double globalAlpha);
double
ctx2d_globalAlpha_get(struct Context2D *ctx2d);
void
ctx2d_lineWidth_set(struct Context2D *ctx2d, double lineWidth);
double
ctx2d_lineWidth_get(struct Context2D *ctx2d);
void
ctx2d_fillRect(struct Context2D *ctx2d, double x, double y, double w, double h);
void
ctx2d_clearRect(struct Context2D *ctx2d, double x, double y, double w,
    double h);
void
ctx2d_beginPath(struct Context2D *ctx2d);
void
ctx2d_arc(struct Context2D *ctx2d, double x, double y, double r,
    double startAngle, double endAngle, int ccw);
void
ctx2d_stroke(struct Context2D *ctx2d);
void
ctx2d_scale(struct Context2D *ctx2d, double x, double y);
void
ctx2d_setTransform(struct Context2D *ctx2d, double a, double b, double c,
    double d, double e, double f);
void
ctx2d_fillText(struct Context2D *ctx2d, const char *text, double x, double y);

// Get pixel data (ARGB premultiplied format)
unsigned char *
ctx2d_get_data(struct Context2D *ctx2d);
int
ctx2d_get_stride(struct Context2D *ctx2d);
