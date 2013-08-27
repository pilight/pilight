#include <stdio.h>
/* Hardware types */
extern struct hardware *hw_list[];
extern struct hardware hw;
extern struct ir_remote *last_remote;
int hw_choose_driver(char *);
void hw_print_drivers(FILE *);
