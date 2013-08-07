#ifndef _LOG_H_
#define _LOG_H_

#include <syslog.h>
#include "settings.h"

void logprintf(int prio, const char *format_str, ...);
void logperror(int prio, const char *s);
void log_file_enable(void);
void log_file_disable(void);
void log_shell_enable(void);
void log_shell_disable(void);
void log_file_set(char *file);
void log_level_set(int level);
int log_level_get(void);

#endif
