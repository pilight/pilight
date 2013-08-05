#include "settings.h"

#ifndef USE_LIRC

#ifndef _IRQ_H_
#define _IRQ_H_

#include <poll.h>

#define CHANGE				0
#define FALLING				1
#define RISING				2

typedef void (*func)(void);

char fn[GPIO_FN_MAXLEN];
int fd, ret;
struct pollfd pfd;
char rdbuf[RDBUF_LEN];

int irq_attach(int gpio_pin, int mode);
void irq_reset(int *fd, char *rdbuf);
int irq_poll(int *fd, char *rdbuf, struct pollfd* pfd);
int irq_read(void);
int irq_free(void);

#endif

#endif