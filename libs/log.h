#ifndef _LOG_H_
#define _LOG_H_

#include <syslog.h>
#include "config.h"

void logprintf(int prio, char *format_str, ...);
void logperror(int prio, const char *s);
void enable_file_log();
void disable_file_log();
void enable_shell_log();
void disable_shell_log();
void set_logfile(char *file);
void set_loglevel(int level);

#endif