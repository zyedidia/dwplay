#pragma once

#include "quickjs.h"

struct Context2D;

void js_canvas_init(JSContext *ctx);
JSValue js_canvas_new(JSContext *ctx, unsigned width, unsigned height);
struct Context2D *js_canvas_get_context2d(JSContext *ctx, JSValue canvas);
