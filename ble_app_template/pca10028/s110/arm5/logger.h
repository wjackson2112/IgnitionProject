
#ifndef LOGGER_H__
#define LOGGER_H__

#include "SEGGER_RTT.h"
#include "app_error.h"

#define LEVEL 5

#ifdef  DEBUG
#define DEBUG_TEST 1
#define LOG_LEVEL LEVEL
#else
#define DEBUG_TEST 0
#define LOG_LEVEL 0
#endif

#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

#define LOG_CLEAR() \
	do { if (DEBUG_TEST && LOG_LEVEL > 0) SEGGER_RTT_printf(0, "%s%s", RTT_CTRL_CLEAR, RTT_CTRL_RESET); } while (0)

#define LOG_DEBUG(format, ...) \
    do { if (DEBUG_TEST && LOG_LEVEL > 4) SEGGER_RTT_printf(0, "%s%s[DEBUG] " format "\n", RTT_CTRL_BG_BLACK, RTT_CTRL_TEXT_WHITE, ##__VA_ARGS__); } while (0)
		
#define LOG_INFO(format, ...) \
    do { if (DEBUG_TEST && LOG_LEVEL > 3) SEGGER_RTT_printf(0, "%s%s[ INFO] " format "\n", RTT_CTRL_BG_BLACK, RTT_CTRL_TEXT_BRIGHT_GREEN, ##__VA_ARGS__); } while (0)

#define LOG_WARN(format, ...) \
    do { if (DEBUG_TEST && LOG_LEVEL > 2) SEGGER_RTT_printf(0, "%s%s[ WARN] %s(%d): " format "\n", RTT_CTRL_BG_BLACK, RTT_CTRL_TEXT_BRIGHT_YELLOW, __FILENAME__, __LINE__, ##__VA_ARGS__); } while (0)

#define LOG_ERROR(format, ...) \
    do { if (DEBUG_TEST && LOG_LEVEL > 1) SEGGER_RTT_printf(0, "%s%s[ERROR] %s(%d): " format "\n", RTT_CTRL_BG_BLACK, RTT_CTRL_TEXT_BRIGHT_RED, __FILENAME__, __LINE__, ##__VA_ARGS__); } while (0)

#define LOG_FATAL(format, ...) \
    do { if (DEBUG_TEST && LOG_LEVEL > 0) SEGGER_RTT_printf(0, "%s%s[FATAL] %s(%d): " format "\n", RTT_CTRL_BG_RED, RTT_CTRL_TEXT_WHITE, __FILENAME__, __LINE__, ##__VA_ARGS__); APP_ERROR_CHECK(NRF_ERROR_NO_MEM); } while (0)

#endif
