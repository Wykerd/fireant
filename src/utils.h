#ifndef FA_UTILS_H
#define FA_UTILS_H

#include <quickjs.h>

#define FA_CHECK(x) js_likely(x)
#define FA_NULL_CHECK(x) FA_CHECK(x != NULL)
#define FA_CHECK_RETURN(x) if (FA_CHECK(x)) { return NULL; }
#define FA_NULL_RETURN(x) FA_CHECK_RETURN(!FA_NULL_CHECK(x))

struct fa_promise_s {
    JSValue p;
    JSValue rfuncs[2];
};

typedef struct fa_promise_s fa_promise_t;

void fa_dump_error1(JSContext *ctx, JSValueConst exception_val);

void fa_dump_error(JSContext *ctx);

/* Promises */
JSValue fa_init_promise (JSContext *ctx, fa_promise_t *p);
// bool return type
int fa_is_promise_pending (JSContext *ctx, fa_promise_t *p);
// dereference the promise
void fa_free_promise (JSContext *ctx, fa_promise_t *p);
void fa_free_promise_rt (JSRuntime *rt, fa_promise_t *p);
// clear the promise (set it to undefined)
void fa_clear_promise (JSContext *ctx, fa_promise_t *p);
// ?
void fa_mark_promise (JSRuntime *rt, fa_promise_t *p, JS_MarkFunc *mark_func);
// Settle the promise (resolve, reject)
void fa_settle_promise (JSContext *ctx, fa_promise_t *p, int is_reject, int argc, JSValueConst *argv);
// shorthands
void fa_resolve_promise (JSContext *ctx, fa_promise_t *p, int argc, JSValueConst *argv);
void fa_reject_promise (JSContext *ctx, fa_promise_t *p, int argc, JSValueConst *argv);
// presettled promises
JSValue fa_resolved_promise (JSContext *ctx, int argc, JSValueConst *argv);
JSValue fa_rejected_promise(JSContext *ctx, int argc, JSValueConst *argv);

#endif