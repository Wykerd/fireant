#ifndef FA_MODULES_H
#define FA_MODULES_H

#include <quickjs.h>

JSModuleDef *fa_module_loader (
    JSContext *ctx,
    const char *module_name, void *opaque
);

int js_module_set_import_meta (
    JSContext *ctx, 
    JSValueConst func_val,
    JS_BOOL use_realpath, 
    JS_BOOL is_main
);

typedef JSModuleDef *(JSInitModuleFunc)(
    JSContext *ctx,
    const char *module_name
);

uint8_t *fa_load_file (JSContext *ctx, size_t *pbuf_len, const char *filename);

#endif