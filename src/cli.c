#include "fireant.h"
#include <stdio.h>

int main () {
    printf("FireAnt Version: %s\nQuickJS Version: %s\n\n", fa_get_ver_str(), fa_get_qjs_ver());
}