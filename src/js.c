#include "canvas.h"
#include "quickjs.h"

#include <math.h>
#include <string.h>

static JSClassID canvas_class_id;
static JSClassID ctx2d_class_id;

// ============================================================================
// Binding helper macros
// ============================================================================

// Get opaque pointer with null check
#define GET_OPAQUE(var, this_val, type, class_id) \
    type *var = JS_GetOpaque(this_val, class_id); \
    if (!var) return JS_EXCEPTION

// Property getter returning double
#define PROP_DOUBLE_GET(name, type, class_id, getter) \
    static JSValue name##_get(JSContext *ctx, JSValueConst this_val) { \
        GET_OPAQUE(opaque, this_val, type, class_id); \
        return JS_NewFloat64(ctx, getter(opaque)); \
    }

// Property setter taking double
#define PROP_DOUBLE_SET(name, type, class_id, setter) \
    static JSValue name##_set(JSContext *ctx, JSValueConst this_val, JSValueConst val) { \
        GET_OPAQUE(opaque, this_val, type, class_id); \
        double v; \
        if (JS_ToFloat64(ctx, &v, val)) return JS_EXCEPTION; \
        setter(opaque, v); \
        return JS_UNDEFINED; \
    }

// Property getter/setter pair for double
#define PROP_DOUBLE(name, type, class_id, getter, setter) \
    PROP_DOUBLE_GET(name, type, class_id, getter) \
    PROP_DOUBLE_SET(name, type, class_id, setter)

// Property getter returning uint32
#define PROP_UINT32_GET(name, type, class_id, getter) \
    static JSValue name##_get(JSContext *ctx, JSValueConst this_val) { \
        GET_OPAQUE(opaque, this_val, type, class_id); \
        return JS_NewUint32(ctx, getter(opaque)); \
    }

// Property setter taking uint32
#define PROP_UINT32_SET(name, type, class_id, setter) \
    static JSValue name##_set(JSContext *ctx, JSValueConst this_val, JSValueConst val) { \
        GET_OPAQUE(opaque, this_val, type, class_id); \
        uint32_t v; \
        if (JS_ToUint32(ctx, &v, val)) return JS_EXCEPTION; \
        setter(opaque, v); \
        return JS_UNDEFINED; \
    }

// Property getter/setter pair for uint32
#define PROP_UINT32(name, type, class_id, getter, setter) \
    PROP_UINT32_GET(name, type, class_id, getter) \
    PROP_UINT32_SET(name, type, class_id, setter)

// Method with 4 double arguments (x, y, w, h pattern)
#define METHOD_RECT(name, type, class_id, func) \
    static JSValue name(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) { \
        (void)argc; \
        GET_OPAQUE(opaque, this_val, type, class_id); \
        double x, y, w, h; \
        if (JS_ToFloat64(ctx, &x, argv[0])) return JS_EXCEPTION; \
        if (JS_ToFloat64(ctx, &y, argv[1])) return JS_EXCEPTION; \
        if (JS_ToFloat64(ctx, &w, argv[2])) return JS_EXCEPTION; \
        if (JS_ToFloat64(ctx, &h, argv[3])) return JS_EXCEPTION; \
        func(opaque, x, y, w, h); \
        return JS_UNDEFINED; \
    }

// Method with no arguments
#define METHOD_VOID(name, type, class_id, func) \
    static JSValue name(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) { \
        (void)ctx; (void)argc; (void)argv; \
        GET_OPAQUE(opaque, this_val, type, class_id); \
        func(opaque); \
        return JS_UNDEFINED; \
    }

// ============================================================================
// Color parsing
// ============================================================================

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

    *r = (int)((rf + m) * 255);
    *g = (int)((gf + m) * 255);
    *b = (int)((bf + m) * 255);
}

static uint32_t
parse_color(const char *str)
{
    int r = 0, g = 0, b = 0;
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
            b *= 17;
        }
    } else if (strcmp(str, "black") == 0) {
        r = g = b = 0;
    } else if (strcmp(str, "white") == 0) {
        r = g = b = 255;
    } else if (strcmp(str, "red") == 0) {
        r = 255; g = b = 0;
    } else if (strcmp(str, "green") == 0) {
        g = 128; r = b = 0;
    } else if (strcmp(str, "blue") == 0) {
        b = 255; r = g = 0;
    }

    uint8_t alpha = (uint8_t)(a * 255);
    return (alpha << 24) | (r << 16) | (g << 8) | b;
}

// ============================================================================
// Context2D JS bindings
// ============================================================================

static void
js_ctx2d_finalizer(JSRuntime *rt, JSValue val)
{
    (void)rt;
    (void)val;
    // Context2D is owned by Canvas, so don't free here
}

static JSClassDef ctx2d_class = {
    "CanvasRenderingContext2D",
    .finalizer = js_ctx2d_finalizer,
};

// fillStyle is special (string ↔ color conversion)
static JSValue
js_ctx2d_fillStyle_get(JSContext *ctx, JSValueConst this_val)
{
    (void)this_val;
    // TODO: convert stored color back to string
    return JS_NewString(ctx, "#000000");
}

static JSValue
js_ctx2d_fillStyle_set(JSContext *ctx, JSValueConst this_val, JSValueConst val)
{
    GET_OPAQUE(ctx2d, this_val, struct Context2D, ctx2d_class_id);
    const char *str = JS_ToCString(ctx, val);
    if (str) {
        ctx2d_fillStyle_set(ctx2d, parse_color(str));
        JS_FreeCString(ctx, str);
    }
    return JS_UNDEFINED;
}

// Simple properties using macros
PROP_DOUBLE(js_ctx2d_globalAlpha, struct Context2D, ctx2d_class_id,
    ctx2d_globalAlpha_get, ctx2d_globalAlpha_set)
PROP_DOUBLE(js_ctx2d_lineWidth, struct Context2D, ctx2d_class_id,
    ctx2d_lineWidth_get, ctx2d_lineWidth_set)

// Rect methods using macros
METHOD_RECT(js_ctx2d_fillRect, struct Context2D, ctx2d_class_id, ctx2d_fillRect)
METHOD_RECT(js_ctx2d_clearRect, struct Context2D, ctx2d_class_id, ctx2d_clearRect)

// Path methods
METHOD_VOID(js_ctx2d_beginPath, struct Context2D, ctx2d_class_id, ctx2d_beginPath)
METHOD_VOID(js_ctx2d_stroke, struct Context2D, ctx2d_class_id, ctx2d_stroke)

// arc(x, y, radius, startAngle, endAngle, counterclockwise)
static JSValue
js_ctx2d_arc(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    GET_OPAQUE(ctx2d, this_val, struct Context2D, ctx2d_class_id);
    double x, y, r, startAngle, endAngle;
    if (JS_ToFloat64(ctx, &x, argv[0])) return JS_EXCEPTION;
    if (JS_ToFloat64(ctx, &y, argv[1])) return JS_EXCEPTION;
    if (JS_ToFloat64(ctx, &r, argv[2])) return JS_EXCEPTION;
    if (JS_ToFloat64(ctx, &startAngle, argv[3])) return JS_EXCEPTION;
    if (JS_ToFloat64(ctx, &endAngle, argv[4])) return JS_EXCEPTION;
    int ccw = (argc > 5) ? JS_ToBool(ctx, argv[5]) : 0;
    ctx2d_arc(ctx2d, x, y, r, startAngle, endAngle, ccw);
    return JS_UNDEFINED;
}

// scale(x, y)
static JSValue
js_ctx2d_scale(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)argc;
    GET_OPAQUE(ctx2d, this_val, struct Context2D, ctx2d_class_id);
    double x, y;
    if (JS_ToFloat64(ctx, &x, argv[0])) return JS_EXCEPTION;
    if (JS_ToFloat64(ctx, &y, argv[1])) return JS_EXCEPTION;
    ctx2d_scale(ctx2d, x, y);
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry ctx2d_proto_funcs[] = {
    JS_CGETSET_DEF("fillStyle", js_ctx2d_fillStyle_get, js_ctx2d_fillStyle_set),
    JS_CGETSET_DEF("globalAlpha", js_ctx2d_globalAlpha_get, js_ctx2d_globalAlpha_set),
    JS_CGETSET_DEF("lineWidth", js_ctx2d_lineWidth_get, js_ctx2d_lineWidth_set),
    JS_CFUNC_DEF("fillRect", 4, js_ctx2d_fillRect),
    JS_CFUNC_DEF("clearRect", 4, js_ctx2d_clearRect),
    JS_CFUNC_DEF("beginPath", 0, js_ctx2d_beginPath),
    JS_CFUNC_DEF("arc", 5, js_ctx2d_arc),
    JS_CFUNC_DEF("stroke", 0, js_ctx2d_stroke),
    JS_CFUNC_DEF("scale", 2, js_ctx2d_scale),
};

// ============================================================================
// Canvas JS bindings
// ============================================================================

static void
js_canvas_finalizer(JSRuntime *rt, JSValue val)
{
    (void)rt;
    struct Canvas *canvas = JS_GetOpaque(val, canvas_class_id);
    canvas_destroy(canvas);
}

static JSClassDef canvas_class = {
    "HTMLCanvasElement",
    .finalizer = js_canvas_finalizer,
};

// Canvas properties using macros
PROP_UINT32(js_canvas_width, struct Canvas, canvas_class_id,
    canvas_width_get, canvas_width_set)
PROP_UINT32(js_canvas_height, struct Canvas, canvas_class_id,
    canvas_height_get, canvas_height_set)

// getContext is special
static JSValue
js_canvas_getContext(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)argc;
    GET_OPAQUE(canvas, this_val, struct Canvas, canvas_class_id);

    const char *type = JS_ToCString(ctx, argv[0]);
    if (!type)
        return JS_EXCEPTION;

    struct Context2D *ctx2d = canvas_getContext(canvas, type);
    JS_FreeCString(ctx, type);

    if (!ctx2d)
        return JS_NULL;

    JSValue obj = JS_NewObjectClass(ctx, ctx2d_class_id);
    if (JS_IsException(obj))
        return obj;

    JS_SetOpaque(obj, ctx2d);
    JS_SetPropertyFunctionList(ctx, obj, ctx2d_proto_funcs,
        sizeof(ctx2d_proto_funcs) / sizeof(ctx2d_proto_funcs[0]));

    return obj;
}

static const JSCFunctionListEntry canvas_proto_funcs[] = {
    JS_CGETSET_DEF("width", js_canvas_width_get, js_canvas_width_set),
    JS_CGETSET_DEF("height", js_canvas_height_get, js_canvas_height_set),
    JS_CFUNC_DEF("getContext", 1, js_canvas_getContext),
};

// ============================================================================
// Public API
// ============================================================================

JSValue
js_canvas_new(JSContext *ctx, unsigned width, unsigned height)
{
    struct Canvas *canvas = canvas_new(width, height);
    if (!canvas)
        return JS_EXCEPTION;

    JSValue obj = JS_NewObjectClass(ctx, canvas_class_id);
    if (JS_IsException(obj)) {
        canvas_destroy(canvas);
        return obj;
    }

    JS_SetOpaque(obj, canvas);
    JS_SetPropertyFunctionList(ctx, obj, canvas_proto_funcs,
        sizeof(canvas_proto_funcs) / sizeof(canvas_proto_funcs[0]));

    return obj;
}

void
js_canvas_init(JSContext *ctx)
{
    JS_NewClassID(JS_GetRuntime(ctx), &canvas_class_id);
    JS_NewClassID(JS_GetRuntime(ctx), &ctx2d_class_id);
    JS_NewClass(JS_GetRuntime(ctx), canvas_class_id, &canvas_class);
    JS_NewClass(JS_GetRuntime(ctx), ctx2d_class_id, &ctx2d_class);
}

struct Context2D *
js_canvas_get_context2d(JSContext *ctx, JSValue canvas_val)
{
    (void)ctx;
    struct Canvas *canvas = JS_GetOpaque(canvas_val, canvas_class_id);
    if (!canvas)
        return NULL;
    return canvas_getContext(canvas, "2d");
}
