#include <BridgeEngine.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    bapi_log_config_t config = {
        .min_level = BAPI_LOG_LEVEL_DEBUG,
        .use_colors = true,
        .use_file = true,
        .log_file_path = "bridgeengine.log"
    };

    bapi_log_init(&config);

    BAPI_LOG_DEBUG("This is a debug message");
    BAPI_LOG_INFO("This is an info message");
    BAPI_LOG_WARN("This is a warning message");
    BAPI_LOG_ERROR("This is an error message");
    BAPI_LOG_CRITICAL("This is a critical message");

    int value = 42;
    const char* name = "BridgeEngine";
    BAPI_LOG_INFO("Engine version info: name=%s, value=%d", name, value);

    BAPI_LOG_ASSERT(value == 42, "Value should be 42 but got %d", value);
    BAPI_LOG_ASSERT(value != 42, "This assertion should fail: %d", value);

    bapi_log_shutdown();

    printf("\nLog file 'bridgeengine.log' has been created with all log messages.\n");

    return 0;
}
