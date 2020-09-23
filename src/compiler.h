#ifndef FA_COMPILE_H
#define FA_COMPILE_H

#include <stddef.h>

struct fa_bytecode_s {
    char    *buf;
    size_t  size;
};

typedef struct fa_bytecode_s fa_bytecode_t;

struct namelist_entry_s {
    char *name;
    char *short_name;
    int flags;
};

typedef struct namelist_entry_s namelist_entry_t;

struct namelist_s {
    namelist_entry_t *array;
    int count;
    int size;
};

typedef struct namelist_s namelist_t;

struct fa_compile_s {
    namelist_t cname_list;
    namelist_t cmodule_list;
    namelist_t init_module_list;
    int dynamic_export;
    fa_bytecode_t output;
};

typedef struct fa_compile_s fa_compile_t;

fa_compile_t *compile (
    const char    *modulename
);

#endif