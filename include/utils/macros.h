#ifndef __MACROS_H__
#define __MACROS_H__

#define check_if_log(A, LOG, CMD)                                              \
    if (A) {                                                                   \
        LOG;                                                                   \
        CMD;                                                                   \
    }

#endif //__MACROS_H__
