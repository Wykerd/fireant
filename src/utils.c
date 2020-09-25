#include "fireant.h"
#include "utils.h"

/**
 * Personal notes:
 * 
 * JS_DupValue  -> Increases reference counter to object (does not duplicate!)
 * JS_FreeValue -> Decreases reference counter to object (does not release memory!)
 */

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

JSValue fa_init_promise (JSContext *ctx, fa_promise_t *p) {
    JSValue rfuncs[2];
    p->p = JS_NewPromiseCapability(ctx, rfuncs);
    if (JS_IsException(p->p))
        return JS_EXCEPTION;
    p->rfuncs[0] = JS_DupValue(ctx, rfuncs[0]);
    p->rfuncs[1] = JS_DupValue(ctx, rfuncs[1]);
    return JS_DupValue(ctx, p->p);
}

int fa_is_promise_pending (JSContext *ctx, fa_promise_t *p) {
    return !JS_IsUndefined(p->p);
}

void fa_free_promise (JSContext *ctx, fa_promise_t *p) {
    // remove reference to promise in the runtime if used in script it should still remain in memory!
    JS_FreeValue(ctx, p->rfuncs[0]);
    JS_FreeValue(ctx, p->rfuncs[1]);
    JS_FreeValue(ctx, p->p);
}

void fa_free_promise_rt (JSRuntime *rt, fa_promise_t *p) {
    JS_FreeValueRT(rt, p->rfuncs[0]);
    JS_FreeValueRT(rt, p->rfuncs[1]);
    JS_FreeValueRT(rt, p->p);
}

void fa_clear_promise (JSContext *ctx, fa_promise_t *p) {
    p->p = JS_UNDEFINED;
    p->rfuncs[0] = JS_UNDEFINED;
    p->rfuncs[1] = JS_UNDEFINED;
}

void fa_mark_promise (JSRuntime *rt, fa_promise_t *p, JS_MarkFunc *mark_func) {
    JS_MarkValue(rt, p->p, mark_func);
    JS_MarkValue(rt, p->rfuncs[0], mark_func);
    JS_MarkValue(rt, p->rfuncs[1], mark_func);
}

void fa_settle_promise (JSContext *ctx, fa_promise_t *p, int is_reject, int argc, JSValueConst *argv) {
    JSValue ret = JS_Call(ctx, p->rfuncs[is_reject], JS_UNDEFINED, argc, argv);
    for (int i = 0; i < argc; i++)
        JS_FreeValue(ctx, argv[i]);
    JS_FreeValue(ctx, ret); /* XXX: what to do if exception ? */
    JS_FreeValue(ctx, p->rfuncs[0]);
    JS_FreeValue(ctx, p->rfuncs[1]);
    TJS_FreePromise(ctx, p);
}

void fa_resolve_promise (JSContext *ctx, fa_promise_t *p, int argc, JSValueConst *argv) {
    TJS_SettlePromise(ctx, p, 0, argc, argv);
}

void fa_reject_promise (JSContext *ctx, fa_promise_t *p, int argc, JSValueConst *argv) {
    TJS_SettlePromise(ctx, p, 1, argc, argv);
}

static inline JSValue fa_settled_promise(JSContext *ctx, int is_reject, int argc, JSValueConst *argv) {
    JSValue promise, resolving_funcs[2], ret;

    promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (JS_IsException(promise))
        return JS_EXCEPTION;

    ret = JS_Call(ctx, resolving_funcs[is_reject], JS_UNDEFINED, argc, argv);

    for (int i = 0; i < argc; i++)
        JS_FreeValue(ctx, argv[i]);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);

    return promise;
}

// TJS_NewResolvedPromise
JSValue fa_resolved_promise (JSContext *ctx, int argc, JSValueConst *argv) {
    return fa_settled_promise(ctx, 0, argc, argv);
}

// TJS_NewRejectedPromise
JSValue fa_rejected_promise(JSContext *ctx, int argc, JSValueConst *argv) {
    return fa_settled_promise(ctx, 1, argc, argv);
}