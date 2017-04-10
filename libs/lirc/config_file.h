/*      $Id: config_file.h,v 5.12 2009/05/24 10:46:52 lirc Exp $      */

/****************************************************************************
 ** config_file.h ***********************************************************
 ****************************************************************************
 *
 * config_file.h - parses the config file of lircd
 *
 * Copyright (C) 1998 Pablo d'Angelo (pablo@ag-trek.allgaeu.org)
 *
 */

#ifndef  _CONFIG_FILE_H
#define  _CONFIG_FILE_H

#include <sys/types.h>

#include <unistd.h>

#include "ir_remote.h"

/*
  config stuff
*/

enum directive { ID_none, ID_remote, ID_codes, ID_raw_codes, ID_raw_name };

struct ptr_array {
	void **ptr;
	size_t nr_items;
	size_t chunk_size;
};

struct void_array {
	void *ptr;
	size_t item_size;
	size_t nr_items;
	size_t chunk_size;
};

void **init_void_array(struct void_array *ar, size_t chunk_size, size_t item_size);
int add_void_array(struct void_array *ar, void *data);
void *get_void_array(struct void_array *ar);

/* some safer functions */
void *s_malloc(size_t size);
char *s_strdup(char *string);
ir_code s_strtocode(const char *val);
__u32 s_strtou32(char *val);
int s_strtoi(char *val);
unsigned int s_strtoui(char *val);
lirc_t s_strtolirc_t(char *val);

int checkMode(int is_mode, int c_mode, char *error);
int parseFlags(char *val);
int addSignal(struct void_array *signals, char *val);
struct ir_ncode *defineCode(char *key, char *val, struct ir_ncode *code);
struct ir_code_node *defineNode(struct ir_ncode *code, const char *val);
int defineRemote(char *key, char *val, char *val2, struct ir_remote *rem);
struct ir_remote *read_config(FILE * f, const char *name);
void free_config(struct ir_remote *remotes);

#endif
