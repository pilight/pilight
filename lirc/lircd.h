/*      $Id: lircd.h,v 5.19 2010/03/20 16:18:30 lirc Exp $      */

/****************************************************************************
 ** lircd.h *****************************************************************
 ****************************************************************************
 *
 */

#ifndef _LIRCD_H
#define _LIRCD_H

#include <syslog.h>
#include <sys/time.h>

#include "ir_remote.h"

#define PACKET_SIZE (256)
#define WHITE_SPACE " \t"

struct peer_connection {
	char *host;
	unsigned short port;
	struct timeval reconnect;
	int connection_failure;
	int socket;
};

extern int debug;

void sigterm(int sig);
void dosigterm(int sig);
void sighup(int sig);
void dosighup(int sig);
int setup_uinput(const char *name);
void config(void);
void nolinger(int sock);
void remove_client(int fd);
void add_client(int);
int add_peer_connection(char *server);
void connect_to_peers();
int get_peer_message(struct peer_connection *peer);

#ifdef DEBUG
#define LOGPRINTF(level,fmt,args...)	\
  if(level<=debug) logprintf(LOG_DEBUG,fmt, ## args )
#define LOGPERROR(level,s) \
  if(level<=debug) logperror(LOG_DEBUG,s)
#else
#define LOGPRINTF(level,fmt,args...)	\
  do {} while(0)
#define LOGPERROR(level,s) \
  do {} while(0)
#endif

void logprintf(int prio, char *format_str, ...);
void logperror(int prio, const char *s);

void daemonize(void);
void sigalrm(int sig);
void dosigalrm(int sig);
int parse_rc(int fd, char *message, char *arguments, struct ir_remote **remote, struct ir_ncode **code, int *reps,
	     int n, int *err);
int send_success(int fd, char *message);
int send_error(int fd, char *message, char *format_str, ...);
int send_remote_list(int fd, char *message);
int send_remote(int fd, char *message, struct ir_remote *remote);
int send_name(int fd, char *message, struct ir_ncode *code);
int list(int fd, char *message, char *arguments);
int set_transmitters(int fd, char *message, char *arguments);
int simulate(int fd, char *message, char *arguments);
int send_once(int fd, char *message, char *arguments);
int send_start(int fd, char *message, char *arguments);
int send_stop(int fd, char *message, char *arguments);
int send_core(int fd, char *message, char *arguments, int once);
int version(int fd, char *message, char *arguments);
int get_pid(int fd, char *message, char *arguments);
int get_command(int fd);
void input_message(const char *message, const char *remote_name, const char *button_name, int reps, int release);
void broadcast_message(const char *message);
int waitfordata(long maxusec);
void loop(void);

struct protocol_directive {
	char *name;
	int (*function) (int fd, char *message, char *arguments);
};

#endif /* _LIRCD_H */
