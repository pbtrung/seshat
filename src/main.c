#include <sodium.h>

#include "foundation/foundation.h"
#include "utils/common.h"

int main_initialize(void) {
    rc_t rc = SESHAT_SUCCESS;

    application_t application;
    foundation_config_t config;

    memset(&config, 0, sizeof(config));

    memset(&application, 0, sizeof(application));
    application.name = string_const(STRING_CONST("seshat"));
    application.short_name = string_const(STRING_CONST("seshat"));
    application.flags = APPLICATION_UTILITY;

    log_enable_prefix(false);
    log_set_suppress(0, ERRORLEVEL_WARNING);

    rc = foundation_initialize(memory_system_malloc(), application, config);
    if (rc != SESHAT_SUCCESS) {
        return rc;
    }

    return SESHAT_SUCCESS;
}

int main_run(void *main_arg) {
    rc_t rc = SESHAT_SUCCESS;

    rc = sodium_init();
    
    return rc;
}

void main_finalize(void) { foundation_finalize(); }