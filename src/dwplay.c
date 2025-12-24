#include "gfx.h"
#include "quickjs.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static char *
read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(len + 1);
    if (!buf)
        goto err;

    long n = fread(buf, 1, len, f);
    if (n != len)
        goto err;
    buf[len] = '\0';
    fclose(f);

    return buf;

err:
    fclose(f);
    return NULL;
}

static uint32_t current_fill_color = 0xFF000000; // default black, ARGB

// Convert HSL to RGB
static void
hsl_to_rgb(float h, float s, float l, int *r, int *g, int *b)
{
    h = fmodf(h, 360.0f);
    if (h < 0)
        h += 360.0f;
    s = s < 0 ? 0 : (s > 1 ? 1 : s);
    l = l < 0 ? 0 : (l > 1 ? 1 : l);

    float c = (1 - fabsf(2 * l - 1)) * s;
    float x = c * (1 - fabsf(fmodf(h / 60.0f, 2) - 1));
    float m = l - c / 2;

    float rf, gf, bf;
    if (h < 60) {
        rf = c; gf = x; bf = 0;
    } else if (h < 120) {
        rf = x; gf = c; bf = 0;
    } else if (h < 180) {
        rf = 0; gf = c; bf = x;
    } else if (h < 240) {
        rf = 0; gf = x; bf = c;
    } else if (h < 300) {
        rf = x; gf = 0; bf = c;
    } else {
        rf = c; gf = 0; bf = x;
    }

    *r = (int) ((rf + m) * 255);
    *g = (int) ((gf + m) * 255);
    *b = (int) ((bf + m) * 255);
}

// Parse color string to uint32_t (ARGB format)
static uint32_t
parse_color(const char *str)
{
    int r = 0;
    int g = 0;
    int b = 0;
    float a = 1.0f;

    if (strncmp(str, "hsla(", 5) == 0) {
        float h, s, l;
        sscanf(str, "hsla(%f,%f%%,%f%%,%f)", &h, &s, &l, &a);
        hsl_to_rgb(h, s / 100.0f, l / 100.0f, &r, &g, &b);
    } else if (strncmp(str, "hsl(", 4) == 0) {
        float h, s, l;
        sscanf(str, "hsl(%f,%f%%,%f%%)", &h, &s, &l);
        hsl_to_rgb(h, s / 100.0f, l / 100.0f, &r, &g, &b);
    } else if (strncmp(str, "rgba(", 5) == 0) {
        sscanf(str, "rgba(%d,%d,%d,%f)", &r, &g, &b, &a);
    } else if (strncmp(str, "rgb(", 4) == 0) {
        sscanf(str, "rgb(%d,%d,%d)", &r, &g, &b);
    } else if (str[0] == '#') {
        if (strlen(str) == 7) {
            sscanf(str + 1, "%02x%02x%02x", &r, &g, &b);
        } else if (strlen(str) == 4) {
            sscanf(str + 1, "%1x%1x%1x", &r, &g, &b);
            r *= 17;
            g *= 17;
            b *= 17; // expand #RGB to #RRGGBB
        }
    } else {
        // Basic color names
        if (strcmp(str, "black") == 0) {
            r = 0;
            g = 0;
            b = 0;
        } else if (strcmp(str, "white") == 0) {
            r = 255;
            g = 255;
            b = 255;
        } else if (strcmp(str, "red") == 0) {
            r = 255;
            g = 0;
            b = 0;
        } else if (strcmp(str, "green") == 0) {
            r = 0;
            g = 128;
            b = 0;
        } else if (strcmp(str, "blue") == 0) {
            r = 0;
            g = 0;
            b = 255;
        } else {
            r = 0;
            g = 0;
            b = 0;
        }
    }

    uint8_t alpha = (uint8_t) (a * 255);
    return (alpha << 24) | (r << 16) | (g << 8) | b;
}

// fillStyle getter
static JSValue
js_fillStyle_get(JSContext *ctx, JSValueConst this_val, int argc,
    JSValueConst *argv)
{
    // Return current color as string (simplified - just return black)
    return JS_NewString(ctx, "#000000");
}

// fillStyle setter
static JSValue
js_fillStyle_set(JSContext *ctx, JSValueConst this_val, int argc,
    JSValueConst *argv)
{
    if (argc > 0) {
        const char *str = JS_ToCString(ctx, argv[0]);
        if (str) {
            current_fill_color = parse_color(str);
            JS_FreeCString(ctx, str);
        }
    }
    return JS_UNDEFINED;
}

static float global_alpha = 1.0f;

static JSValue
js_globalAlpha_get(JSContext *ctx, JSValueConst this_val, int argc,
    JSValueConst *argv)
{
    return JS_NewFloat64(ctx, global_alpha);
}

static JSValue
js_globalAlpha_set(JSContext *ctx, JSValueConst this_val, int argc,
    JSValueConst *argv)
{
    if (argc > 0) {
        double a;
        JS_ToFloat64(ctx, &a, argv[0]);
        if (a < 0.0)
            a = 0.0;
        if (a > 1.0)
            a = 1.0;
        global_alpha = (float) a;
    }
    return JS_UNDEFINED;
}

static JSValue
js_R(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double r, g, b, a = 1.0;
    JS_ToFloat64(ctx, &r, argv[0]);
    JS_ToFloat64(ctx, &g, argv[1]);
    JS_ToFloat64(ctx, &b, argv[2]);
    if (argc > 3)
        JS_ToFloat64(ctx, &a, argv[3]);

    char buf[64];
    snprintf(buf, sizeof(buf), "rgba(%.0f,%.0f,%.0f,%g)", r, g, b, a);
    return JS_NewString(ctx, buf);
}

static JSValue
js_canvas_width_get(JSContext *ctx, JSValueConst this_val, int argc,
    JSValueConst *argv)
{
    return JS_NewInt32(ctx, 1920);
}

static JSValue
js_canvas_width_set(JSContext *ctx, JSValueConst this_val, int argc,
    JSValueConst *argv)
{
    // Setting width clears the canvas (even to same value)
    gfx_clear(0xFFFFFF);
    return JS_UNDEFINED;
}

static JSValue
js_fillRect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    double x, y, w, h;
    JS_ToFloat64(ctx, &x, argv[0]);
    JS_ToFloat64(ctx, &y, argv[1]);
    JS_ToFloat64(ctx, &w, argv[2]);
    JS_ToFloat64(ctx, &h, argv[3]);

    // Ensure minimum 1 pixel if value is positive
    int iw = (int) w;
    int ih = (int) h;
    if (w > 0 && iw < 1)
        iw = 1;
    if (h > 0 && ih < 1)
        ih = 1;

    // Apply globalAlpha to the color's alpha channel
    uint8_t a = (current_fill_color >> 24) & 0xFF;
    a = (uint8_t) (a * global_alpha);
    uint32_t color = (a << 24) | (current_fill_color & 0x00FFFFFF);

    gfx_fill_rect((int) x, (int) y, (int) iw, (int) ih, color);
    return JS_UNDEFINED;
}

static JSValue
js_clearRect(JSContext *ctx, JSValueConst this_val, int argc,
    JSValueConst *argv)
{
    double x, y, w, h;
    JS_ToFloat64(ctx, &x, argv[0]);
    JS_ToFloat64(ctx, &y, argv[1]);
    JS_ToFloat64(ctx, &w, argv[2]);
    JS_ToFloat64(ctx, &h, argv[3]);

    // Clear to white (background color)
    gfx_fill_rect((int) x, (int) y, (int) w, (int) h, 0xFFFFFFFF);
    return JS_UNDEFINED;
}

static void
setup_globals(JSContext *ctx)
{
    JSValue global = JS_GetGlobalObject(ctx);

    // S = Math.sin, C = Math.cos, T = Math.tan
    JSValue math = JS_GetPropertyStr(ctx, global, "Math");
    JS_SetPropertyStr(ctx, global, "S", JS_GetPropertyStr(ctx, math, "sin"));
    JS_SetPropertyStr(ctx, global, "C", JS_GetPropertyStr(ctx, math, "cos"));
    JS_SetPropertyStr(ctx, global, "T", JS_GetPropertyStr(ctx, math, "tan"));
    JS_FreeValue(ctx, math);

    // R(r,g,b,a) - returns "rgba(r,g,b,a)" string
    JS_SetPropertyStr(ctx, global, "R", JS_NewCFunction(ctx, js_R, "R", 4));

    // c = canvas object with width/height
    JSValue canvas = JS_NewObject(ctx);
    /* JS_SetPropertyStr(ctx, canvas, "width", JS_NewInt32(ctx, 1920)); */
    JS_SetPropertyStr(ctx, canvas, "height", JS_NewInt32(ctx, 1080));
    JS_SetPropertyStr(ctx, global, "c", canvas);

    // x = 2D context (we'll expand this next)
    JSValue context = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, context, "fillRect",
        JS_NewCFunction(ctx, js_fillRect, "fillRect", 4));
    JS_SetPropertyStr(ctx, context, "clearRect",
        JS_NewCFunction(ctx, js_clearRect, "clearRect", 4));

    JSValue getter = JS_NewCFunction(ctx, js_fillStyle_get, "get fillStyle", 0);
    JSValue setter = JS_NewCFunction(ctx, js_fillStyle_set, "set fillStyle", 1);
    JSAtom atom = JS_NewAtom(ctx, "fillStyle");
    JS_DefinePropertyGetSet(ctx, context, atom, getter, setter, 0);
    JS_FreeAtom(ctx, atom);

    getter = JS_NewCFunction(ctx, js_globalAlpha_get, "get globalAlpha", 0);
    setter = JS_NewCFunction(ctx, js_globalAlpha_set, "set globalAlpha", 1);
    atom = JS_NewAtom(ctx, "globalAlpha");
    JS_DefinePropertyGetSet(ctx, context, atom, getter, setter, 0);
    JS_FreeAtom(ctx, atom);

    getter = JS_NewCFunction(ctx, js_canvas_width_get, "get canvas width", 0);
    setter = JS_NewCFunction(ctx, js_canvas_width_set, "set canvas width", 1);
    atom = JS_NewAtom(ctx, "width");
    JS_DefinePropertyGetSet(ctx, canvas, atom, getter, setter, 0);
    JS_FreeAtom(ctx, atom);

    JS_SetPropertyStr(ctx, global, "x", context);

    JS_FreeValue(ctx, global);
}

static bool
check_exception(JSValue value, JSContext *ctx)
{
    if (JS_IsException(value)) {
        JSValue exc = JS_GetException(ctx);
        const char *str = JS_ToCString(ctx, exc);
        fprintf(stderr, "error: %s\n", str);
        JS_FreeCString(ctx, str);
        JSValue stack = JS_GetPropertyStr(ctx, exc, "stack");
        if (!JS_IsUndefined(stack)) {
            const char *stack_str = JS_ToCString(ctx, stack);
            fprintf(stderr, "%s", stack_str);
            JS_FreeCString(ctx, stack_str);
        }
        JS_FreeValue(ctx, stack);
        JS_FreeValue(ctx, exc);
        return true;
    }
    return false;
}

static double
get_time(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int
main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <file.js>\n", argv[0]);
        return 1;
    }

    char *code = read_file(argv[1]);
    if (!code) {
        fprintf(stderr, "error: could not read '%s'\n", argv[1]);
        return 1;
    }

    JSRuntime *rt = JS_NewRuntime();
    if (!rt) {
        fprintf(stderr, "error: could not initialize JS runtime\n");
        return 1;
    }

    JSContext *ctx = JS_NewContext(rt);
    if (!ctx) {
        fprintf(stderr, "error: could not initialize JS context\n");
        JS_FreeRuntime(rt);
        return 1;
    }

    gfx_init(1920, 1080, "Dwitter Player");

    setup_globals(ctx);

    char *wrapped;
    if (asprintf(&wrapped, "function u(t) { %s }", code) == -1)
        goto cleanup;

    JSValue result = JS_Eval(ctx, wrapped, strlen(wrapped), argv[1],
        JS_EVAL_TYPE_GLOBAL);
    free(wrapped);

    if (check_exception(result, ctx)) {
        JS_FreeValue(ctx, result);
        goto cleanup;
    }
    JS_FreeValue(ctx, result);

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue u_func = JS_GetPropertyStr(ctx, global, "u");

    double start_time = get_time();

    gfx_clear(0xffffff);

    while (!gfx_poll_quit()) {
        double t = get_time() - start_time;

        JSValue t_val = JS_NewFloat64(ctx, t);
        JSValue ret = JS_Call(ctx, u_func, global, 1, &t_val);
        JS_FreeValue(ctx, t_val);

        if (check_exception(ret, ctx)) {
            JS_FreeValue(ctx, ret);
            break;
        }
        JS_FreeValue(ctx, ret);

        gfx_present();
    }

    JS_FreeValue(ctx, global);
    JS_FreeValue(ctx, u_func);

cleanup:
    gfx_cleanup();

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    free(code);

    return 0;
}
