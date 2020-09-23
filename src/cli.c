#include "fireant.h"
#include <stdio.h>
#include <string.h>

int main () {
    printf("FireAnt Version: %s\nQuickJS Version: %s\n\n", fa_get_ver_str(), fa_get_qjs_ver());

    fa_runtime_t *rt = fa_new_runtime();

    js_init_module_std(fa_get_context(rt), "std");

    char *script = "import yes from '../test.js'; import { print } from 'std'; print('Hello World', 123, yes());";

    fa_eval_std_free(fa_get_context(rt), fa_eval_buf(fa_get_context(rt), script, strlen(script), "<input>", JS_EVAL_TYPE_MODULE));

    fa_run(rt);

    fa_free_runtime(rt);
}