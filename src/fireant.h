#ifndef FIREANT_H
#define FIREANT_H

#include <quickjs.h>
#include <uv.h>

struct fa_runtime_s {
    JSRuntime *rt;
    JSContext *ctx;
    uv_loop_t loop;
    struct {
        uv_check_t check;
        uv_idle_t idle;
        uv_prepare_t prepare;
        uv_async_t stop;
    } event_handles;
    int is_worker;
};

typedef struct fa_runtime_s fa_runtime_t;

fa_runtime_t *fa_new_runtime (void);
void fa_free_runtime (fa_runtime_t *rt);

void fa_setup_args (int argc, char **argv);
void fa_run (fa_runtime_t *rt);
void fa_stop (fa_runtime_t *rt);

JSContext *fa_get_context (fa_runtime_t *rt);
fa_runtime_t *fa_get_runtime (JSContext *ctx);

JSValue fa_eval_file (JSContext *ctx, const char *filename, int eval_flags, int is_main, char *override_filename);

JSValue fa_eval_binary (JSContext *ctx, const uint8_t *buf, size_t buf_len);

JSValue fa_eval_buf (
    JSContext *ctx, 
    const void *buf, 
    int buf_len,
    const char *filename, 
    int eval_flags
);

const char *fa_get_ver_str (void);
const int fa_get_maj_ver (void);
const int fa_get_min_ver (void);
const int fa_get_patch_ver (void);
const char *fa_get_qjs_ver (void);

#endif