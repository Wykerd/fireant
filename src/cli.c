#include "fireant.h"
#include <stdio.h>
#include <string.h>

int main () {
    printf("FireAnt Version: %s\nQuickJS Version: %s\n\n", fa_get_ver_str(), fa_get_qjs_ver());

    fa_runtime_t *rt = fa_new_runtime();

    char *script = "1 + 2";

    JSValue eval = fa_eval_buf(fa_get_context(rt), script, strlen(script), "<input>", JS_EVAL_TYPE_MODULE);

    const char *e = JS_ToCString(fa_get_context(rt), eval);

    printf("Value: %s\n", e);

    fa_run(rt);
}