#include "canvas.h"
#include "plutovg.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct Canvas {
    unsigned width;
    unsigned height;

    struct Context2D *ctx2d;
};

struct Context2D {
    struct Canvas *canvas;

    plutovg_surface_t *pvg_surface;
    plutovg_canvas_t *pvg_canvas;

    plutovg_color_t fillStyle;
    plutovg_color_t strokeStyle;
};

static struct Context2D *
ctx2d_new(struct Canvas *canvas)
{
    struct Context2D *ctx2d = malloc(sizeof(*ctx2d));
    if (!ctx2d)
        return NULL;

    *ctx2d = (struct Context2D) {
        .canvas = canvas,
    };

    ctx2d->pvg_surface = plutovg_surface_create(canvas->width, canvas->height);
    ctx2d->pvg_canvas = plutovg_canvas_create(ctx2d->pvg_surface);

    // Clear to white (dwitter default)
    plutovg_surface_clear(ctx2d->pvg_surface, &PLUTOVG_WHITE_COLOR);

    ctx2d->fillStyle = PLUTOVG_BLACK_COLOR;
    ctx2d->strokeStyle = PLUTOVG_BLACK_COLOR;
    plutovg_canvas_set_opacity(ctx2d->pvg_canvas, 1.0f);

    return ctx2d;
}

static void
ctx2d_destroy(struct Context2D *ctx2d)
{
    if (!ctx2d)
        return;
    plutovg_canvas_destroy(ctx2d->pvg_canvas);
    plutovg_surface_destroy(ctx2d->pvg_surface);
    free(ctx2d);
}

struct Canvas *
canvas_new(unsigned width, unsigned height)
{
    struct Canvas *canvas = malloc(sizeof(*canvas));
    if (!canvas)
        return NULL;
    *canvas = (struct Canvas) {
        .width = width,
        .height = height,
        .ctx2d = NULL,
    };
    return canvas;
}

void
canvas_destroy(struct Canvas *canvas)
{
    if (!canvas)
        return;
    ctx2d_destroy(canvas->ctx2d);
    free(canvas);
}

struct Context2D *
canvas_getContext(struct Canvas *canvas, const char *contextType)
{
    if (strcmp(contextType, "2d") != 0)
        return NULL;
    if (canvas->ctx2d)
        return canvas->ctx2d;

    struct Context2D *ctx2d = ctx2d_new(canvas);
    if (!ctx2d)
        return NULL;
    canvas->ctx2d = ctx2d;
    return ctx2d;
}

static void
ctx2d_reset(struct Context2D *ctx2d);

unsigned
canvas_width_get(struct Canvas *canvas)
{
    return canvas->width;
}

void
canvas_width_set(struct Canvas *canvas, unsigned val)
{
    /* if (val != canvas->width) */
    /*     fprintf(stderr, "warning: setting canvas width to a different value is not supported\n"); */
    if (canvas->ctx2d)
        ctx2d_reset(canvas->ctx2d);
}

unsigned
canvas_height_get(struct Canvas *canvas)
{
    return canvas->height;
}


void
canvas_height_set(struct Canvas *canvas, unsigned val)
{
    /* if (val != canvas->height) */
    /*     fprintf(stderr, "warning: setting canvas height to a different value is not supported\n"); */
    if (canvas->ctx2d)
        ctx2d_reset(canvas->ctx2d);
}

static void
ctx2d_reset(struct Context2D *ctx2d)
{
    // Clear to white (dwitter default)
    plutovg_surface_clear(ctx2d->pvg_surface, &PLUTOVG_WHITE_COLOR);

    // Reset path
    plutovg_canvas_new_path(ctx2d->pvg_canvas);

    // Reset transform
    plutovg_canvas_reset_matrix(ctx2d->pvg_canvas);

    // Reset properties to defaults
    ctx2d->fillStyle = PLUTOVG_BLACK_COLOR;
    ctx2d->strokeStyle = PLUTOVG_BLACK_COLOR;
    plutovg_canvas_set_opacity(ctx2d->pvg_canvas, 1.0f);
}

// Instance properties

static plutovg_color_t
color_from_argb(uint32_t argb)
{
    return PLUTOVG_MAKE_COLOR(
        ((argb >> 16) & 0xFF) / 255.0f,
        ((argb >> 8) & 0xFF) / 255.0f,
        (argb & 0xFF) / 255.0f,
        ((argb >> 24) & 0xFF) / 255.0f
    );
}

static uint32_t
color_to_argb(plutovg_color_t c)
{
    uint8_t a = (uint8_t)(c.a * 255.0f);
    uint8_t r = (uint8_t)(c.r * 255.0f);
    uint8_t g = (uint8_t)(c.g * 255.0f);
    uint8_t b = (uint8_t)(c.b * 255.0f);
    return (a << 24) | (r << 16) | (g << 8) | b;
}

void
ctx2d_fillStyle_set(struct Context2D *ctx2d, uint32_t color)
{
    ctx2d->fillStyle = color_from_argb(color);
}

uint32_t
ctx2d_fillStyle_get(struct Context2D *ctx2d)
{
    return color_to_argb(ctx2d->fillStyle);
}

void
ctx2d_globalAlpha_set(struct Context2D *ctx2d, double globalAlpha)
{
    if (globalAlpha < 0.0) globalAlpha = 0.0;
    if (globalAlpha > 1.0) globalAlpha = 1.0;
    plutovg_canvas_set_opacity(ctx2d->pvg_canvas, (float)globalAlpha);
}

double
ctx2d_globalAlpha_get(struct Context2D *ctx2d)
{
    return plutovg_canvas_get_opacity(ctx2d->pvg_canvas);
}

void
ctx2d_lineWidth_set(struct Context2D *ctx2d, double lineWidth)
{
    plutovg_canvas_set_line_width(ctx2d->pvg_canvas, (float)lineWidth);
}

double
ctx2d_lineWidth_get(struct Context2D *ctx2d)
{
    return plutovg_canvas_get_line_width(ctx2d->pvg_canvas);
}

// Instance functions

void
ctx2d_fillRect(struct Context2D *ctx2d, double x, double y, double w, double h)
{
    plutovg_color_t *c = &ctx2d->fillStyle;
    plutovg_canvas_set_rgba(ctx2d->pvg_canvas, c->r, c->g, c->b, c->a);
    plutovg_canvas_fill_rect(ctx2d->pvg_canvas, (float)x, (float)y, (float)w, (float)h);
}

void
ctx2d_clearRect(struct Context2D *ctx2d, double x, double y, double w, double h)
{
    // Clear to white (dwitter's page background)
    // In browsers, clearRect makes pixels transparent, revealing the page background.
    // For dwitter compatibility, we clear to white since that's dwitter's background.
    float opacity = plutovg_canvas_get_opacity(ctx2d->pvg_canvas);
    plutovg_canvas_set_opacity(ctx2d->pvg_canvas, 1.0f);
    plutovg_canvas_set_rgba(ctx2d->pvg_canvas, 1, 1, 1, 1);
    plutovg_canvas_set_operator(ctx2d->pvg_canvas, PLUTOVG_OPERATOR_SRC);
    plutovg_canvas_fill_rect(ctx2d->pvg_canvas, (float)x, (float)y, (float)w, (float)h);
    plutovg_canvas_set_operator(ctx2d->pvg_canvas, PLUTOVG_OPERATOR_SRC_OVER);
    plutovg_canvas_set_opacity(ctx2d->pvg_canvas, opacity);
}

void
ctx2d_beginPath(struct Context2D *ctx2d)
{
    plutovg_canvas_new_path(ctx2d->pvg_canvas);
}

void
ctx2d_arc(struct Context2D *ctx2d, double x, double y, double r,
    double startAngle, double endAngle, int ccw)
{
    plutovg_canvas_arc(ctx2d->pvg_canvas, (float)x, (float)y, (float)r,
        (float)startAngle, (float)endAngle, ccw);
}

void
ctx2d_stroke(struct Context2D *ctx2d)
{
    plutovg_color_t *c = &ctx2d->strokeStyle;
    plutovg_canvas_set_rgba(ctx2d->pvg_canvas, c->r, c->g, c->b, c->a);
    // Use stroke_preserve - Canvas2D stroke() does not clear the path
    plutovg_canvas_stroke_preserve(ctx2d->pvg_canvas);
}

unsigned char *
ctx2d_get_data(struct Context2D *ctx2d)
{
    return plutovg_surface_get_data(ctx2d->pvg_surface);
}

int
ctx2d_get_stride(struct Context2D *ctx2d)
{
    return plutovg_surface_get_stride(ctx2d->pvg_surface);
}
