#ifndef FA_RUNTIME_H
#define FA_RUNTIME_H

#include "fireant.h"

fa_runtime_t *fa_new_runtime_impl (int is_worker);
void fa_execute_jobs (JSContext *ctx);

#endif