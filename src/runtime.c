#include "runtime.h"
#include "utils.h"
#include "modules.h"
#include <stdlib.h>
#include <string.h>
#include <quickjs/quickjs.h>
#include <assert.h>

static const char fa_sig[] = "FaBC";

static void fa_uv_stop (uv_async_t *handle) {
    fa_runtime_t *qrt = handle->data;
    assert(qrt != NULL);
    /* Stop the loop and finish running */
    uv_stop(&qrt->loop);
}

fa_runtime_t *fa_new_runtime (void) {
    return fa_new_runtime_impl(0);
}

fa_runtime_t *fa_new_runtime_impl (int is_worker) {
    fa_runtime_t *qrt = malloc(sizeof(fa_runtime_t));

    memset(qrt, 0, sizeof(fa_runtime_t));

    /* Create QuickJS runtime and context */
    qrt->rt = JS_NewRuntime();

    FA_NULL_RETURN(qrt->rt);

    qrt->ctx = JS_NewContext(qrt->rt);

    FA_NULL_RETURN(qrt->ctx);

    /* Make the extended runtime accesable from the QuickJS runtime and context */
    JS_SetRuntimeOpaque(qrt->rt, qrt);
    JS_SetContextOpaque(qrt->ctx, qrt);

    /* Add QuickJS math extensions */
    JS_AddIntrinsicBigFloat(qrt->ctx);
    JS_AddIntrinsicBigDecimal(qrt->ctx);
    JS_AddIntrinsicOperators(qrt->ctx);
    JS_EnableBignumExt(qrt->ctx, 1);

    qrt->is_worker = is_worker;

    /* Create the main event loop */
    FA_CHECK(uv_loop_init(&qrt->loop) == 0);

    /* handle which runs the job queue */
    FA_CHECK(uv_prepare_init(&qrt->loop, &qrt->event_handles.prepare) == 0);
    qrt->event_handles.prepare.data = qrt;

    /* handle to prevent the loop from blocking for i/o when there are pending jobs. */
    FA_CHECK(uv_idle_init(&qrt->loop, &qrt->event_handles.idle) == 0);
    qrt->event_handles.idle.data = qrt;

    /* handle which runs the job queue */
    FA_CHECK(uv_check_init(&qrt->loop, &qrt->event_handles.check) == 0);
    qrt->event_handles.check.data = qrt;

    /* handle for stopping this runtime (also works from another thread) */
    FA_CHECK(uv_async_init(&qrt->loop, &qrt->event_handles.stop, fa_uv_stop) == 0);
    qrt->event_handles.stop.data = qrt;

    /* loader for ES6 modules */
    JS_SetModuleLoaderFunc(qrt->rt, NULL, fa_module_loader, qrt);

    /* unhandled promise rejection tracker */
    // JS_SetHostPromiseRejectionTracker(qrt->rt, handler, NULL);

    // Builtins are not automatically loaded! The embedder can decide what modules to expose to the user.

    return qrt;
}

void fa_free_runtime (fa_runtime_t *rt) {
    /* Close all loop handles. */
    uv_close((uv_handle_t *) &rt->event_handles.prepare, NULL);
    uv_close((uv_handle_t *) &rt->event_handles.idle, NULL);
    uv_close((uv_handle_t *) &rt->event_handles.check, NULL);
    uv_close((uv_handle_t *) &rt->event_handles.stop, NULL);

    JS_FreeContext(rt->ctx);
    JS_FreeRuntime(rt->rt);

    /* Cleanup loop. All handles should be closed. */
    int closed = 0;
    for (int i = 0; i < 5; i++) {
        if (uv_loop_close(&rt->loop) == 0) {
            closed = 1;
            break;
        }
        uv_run(&rt->loop, UV_RUN_NOWAIT);
    };

#ifdef DEBUG
    if (!closed)
        uv_print_all_handles(&qrt->loop, stderr);
#endif

    assert(closed);

    free(rt);
}


JSContext *fa_get_context (fa_runtime_t *rt) {
    return rt->ctx;
}

fa_runtime_t *fa_get_runtime (JSContext *ctx) {
    return JS_GetContextOpaque(ctx);
}

static void fa_uv_idle_cb(uv_idle_t *handle) {
    // Do nothing just prevent i/o polling
}

static void fa_uv_maybe_idle(fa_runtime_t *qrt) {
    /* Idle until no jobs are pending */
    /* UV doesn't block for i/o polling when there are active idlers */
    if (JS_IsJobPending(qrt->rt))
        assert(uv_idle_start(&qrt->event_handles.idle, fa_uv_idle_cb) == 0);
    else
        assert(uv_idle_stop(&qrt->event_handles.idle) == 0);
}

static void fa_uv_prepare_cb(uv_prepare_t *handle) {
    fa_runtime_t *qrt = handle->data;
    assert(qrt != NULL);

    /* Before polling i/o idle if active jobs still exist */
    fa_uv_maybe_idle(qrt);
}

void fa_execute_jobs (JSContext *ctx) {
    // job context
    JSContext *ctx1;
    int err;

    /* execute the pending jobs */
    for (;;) {
        err = JS_ExecutePendingJob(JS_GetRuntime(ctx), &ctx1);
        if (err <= 0) {
            if (err < 0)
                fa_dump_error(ctx1);
            break;
        }
    }
}

static void fa_uv_check_cb(uv_check_t *handle) {
    fa_runtime_t *qrt = handle->data;
    assert(qrt != NULL);

    /* After I/O was polled execute all the pending jobs and idle untill they are done */
    fa_execute_jobs(qrt->ctx);

    fa_uv_maybe_idle(qrt);
}

void fa_run (fa_runtime_t *rt) {
    assert(uv_prepare_start(&rt->event_handles.prepare, fa_uv_prepare_cb) == 0);
    /* remove reference to the handle so that the loop exits itself */
    uv_unref((uv_handle_t *) &rt->event_handles.prepare);
    assert(uv_check_start(&rt->event_handles.check, fa_uv_check_cb) == 0);
    uv_unref((uv_handle_t *) &rt->event_handles.check);

    /* Async handle keeps worker loops alive even when they do nothing. */
    if (!rt->is_worker)
        uv_unref((uv_handle_t *) &rt->event_handles.stop);

    fa_uv_maybe_idle(rt);

    uv_run(&rt->loop, UV_RUN_DEFAULT);
}

void fa_stop (fa_runtime_t *rt) {
    assert(rt != NULL);
    /* Trigger the async callback which stops the loop and exits fa_run */
    uv_async_send(&rt->event_handles.stop);
}

JSValue fa_eval_buf (
    JSContext *ctx, 
    const void *buf, 
    int buf_len,
    const char *filename, 
    int eval_flags
) {
    JSValue val;

    if ((eval_flags & JS_EVAL_TYPE_MASK) == JS_EVAL_TYPE_MODULE) {
        /* for the modules, we compile then run to be able to set
           import.meta */
        val = JS_Eval(ctx, buf, buf_len, filename,
                      eval_flags | JS_EVAL_FLAG_COMPILE_ONLY);
        if (!JS_IsException(val)) {
            js_module_set_import_meta(ctx, val, 1, 1);
            val = JS_EvalFunction(ctx, val);
        }
    } else {
        val = JS_Eval(ctx, buf, buf_len, filename, eval_flags);
    }
    
    return val;
}

void fa_eval_binary (
    JSContext *ctx, 
    const uint8_t *buf, 
    size_t buf_len, 
    int load_only
) {
    JSValue obj, val;
    obj = JS_ReadObject(ctx, buf, buf_len, JS_READ_OBJ_BYTECODE);
    if (JS_IsException(obj))
        goto exception;
    if (load_only) {
        if (JS_VALUE_GET_TAG(obj) == JS_TAG_MODULE) {
            js_module_set_import_meta(ctx, obj, 0, 0);
        }
    } else {
        if (JS_VALUE_GET_TAG(obj) == JS_TAG_MODULE) {
            if (JS_ResolveModule(ctx, obj) < 0) {
                JS_FreeValue(ctx, obj);
                goto exception;
            }
            js_module_set_import_meta(ctx, obj, 0, 1);
        }
        val = JS_EvalFunction(ctx, obj);
        if (JS_IsException(val)) {
        exception:
            fa_dump_error(ctx);
            exit(1);
        }
        JS_FreeValue(ctx, val);
    }
}

void fa_eval_bin_bundle (
    JSContext *ctx, 
    const uint8_t *buf, 
    size_t buf_len, 
    int load_only
) {
    assert(buf_len > 3);
    
    int sig_ver = memcmp(buf, fa_sig, 4);

    assert(sig_ver == 0);

    size_t cursor = 4;

    size_t module_len;
    
    while (1) {
        memcpy(&module_len, buf + cursor, sizeof(size_t));

        cursor += sizeof(size_t);

        char *mod = malloc(module_len + 1);

        char mod_load_only;

        memcpy(&mod_load_only, buf + cursor, 1);

        cursor++;

        memcpy(mod, buf + cursor, module_len);

        cursor += module_len;

        int end = cursor >= (buf_len - 1);

        fa_eval_binary(ctx, mod, module_len, !end | load_only | mod_load_only);

        free(mod);

        if (end) break;
    };
}
