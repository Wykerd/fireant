#include "fireant.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define FA_VERSION_STRING TOSTRING(FA_VERSION_MAJOR) "." TOSTRING(FA_VERSION_MINOR) "." TOSTRING(FA_VERSION_PATCH) FA_VERSION_SUFFIX

const char *fa_get_ver_str (void) {
    return FA_VERSION_STRING;
}

const int fa_get_maj_ver (void) {
    return FA_VERSION_MAJOR;
}

const int fa_get_min_ver (void) {
    return FA_VERSION_MINOR;
}

const int fa_get_patch_ver (void) {
    return FA_VERSION_PATCH;
}

const char *fa_get_qjs_ver (void) {
    return QJS_VERSION_STR;
}