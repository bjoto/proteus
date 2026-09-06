#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define JAR_XM_IMPLEMENTATION
#include "jar_xm.h"

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s module.xm\n", argv[0]);
        return 2;
    }
    jar_xm_context_t *xm = NULL;
    int result = jar_xm_create_context_from_file(&xm, 48000, argv[1]);
    if (result != 0) {
        fprintf(stderr, "invalid XM (jar_xm error %d)\n", result);
        return 1;
    }
    printf("%s: %u channels, %u patterns, %u instruments\n", jar_xm_get_module_name(xm),
        jar_xm_get_number_of_channels(xm), jar_xm_get_number_of_patterns(xm),
        jar_xm_get_number_of_instruments(xm));
    float frames[48000 * 2];
    jar_xm_generate_samples(xm, frames, 48000);
    jar_xm_free_context(xm);
    return 0;
}
