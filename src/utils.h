#ifndef FA_UTILS_H
#define FA_UTILS_H

#include <quickjs/quickjs.h>

#define FA_CHECK(x) js_likely(x)
#define FA_NULL_CHECK(x) FA_CHECK(x != NULL)
#define FA_CHECK_RETURN(x) if (FA_CHECK(x)) { return NULL; }
#define FA_NULL_RETURN(x) FA_CHECK_RETURN(!FA_NULL_CHECK(x))

void js_std_dump_error(JSContext *ctx);

#endif