#include "canvas.h"
#include "gfx.h"
#include "js.h"
#include "quickjs.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CANVAS_WIDTH  1920
#define CANVAS_HEIGHT 1080

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
    if (!buf) {
        fclose(f);
        return NULL;
    }

    if (fread(buf, 1, len, f) != (size_t) len) {
        free(buf);
        fclose(f);
        return NULL;
    }

    buf[len] = '\0';
    fclose(f);
    return buf;
}

static int
hex_digit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

static JSValue
js_unescape(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void) this_val;
    if (argc < 1)
        return JS_NewString(ctx, "");

    const char *str = JS_ToCString(ctx, argv[0]);
    if (!str)
        return JS_EXCEPTION;

    size_t len = strlen(str);
    char *out = malloc(len + 1);
    if (!out) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }

    size_t j = 0;
    for (size_t i = 0; i < len;) {
        if (str[i] == '%' && i + 5 < len && str[i + 1] == 'u') {
            // %uXXXX
            int h1 = hex_digit(str[i + 2]);
            int h2 = hex_digit(str[i + 3]);
            int h3 = hex_digit(str[i + 4]);
            int h4 = hex_digit(str[i + 5]);
            if (h1 >= 0 && h2 >= 0 && h3 >= 0 && h4 >= 0) {
                int cp = (h1 << 12) | (h2 << 8) | (h3 << 4) | h4;
                // Encode as UTF-8
                if (cp < 0x80) {
                    out[j++] = cp;
                } else if (cp < 0x800) {
                    out[j++] = 0xC0 | (cp >> 6);
                    out[j++] = 0x80 | (cp & 0x3F);
                } else {
                    out[j++] = 0xE0 | (cp >> 12);
                    out[j++] = 0x80 | ((cp >> 6) & 0x3F);
                    out[j++] = 0x80 | (cp & 0x3F);
                }
                i += 6;
                continue;
            }
        } else if (str[i] == '%' && i + 2 < len) {
            // %XX
            int h1 = hex_digit(str[i + 1]);
            int h2 = hex_digit(str[i + 2]);
            if (h1 >= 0 && h2 >= 0) {
                out[j++] = (h1 << 4) | h2;
                i += 3;
                continue;
            }
        }
        out[j++] = str[i++];
    }
    out[j] = '\0';

    JS_FreeCString(ctx, str);
    JSValue result = JS_NewString(ctx, out);
    free(out);
    return result;
}

static int
is_safe_char(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '@' || c == '*' || c == '_' ||
        c == '+' || c == '-' || c == '.' || c == '/';
}

static JSValue
js_escape(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void) this_val;
    if (argc < 1)
        return JS_NewString(ctx, "");

    // Handle tagged template literal: escape`...` passes an array as first arg
    JSValue str_val = argv[0];
    int free_str_val = 0;
    if (JS_IsArray(argv[0])) {
        str_val = JS_GetPropertyUint32(ctx, argv[0], 0);
        free_str_val = 1;
    }

    const char *str = JS_ToCString(ctx, str_val);
    if (free_str_val)
        JS_FreeValue(ctx, str_val);
    if (!str)
        return JS_EXCEPTION;

    size_t len = strlen(str);
    // Worst case: each char becomes %uXXXX (6 chars)
    char *out = malloc(len * 6 + 1);
    if (!out) {
        JS_FreeCString(ctx, str);
        return JS_EXCEPTION;
    }

    size_t j = 0;
    for (size_t i = 0; i < len;) {
        unsigned char c = str[i];
        if (is_safe_char(c)) {
            out[j++] = c;
            i++;
        } else if (c < 0x80) {
            // Single byte, encode as %XX
            j += sprintf(out + j, "%%%02X", c);
            i++;
        } else {
            // UTF-8 sequence, decode to code point then encode as %uXXXX
            int cp = 0;
            if ((c & 0xE0) == 0xC0 && i + 1 < len) {
                cp = ((c & 0x1F) << 6) | (str[i + 1] & 0x3F);
                i += 2;
            } else if ((c & 0xF0) == 0xE0 && i + 2 < len) {
                cp = ((c & 0x0F) << 12) | ((str[i + 1] & 0x3F) << 6) |
                    (str[i + 2] & 0x3F);
                i += 3;
            } else if ((c & 0xF8) == 0xF0 && i + 3 < len) {
                cp = ((c & 0x07) << 18) | ((str[i + 1] & 0x3F) << 12) |
                    ((str[i + 2] & 0x3F) << 6) | (str[i + 3] & 0x3F);
                i += 4;
            } else {
                // Invalid UTF-8, just encode the byte
                j += sprintf(out + j, "%%%02X", c);
                i++;
                continue;
            }
            if (cp > 0xFFFF) {
                // Surrogate pair for characters > 0xFFFF
                int hi = 0xD800 + ((cp - 0x10000) >> 10);
                int lo = 0xDC00 + ((cp - 0x10000) & 0x3FF);
                j += sprintf(out + j, "%%u%04X%%u%04X", hi, lo);
            } else {
                j += sprintf(out + j, "%%u%04X", cp);
            }
        }
    }
    out[j] = '\0';

    JS_FreeCString(ctx, str);
    JSValue result = JS_NewString(ctx, out);
    free(out);
    return result;
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

static void
setup_globals(JSContext *ctx, JSValue canvas)
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

    // escape/unescape for dweets using eval(unescape(escape`...`)) compression
    JS_SetPropertyStr(ctx, global, "escape",
        JS_NewCFunction(ctx, js_escape, "escape", 1));
    JS_SetPropertyStr(ctx, global, "unescape",
        JS_NewCFunction(ctx, js_unescape, "unescape", 1));

    // c = canvas, x = 2d context
    JS_SetPropertyStr(ctx, global, "c", JS_DupValue(ctx, canvas));

    JSValue context = JS_GetPropertyStr(ctx, canvas, "getContext");
    JSValue args[] = { JS_NewString(ctx, "2d") };
    JSValue x = JS_Call(ctx, context, canvas, 1, args);
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, context);
    JS_SetPropertyStr(ctx, global, "x", x);

    JS_FreeValue(ctx, global);
}

static bool
check_exception(JSContext *ctx, JSValue val)
{
    if (JS_IsException(val)) {
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
        fprintf(stderr, "error: could not create JS runtime\n");
        free(code);
        return 1;
    }

    JSContext *ctx = JS_NewContext(rt);
    if (!ctx) {
        fprintf(stderr, "error: could not create JS context\n");
        JS_FreeRuntime(rt);
        free(code);
        return 1;
    }

    // Initialize canvas classes and create canvas
    js_canvas_init(ctx);
    JSValue canvas = js_canvas_new(ctx, CANVAS_WIDTH, CANVAS_HEIGHT);
    if (JS_IsException(canvas)) {
        fprintf(stderr, "error: could not create canvas\n");
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        free(code);
        return 1;
    }

    // Get Context2D for rendering
    struct Context2D *ctx2d = js_canvas_get_context2d(ctx, canvas);
    if (!ctx2d) {
        fprintf(stderr, "error: could not get 2D context\n");
        JS_FreeValue(ctx, canvas);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        free(code);
        return 1;
    }

    // Initialize graphics
    if (gfx_init(CANVAS_WIDTH, CANVAS_HEIGHT, "Dwitter Player") < 0) {
        fprintf(stderr, "error: could not initialize graphics\n");
        JS_FreeValue(ctx, canvas);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        free(code);
        return 1;
    }

    setup_globals(ctx, canvas);

    // Wrap code in u(t) function
    char *wrapped;
    if (asprintf(&wrapped, "function u(t) { %s }", code) == -1) {
        fprintf(stderr, "error: out of memory\n");
        goto cleanup;
    }

    JSValue result = JS_Eval(ctx, wrapped, strlen(wrapped), argv[1],
        JS_EVAL_TYPE_GLOBAL);
    free(wrapped);

    if (check_exception(ctx, result)) {
        JS_FreeValue(ctx, result);
        goto cleanup;
    }
    JS_FreeValue(ctx, result);

    // Get u function
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue u_func = JS_GetPropertyStr(ctx, global, "u");

    double start_time = get_time();

    // Main loop
    while (!gfx_poll_quit()) {
        double t = get_time() - start_time;

        // Call u(t)
        JSValue t_val = JS_NewFloat64(ctx, t);
        JSValue ret = JS_Call(ctx, u_func, global, 1, &t_val);
        JS_FreeValue(ctx, t_val);

        if (check_exception(ctx, ret)) {
            JS_FreeValue(ctx, ret);
            break;
        }
        JS_FreeValue(ctx, ret);

        // Update display with PlutoVG surface data
        gfx_update(ctx2d_get_data(ctx2d), ctx2d_get_stride(ctx2d));
        gfx_present();
    }

    JS_FreeValue(ctx, u_func);
    JS_FreeValue(ctx, global);

cleanup:
    gfx_cleanup();
    JS_FreeValue(ctx, canvas);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    free(code);

    return 0;
}
