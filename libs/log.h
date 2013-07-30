#ifndef _LOG_H_
#define _LOG_H_

#include <syslog.h>
#include "settings.h"

void logprintf(int prio, const char *format_str, ...);
void logperror(int prio, const char *s);
void enable_file_log(void);
void disable_file_log(void);
void enable_shell_log(void);
void disable_shell_log(void);
void set_logfile(char *file);
void set_loglevel(int level);
int get_loglevel(void);

#endif
