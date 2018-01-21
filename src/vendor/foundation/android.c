/* android.c  -  Foundation library  -  Public Domain  -  2013 Mattias Jansson /
 * Rampant Pixels
 *
 * This library provides a cross-platform foundation library in C11 providing
 * basic support data types and functions to write applications and games in a
 * platform-independent fashion. The latest source code is always available at
 *
 * https://github.com/rampantpixels/foundation_lib
 *
 * This library is put in the public domain; you can redistribute it and/or
 * modify it without any restrictions.
 */

#include <foundation/foundation.h>

#if FOUNDATION_PLATFORM_ANDROID

#include <foundation/android.h>

#include <android/sensor.h>

static struct android_app *_android_app;
static struct ASensorEventQueue *_android_sensor_queue;
static bool _android_sensor_enabled[16];
static bool _android_destroyed = false;

static void _android_enable_sensor(int sensor_type);

static void _android_disable_sensor(int sensor_type);

void android_entry(struct android_app *app) {
    if (!app || !app->activity)
        return;

    // Avoid glue code getting stripped
    app_dummy();

    _android_app = app;
    _android_app->onAppCmd = android_handle_cmd;
    _android_app->onInputEvent = 0; // android_handle_input;
    _android_app->userData = 0;
}

int android_initialize(void) {
    _android_destroyed = false;

    log_debug(0, STRING_CONST("Waiting for application window to be set"));

    int ident = 0;
    int events = 0;
    struct android_poll_source *source = 0;
    while (!_android_app->window) {
        while ((ident = ALooper_pollAll(0, 0, &events, (void **)&source)) >=
               0) {
            // Process this event.
            if (source)
                source->process(_android_app, source);
        }
        thread_sleep(10);
    }

    log_debug(0, STRING_CONST("Application window set, continuing"));
    log_debugf(0, STRING_CONST("Internal data path: %s"),
               _android_app->activity->internalDataPath);
    log_debugf(0, STRING_CONST("External data path: %s"),
               _android_app->activity->externalDataPath);

    ASensorManager *sensor_manager = ASensorManager_getInstance();
    _android_sensor_queue = ASensorManager_createEventQueue(
                                sensor_manager, _android_app->looper, 0, android_sensor_callback, 0);

    // Enable accelerometer sensor
    //_android_enable_sensor(ASENSOR_TYPE_ACCELEROMETER);

    return 0;
}

void android_finalize(void) {
    log_info(0, STRING_CONST("Native android app shutdown"));

    _android_app->onAppCmd = 0;
    _android_app->onInputEvent = 0;
    _android_app->userData = 0;

    if (_android_sensor_queue) {
        ASensorManager *sensor_manager = ASensorManager_getInstance();
        ASensorManager_destroyEventQueue(sensor_manager, _android_sensor_queue);
    }
    _android_sensor_queue = 0;

    ANativeActivity_finish(_android_app->activity);

    while (!_android_destroyed) {
        int ident = 0;
        int events = 0;
        struct android_poll_source *source = 0;
        while ((ident = ALooper_pollAll(0, 0, &events, (void **)&source)) >=
               0) {
            // Process this event.
            if (source)
                source->process(_android_app, source);
        }

        thread_sleep(10);
    }

    _android_app = 0;

    log_debug(0, STRING_CONST("Exiting native android app"));
}

struct android_app *android_app(void) {
    return _android_app;
}

void android_handle_cmd(struct android_app *app, int32_t cmd) {
    FOUNDATION_UNUSED(app);
    switch (cmd) {
    case APP_CMD_INPUT_CHANGED:
        log_info(0, STRING_CONST("System command: APP_CMD_INPUT_CHANGED"));
        break;

    case APP_CMD_INIT_WINDOW:
#if BUILD_ENABLE_LOG
        if (app->window) {
            int w = 0, h = 0;
            w = ANativeWindow_getWidth(app->window);
            h = ANativeWindow_getHeight(app->window);
            log_infof(0,
                      STRING_CONST("System command: APP_CMD_INIT_WINDOW %dx%d"),
                      w, h);
        }
#endif
        break;

    case APP_CMD_TERM_WINDOW:
        log_info(0, STRING_CONST("System command: APP_CMD_TERM_WINDOW"));
        break;

    case APP_CMD_WINDOW_RESIZED:
#if BUILD_ENABLE_LOG
        if (app->window) {
            int w = 0, h = 0;
            w = ANativeWindow_getWidth(app->window);
            h = ANativeWindow_getHeight(app->window);
            log_infof(
                0, STRING_CONST("System command: APP_CMD_WINDOW_RESIZED %dx%d"),
                w, h);
        }
#endif
        break;

    case APP_CMD_WINDOW_REDRAW_NEEDED:
        log_info(0,
                 STRING_CONST("System command: APP_CMD_WINDOW_REDRAW_NEEDED"));
        break;

    case APP_CMD_CONTENT_RECT_CHANGED:
        log_info(0,
                 STRING_CONST("System command: APP_CMD_CONTENT_RECT_CHANGED"));
        break;

    case APP_CMD_GAINED_FOCUS:
        log_info(0, STRING_CONST("System command: APP_CMD_GAINED_FOCUS"));
        system_post_event(FOUNDATIONEVENT_FOCUS_GAIN);
        _android_enable_sensor(ASENSOR_TYPE_ACCELEROMETER);
        break;

    case APP_CMD_LOST_FOCUS:
        log_info(0, STRING_CONST("System command: APP_CMD_LOST_FOCUS"));
        _android_disable_sensor(ASENSOR_TYPE_ACCELEROMETER);
        system_post_event(FOUNDATIONEVENT_FOCUS_LOST);
        break;

    case APP_CMD_CONFIG_CHANGED:
        log_info(0, STRING_CONST("System command: APP_CMD_CONFIG_CHANGED"));
        break;

    case APP_CMD_LOW_MEMORY:
        log_info(0, STRING_CONST("System command: APP_CMD_LOW_MEMORY"));
        break;

    case APP_CMD_START:
        log_info(0, STRING_CONST("System command: APP_CMD_START"));
        system_post_event(FOUNDATIONEVENT_START);
        break;

    case APP_CMD_RESUME:
        log_info(0, STRING_CONST("System command: APP_CMD_RESUME"));
        system_post_event(FOUNDATIONEVENT_RESUME);
        break;

    case APP_CMD_SAVE_STATE:
        log_info(0, STRING_CONST("System command: APP_CMD_SAVE_STATE"));
        break;

    case APP_CMD_PAUSE:
        log_info(0, STRING_CONST("System command: APP_CMD_PAUSE"));
        system_post_event(FOUNDATIONEVENT_PAUSE);
        break;

    case APP_CMD_STOP:
        log_info(0, STRING_CONST("System command: APP_CMD_STOP"));
        system_post_event(FOUNDATIONEVENT_TERMINATE);
        break;

    case APP_CMD_DESTROY:
        log_info(0, STRING_CONST("System command: APP_CMD_DESTROY"));
        system_post_event(FOUNDATIONEVENT_TERMINATE);
        _android_destroyed = true;
        break;

    default:
        break;
    }
}

int android_sensor_callback(int fd, int events, void *data) {
    FOUNDATION_UNUSED(fd);
    FOUNDATION_UNUSED(events);
    FOUNDATION_UNUSED(data);
    return 1;
}

void _android_enable_sensor(int type) {
    FOUNDATION_ASSERT(type > 0 && type < 16);
    if (_android_sensor_enabled[type])
        return;

    ASensorManager *manager =
        _android_sensor_queue ? ASensorManager_getInstance() : 0;
    const ASensor *sensor =
        manager ? ASensorManager_getDefaultSensor(manager, type) : 0;
    if (sensor) {
        log_debugf(0, STRING_CONST("Initializing sensor of type %d"), type);
        if (ASensorEventQueue_enableSensor(_android_sensor_queue, sensor) < 0) {
            log_warnf(0, WARNING_SYSTEM_CALL_FAIL,
                      STRING_CONST("Unable to enable sensor of type %d"), type);
        } else {
            int min_delay = ASensor_getMinDelay(sensor);
            if (min_delay < 60000)
                min_delay = 60000;
            if (ASensorEventQueue_setEventRate(_android_sensor_queue, sensor,
                                               min_delay) < 0)
                log_warnf(
                    0, WARNING_SYSTEM_CALL_FAIL,
                    STRING_CONST(
                        "Unable to set event rate %d for sensor of type %d"),
                    min_delay, type);
            _android_sensor_enabled[type] = true;
        }
    } else {
        log_warn(0, WARNING_UNSUPPORTED,
                 STRING_CONST("Unable to initialize sensors"));
    }
}

void _android_disable_sensor(int sensor_type) {
    FOUNDATION_ASSERT(sensor_type > 0 && sensor_type < 16);
    if (!_android_sensor_enabled[sensor_type])
        return;

    ASensorManager *sensor_manager = ASensorManager_getInstance();
    if (_android_sensor_queue && sensor_manager) {
        const ASensor *sensor =
            ASensorManager_getDefaultSensor(sensor_manager, sensor_type);
        if (sensor) {
            ASensorEventQueue_disableSensor(_android_sensor_queue, sensor);
            _android_sensor_enabled[sensor_type] = false;
        }
    }
}

#if FOUNDATION_COMPILER_GCC
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#if FOUNDATION_COMPILER_CLANG
#if __has_warning("-Wformat-pedantic")
#pragma clang diagnostic ignored "-Wformat-pedantic"
#endif
#if __has_warning("-Wmissing-prototypes")
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#endif
#if __has_warning("-Wundef")
#pragma clang diagnostic ignored "-Wundef"
#endif
#if __has_warning("-Wunused-parameter")
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif
#if __has_warning("-Wsign-conversion")
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif
#if __has_warning("-Wundef")
#pragma clang diagnostic ignored "-Wundef"
#endif
#if __has_warning("-Wpedantic")
#pragma clang diagnostic ignored "-Wpedantic"
#endif
#if __has_warning("-Wreserved-id-macro")
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#endif
#if __has_warning("-Wshorten-64-to-32")
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#endif
#endif

// Assimilate app glue library
#include <android_native_app_glue.c>

// Assimilate cpu features library
#include <cpu-features.c>

#endif
