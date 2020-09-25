#include "compiler.h"
#include <quickjs.h>
#include <cutils.h>
#include "utils.h"
#include "modules.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

static const char fa_sig[] = "FaBC";

static void namelist_add (
    namelist_t *lp, 
    const char *name, 
    const char *short_name,
    int flags
) {
    namelist_entry_t *e;
    if (lp->count == lp->size) {
        size_t newsize = lp->size + (lp->size >> 1) + 4;
        namelist_entry_t *a =
            realloc(lp->array, sizeof(lp->array[0]) * newsize);
        /* XXX: check for realloc failure */
        lp->array = a;
        lp->size = newsize;
    }
    e =  &lp->array[lp->count++];
    e->name = strdup(name);
    if (short_name)
        e->short_name = strdup(short_name);
    else
        e->short_name = NULL;
    e->flags = flags;
}

static void namelist_free(namelist_t *lp)
{
    while (lp->count > 0) {
        namelist_entry_t *e = &lp->array[--lp->count];
        free(e->name);
        free(e->short_name);
    }
    free(lp->array);
    lp->array = NULL;
    lp->size = 0;
}

static namelist_entry_t *namelist_find(namelist_t *lp, const char *name)
{
    int i;
    for(i = 0; i < lp->count; i++) {
        namelist_entry_t *e = &lp->array[i];
        if (!strcmp(e->name, name))
            return e;
    }
    return NULL;
}

static void output_object_code (
    JSContext *ctx,
    fa_compile_t *cmp, 
    JSValueConst obj,
    BOOL load_only
) {
    uint8_t *out_buf;
    size_t out_buf_len;
    out_buf = JS_WriteObject(ctx, &out_buf_len, obj, JS_WRITE_OBJ_BYTECODE);
    if (!out_buf) {
        fa_dump_error(ctx);
        exit(1);
    }

    size_t cursor = cmp->output.size;

    printf("Module size: %d Bytes\n", out_buf_len);

    cmp->output.size = cmp->output.size + sizeof(size_t) + 1 + out_buf_len;

    cmp->output.buf = realloc(cmp->output.buf, cmp->output.size);

    memcpy(cmp->output.buf + cursor, &out_buf_len, sizeof(size_t));
    cursor += sizeof(size_t);
    memcpy(cmp->output.buf + cursor, &load_only, 1);
    cursor += 1;
    memcpy(cmp->output.buf + cursor, out_buf, out_buf_len);

    js_free(ctx, out_buf);
}

static int js_module_dummy_init(JSContext *ctx, JSModuleDef *m)
{
    /* should never be called when compiling JS code */
    abort();
}

static JSModuleDef *jsc_module_loader(
    JSContext *ctx,
    const char *module_name, 
    void *opaque
) {
    JSModuleDef *m;
    namelist_entry_t *e;
    fa_compile_t *cmp = JS_GetContextOpaque(ctx);

    /* check if it is a declared C or system module */
    e = namelist_find(&cmp->cmodule_list, module_name);
    if (e) {
        /* add in the static init module list */
        namelist_add(&cmp->init_module_list, e->name, e->short_name, 0);
        /* create a dummy module */
        m = JS_NewCModule(ctx, module_name, js_module_dummy_init);
    } else if (has_suffix(module_name, ".so")) {
        fprintf(stderr, "Warning: binary module '%s' will be dynamically loaded\n", module_name);
        /* create a dummy module */
        m = JS_NewCModule(ctx, module_name, js_module_dummy_init);
        /* the resulting executable will export its symbols for the
           dynamic library */
        cmp->dynamic_export = TRUE;
    } else {
        size_t buf_len;
        uint8_t *buf;
        JSValue func_val;
        
        buf = fa_load_file(ctx, &buf_len, module_name);
        if (!buf) {
            JS_ThrowReferenceError(ctx, "could not load module filename '%s'", module_name);
            return NULL;
        }
        
        /* compile the module */
        func_val = JS_Eval(ctx, (char *)buf, buf_len, module_name,
                           JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
        js_free(ctx, buf);
        if (JS_IsException(func_val))
            return NULL;
        printf("Writing bytecode for module '%s'\n", module_name);
        output_object_code(ctx, cmp, func_val, TRUE);
        
        /* the module is already referenced, so we must free it */
        m = JS_VALUE_GET_PTR(func_val);
        JS_FreeValue(ctx, func_val);
    }
    return m;
}

static void compile_module (
    JSContext *ctx, 
    fa_compile_t *cmp,
    const char *filename,
    int module
) {
    uint8_t *buf;
    int eval_flags;
    JSValue obj;
    size_t buf_len;
    
    buf = fa_load_file(ctx, &buf_len, filename);
    if (!buf) {
        fprintf(stderr, "Could not load '%s'\n", filename);
        exit(1);
    }
    eval_flags = JS_EVAL_FLAG_COMPILE_ONLY;
    if (module < 0) {
        module = (has_suffix(filename, ".mjs") ||
                  JS_DetectModule((const char *)buf, buf_len));
    }
    if (module)
        eval_flags |= JS_EVAL_TYPE_MODULE;
    else
        eval_flags |= JS_EVAL_TYPE_GLOBAL;
    obj = JS_Eval(ctx, (const char *)buf, buf_len, filename, eval_flags);
    if (JS_IsException(obj)) {
        fa_dump_error(ctx);
        exit(1);
    }
    js_free(ctx, buf);
    printf("\nWriting input script bytecode\n");
    output_object_code(ctx, cmp, obj, FALSE);
    JS_FreeValue(ctx, obj);
}

fa_compile_t *compile (
    const char    *modulename
) {
    fa_compile_t *cmp = malloc(sizeof(fa_compile_t));
    memset(cmp, 0, sizeof(fa_compile_t));

    cmp->output.buf = malloc(4);
    memcpy(cmp->output.buf, fa_sig, 4);
    cmp->output.size = 4;

    int i;
    JSRuntime *rt;
    JSContext *ctx;
    namelist_t dynamic_module_list;
    int module;

    // autodetect whether input is module
    module = -1;
    
    memset(&dynamic_module_list, 0, sizeof(dynamic_module_list));
    
    /* add system modules */
    namelist_add(&cmp->cmodule_list, "std", "std", 0);
    
    rt = JS_NewRuntime();
    ctx = JS_NewContext(rt);

    JS_SetContextOpaque(ctx, cmp);
    
    JS_AddIntrinsicBigFloat(ctx);
    JS_AddIntrinsicBigDecimal(ctx);
    JS_AddIntrinsicOperators(ctx);
    JS_EnableBignumExt(ctx, TRUE);
    
    /* loader for ES6 modules */
    JS_SetModuleLoaderFunc(rt, NULL, jsc_module_loader, NULL);

    /* compile the input module */
    compile_module(ctx, cmp, modulename, module);

    for(i = 0; i < dynamic_module_list.count; i++) {
        if (!jsc_module_loader(ctx, dynamic_module_list.array[i].name, NULL)) {
            fprintf(stderr, "Could not load dynamic module '%s'\n",
                    dynamic_module_list.array[i].name);
            exit(1);
        }
    }
    
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    namelist_free(&cmp->cmodule_list);
    namelist_free(&cmp->cmodule_list);
    namelist_free(&cmp->init_module_list);

    cmp->output.size++;
    cmp->output.buf = realloc(cmp->output.buf, cmp->output.size);
    cmp->output.buf[cmp->output.size - 1] = '\0';

    return cmp;
}

int main (int argc, char **argv) {
    assert(argc == 3);
    fa_compile_t *cmp = compile(argv[1]);
    FILE *fptr;
    fptr = fopen(argv[2], "w");
    fwrite(cmp->output.buf, cmp->output.size, 1, fptr);
    fclose(fptr);
    free(cmp);
}