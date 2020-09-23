#include "fireant.h"
#include "utils.h"

static void js_dump_obj(JSContext *ctx, FILE *f, JSValueConst val)
{
    const char *str;
    
    str = JS_ToCString(ctx, val);
    if (str) {
        fprintf(f, "%s\n", str);
        JS_FreeCString(ctx, str);
    } else {
        fprintf(f, "[exception]\n");
    }
}

void fa_dump_error1(JSContext *ctx, JSValueConst exception_val)
{
    JSValue val;
    int is_error;
    
    is_error = JS_IsError(ctx, exception_val);
    js_dump_obj(ctx, stderr, exception_val);
    if (is_error) {
        val = JS_GetPropertyStr(ctx, exception_val, "stack");
        if (!JS_IsUndefined(val)) {
            js_dump_obj(ctx, stderr, val);
        }
        JS_FreeValue(ctx, val);
    }
}

void fa_dump_error(JSContext *ctx)
{
    JSValue exception_val;
    
    exception_val = JS_GetException(ctx);
    fa_dump_error1(ctx, exception_val);
    JS_FreeValue(ctx, exception_val);
}

int fa_eval_check_exception (JSContext *ctx, JSValue val) {
    int ret;
    if (JS_IsException(val)) {
        fa_dump_error(ctx);
        ret = -1;
    } else {
        ret = 0;
    }
    return ret;
}

int fa_eval_std_free (JSContext *ctx, JSValue val) {
    int ex = fa_eval_check_exception(ctx, val);
    JS_FreeValue(ctx, val);
    return ex;
}