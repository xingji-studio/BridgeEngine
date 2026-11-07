#include "BridgeEngine.h"
#include "internal/bapi_internal.h"

#include <stdarg.h>
#include <stdio.h>

static char g_last_error[256];

void bapi_set_last_error(const char *format, ...)
{
	if (format == NULL) {
		g_last_error[0] = '\0';
		return;
	}

	va_list args;
	va_start(args, format);
	vsnprintf(g_last_error, sizeof(g_last_error), format, args);
	va_end(args);
	g_last_error[sizeof(g_last_error) - 1] = '\0';
}

const char *bapi_get_last_error(void)
{
	return g_last_error;
}

void bapi_clear_last_error(void)
{
	g_last_error[0] = '\0';
}
