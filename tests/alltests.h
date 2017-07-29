#ifndef _ALLTESTS_H_
#define _ALLTESTS_H_

#define CONFIG_AUTH	1

int suiteFailed(void);
int test_unittest(void);
int test_memory(void);

extern char whitelist[10][16];
extern int whitelistnr;
extern int config_enable;

#endif