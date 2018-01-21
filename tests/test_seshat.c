#include <sodium.h>

#include "foundation/foundation.h"
#include "utils/common.h"

#include "test_skein.h"
#include "test_key.h"
#include "test_ctr.h"

int main_initialize(void) {
    rc_t rc = SESHAT_SUCCESS;

    application_t application;
    foundation_config_t config;

    memset(&config, 0, sizeof(config));

    memset(&application, 0, sizeof(application));
    application.name = string_const(STRING_CONST("test_seshat"));
    application.short_name = string_const(STRING_CONST("test_seshat"));
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
    check_if_log(rc == -1, log_rc_msg("test_seshat::main", SESHAT_SODIUM_ERR),
                 return SESHAT_SODIUM_ERR);

    MU_RUN_SUITE(test_skein);
    MU_RUN_SUITE(test_key);
    MU_RUN_SUITE(test_ctr);

    MU_REPORT();
    return 0;
}

void main_finalize(void) { foundation_finalize(); }