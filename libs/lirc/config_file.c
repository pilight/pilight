/*      $Id: config_file.c,v 5.34 2010/07/11 03:28:21 jarodwilson Exp $      */

/****************************************************************************
 ** config_file.c ***********************************************************
 ****************************************************************************
 *
 * config_file.c - parses the config file of lircd
 *
 * Copyright (C) 1998 Pablo d'Angelo <pablo@ag-trek.allgaeu.org>
 *
 */

#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <ctype.h>

#include "../pilight/core/log.h"
#include "../pilight/core/mem.h"
#include "ir_remote.h"
#include "config_file.h"
#include "transmit.h"

#define LINE_LEN 1024
#define MAX_INCLUDES 10

struct flaglist {
	char *name;
	int flag;
};

static struct flaglist all_flags[] = {
	{"RAW_CODES", RAW_CODES},
	{"RC5", RC5},
	{"SHIFT_ENC", SHIFT_ENC},	/* obsolete */
	{"RC6", RC6},
	{"RCMM", RCMM},
	{"SPACE_ENC", SPACE_ENC},
	{"SPACE_FIRST", SPACE_FIRST},
	{"GOLDSTAR", GOLDSTAR},
	{"GRUNDIG", GRUNDIG},
	{"BO", BO},
	{"SERIAL", SERIAL},
	{"XMP", XMP},

	{"REVERSE", REVERSE},
	{"NO_HEAD_REP", NO_HEAD_REP},
	{"NO_FOOT_REP", NO_FOOT_REP},
	{"CONST_LENGTH", CONST_LENGTH},	/* remember to adapt warning
					   message when changing this */
	{"REPEAT_HEADER", REPEAT_HEADER},
	{NULL, 0},
};


const char *whitespace = " \t";

static int line;
static int parse_error;

static struct ir_remote *read_config_recursive(FILE * f, const char *name, int depth);
static void calculate_signal_lengths(struct ir_remote *remote);

void **init_void_array(struct void_array *ar, size_t chunk_size, size_t item_size)
{
	ar->chunk_size = chunk_size;
	ar->item_size = item_size;
	ar->nr_items = 0;
	if (!(ar->ptr = calloc(chunk_size, ar->item_size))) {
		OUT_OF_MEMORY
	}
	return (ar->ptr);
}

int add_void_array(struct void_array *ar, void *dataptr)
{
	void *ptr;

	if ((ar->nr_items % ar->chunk_size) == (ar->chunk_size) - 1) {
		/* I hope this works with the right alignment,
		   if not we're screwed */
		if (!(ptr = realloc(ar->ptr, ar->item_size * ((ar->nr_items) + (ar->chunk_size + 1))))) {
			OUT_OF_MEMORY
		}
		ar->ptr = ptr;
	}
	memcpy((ar->ptr) + (ar->item_size * ar->nr_items), dataptr, ar->item_size);
	ar->nr_items = (ar->nr_items) + 1;
	memset((ar->ptr) + (ar->item_size * ar->nr_items), 0, ar->item_size);
	return (1);
}

void *get_void_array(struct void_array *ar)
{
	return (ar->ptr);
}

void *s_malloc(size_t size)
{
	void *ptr;
	if ((ptr = malloc(size)) == NULL) {
		OUT_OF_MEMORY
	}
	memset(ptr, 0, size);
	return (ptr);
}

inline char *s_strdup(char *string)
{
	char *ptr;
	if (!(ptr = strdup(string))) {
		OUT_OF_MEMORY
	}
	return (ptr);
}

inline ir_code s_strtocode(const char *val)
{
	ir_code code = 0;
	char *endptr;

	errno = 0;
	code = strtoull(val, &endptr, 0);
	if ((code == (__u64) - 1 && errno == ERANGE) || strlen(endptr) != 0 || strlen(val) == 0) {
		logprintf(LOG_ERR, "error in configfile line %d:", line);
		logprintf(LOG_ERR, "\"%s\": must be a valid (__u64) number", val);
		parse_error = 1;
		return (0);
	}
	return (code);
}

__u32 s_strtou32(char *val)
{
	__u32 n;
	char *endptr;

	n = strtoul(val, &endptr, 0);
	if (!*val || *endptr) {
		logprintf(LOG_ERR, "error in configfile line %d:", line);
		logprintf(LOG_ERR, "\"%s\": must be a valid (__u32) number", val);
		parse_error = 1;
		return (0);
	}
	return (n);
}

int s_strtoi(char *val)
{
	char *endptr;
	long n;
	int h;

	n = strtol(val, &endptr, 0);
	h = (int)n;
	if (!*val || *endptr || n != ((long)h)) {
		logprintf(LOG_ERR, "error in configfile line %d:", line);
		logprintf(LOG_ERR, "\"%s\": must be a valid (int) number", val);
		parse_error = 1;
		return (0);
	}
	return (h);
}

unsigned int s_strtoui(char *val)
{
	char *endptr;
	__u32 n;
	unsigned int h;

	n = strtoul(val, &endptr, 0);
	h = (unsigned int)n;
	if (!*val || *endptr || n != ((__u32) h)) {
		logprintf(LOG_ERR, "error in configfile line %d:", line);
		logprintf(LOG_ERR, "\"%s\": must be a valid (unsigned int) number", val);
		parse_error = 1;
		return (0);
	}
	return (h);
}

lirc_t s_strtolirc_t(char *val)
{
	__u32 n;
	lirc_t h;
	char *endptr;

	n = strtoul(val, &endptr, 0);
	h = (lirc_t) n;
	if (!*val || *endptr || n != ((__u32) h)) {
		logprintf(LOG_ERR, "error in configfile line %d:", line);
		logprintf(LOG_ERR, "\"%s\": must be a valid (lirc_t) number", val);
		parse_error = 1;
		return (0);
	}
	if (h < 0) {
		logprintf(LOG_WARNING, "error in configfile line %d:", line);
		logprintf(LOG_WARNING, "\"%s\" is out of range", val);
	}
	return (h);
}

int checkMode(int is_mode, int c_mode, char *error)
{
	if (is_mode != c_mode) {
		logprintf(LOG_ERR, "fatal error in configfile line %d:", line);
		logprintf(LOG_ERR, "\"%s\" isn't valid at this position", error);
		parse_error = 1;
		return (0);
	}
	return (1);
}

int addSignal(struct void_array *signals, char *val)
{
	unsigned int t;

	t = s_strtoui(val);
	if (parse_error)
		return 0;
	if (!add_void_array(signals, &t))
		return 0;

	return 1;
}

struct ir_ncode *defineCode(char *key, char *val, struct ir_ncode *code)
{
	memset(code, 0, sizeof(*code));
	code->name = s_strdup(key);
	code->code = s_strtocode(val);
	return (code);
}

struct ir_code_node *defineNode(struct ir_ncode *code, const char *val)
{
	struct ir_code_node *node;

	node = s_malloc(sizeof(*node));
	if (node == NULL)
		return NULL;

	node->code = s_strtocode(val);
	node->next = NULL;

	if (code->current == NULL) {
		code->next = node;
		code->current = node;
	} else {
		code->current->next = node;
		code->current = node;
	}
	return node;
}

int parseFlags(char *val)
{
	struct flaglist *flaglptr;
	int flags = 0;
	char *flag, *help;

	flag = help = val;
	while (flag != NULL) {
		while (*help != '|' && *help != 0)
			help++;
		if (*help == '|') {
			*help = 0;
			help++;
		} else {
			help = NULL;
		}

		flaglptr = all_flags;
		while (flaglptr->name != NULL) {
			if (strcasecmp(flaglptr->name, flag) == 0) {
				if (flaglptr->flag & IR_PROTOCOL_MASK && flags & IR_PROTOCOL_MASK) {
					logprintf(LOG_ERR, "error in configfile line %d:", line);
					logprintf(LOG_ERR, "multiple protocols given in flags: \"%s\"", flag);
					parse_error = 1;
					return (0);
				}
				flags = flags | flaglptr->flag;
				break;
			}
			flaglptr++;
		}
		if (flaglptr->name == NULL) {
			logprintf(LOG_ERR, "error in configfile line %d:", line);
			logprintf(LOG_ERR, "unknown flag: \"%s\"", flag);
			parse_error = 1;
			return (0);
		}
		flag = help;
	}

	return (flags);
}

int defineRemote(char *key, char *val, char *val2, struct ir_remote *rem)
{
	if ((strcasecmp("name", key)) == 0) {
		if (rem->name != NULL)
			free(rem->name);
		rem->name = s_strdup(val);
		return (1);
	}
#ifdef DYNCODES
	if ((strcasecmp("dyncodes_name", key)) == 0) {
		if (rem->dyncodes_name != NULL) {
			free(rem->dyncodes_name);
		}
		rem->dyncodes_name = s_strdup(val);
		return (1);
	}
#endif
	else if ((strcasecmp("bits", key)) == 0) {
		rem->bits = s_strtoi(val);
		return (1);
	} else if (strcasecmp("flags", key) == 0) {
		rem->flags |= parseFlags(val);
		return (1);
	} else if (strcasecmp("eps", key) == 0) {
		rem->eps = s_strtoi(val);
		return (1);
	} else if (strcasecmp("aeps", key) == 0) {
		rem->aeps = s_strtoi(val);
		return (1);
	} else if (strcasecmp("plead", key) == 0) {
		rem->plead = s_strtolirc_t(val);
		return (1);
	} else if (strcasecmp("ptrail", key) == 0) {
		rem->ptrail = s_strtolirc_t(val);
		return (1);
	} else if (strcasecmp("pre_data_bits", key) == 0) {
		rem->pre_data_bits = s_strtoi(val);
		return (1);
	} else if (strcasecmp("pre_data", key) == 0) {
		rem->pre_data = s_strtocode(val);
		return (1);
	} else if (strcasecmp("post_data_bits", key) == 0) {
		rem->post_data_bits = s_strtoi(val);
		return (1);
	} else if (strcasecmp("post_data", key) == 0) {
		rem->post_data = s_strtocode(val);
		return (1);
	} else if (strcasecmp("gap", key) == 0) {
		if (val2 != NULL) {
			rem->gap2 = s_strtou32(val2);
		}
		rem->gap = s_strtou32(val);
		return (val2 != NULL ? 2 : 1);
	} else if (strcasecmp("repeat_gap", key) == 0) {
		rem->repeat_gap = s_strtou32(val);
		return (1);
	}
	/* obsolete: use toggle_bit_mask instead */
	else if (strcasecmp("toggle_bit", key) == 0) {
		rem->toggle_bit = s_strtoi(val);
		return 1;
	} else if (strcasecmp("toggle_bit_mask", key) == 0) {
		rem->toggle_bit_mask = s_strtocode(val);
		return 1;
	} else if (strcasecmp("toggle_mask", key) == 0) {
		rem->toggle_mask = s_strtocode(val);
		return (1);
	} else if (strcasecmp("rc6_mask", key) == 0) {
		rem->rc6_mask = s_strtocode(val);
		return (1);
	} else if (strcasecmp("ignore_mask", key) == 0) {
		rem->ignore_mask = s_strtocode(val);
		return (1);
	}
	/* obsolete name */
	else if (strcasecmp("repeat_bit", key) == 0) {
		rem->toggle_bit = s_strtoi(val);
		return (1);
	} else if (strcasecmp("suppress_repeat", key) == 0) {
		rem->suppress_repeat = s_strtoi(val);
		return (1);
	} else if (strcasecmp("min_repeat", key) == 0) {
		rem->min_repeat = s_strtoi(val);
		return (1);
	} else if (strcasecmp("min_code_repeat", key) == 0) {
		rem->min_code_repeat = s_strtoi(val);
		return (1);
	} else if (strcasecmp("frequency", key) == 0) {
		rem->freq = s_strtoui(val);
		return (1);
	} else if (strcasecmp("duty_cycle", key) == 0) {
		rem->duty_cycle = s_strtoui(val);
		return (1);
	} else if (strcasecmp("baud", key) == 0) {
		rem->baud = s_strtoui(val);
		return (1);
	} else if (strcasecmp("serial_mode", key) == 0) {
		if (val[0] < '5' || val[0] > '9') {
			logprintf(LOG_ERR, "error in configfile line %d:", line);
			logprintf(LOG_ERR, "bad bit count");
			parse_error = 1;
			return 0;
		}
		rem->bits_in_byte = val[0] - '0';
		switch (toupper(val[1])) {
		case 'N':
			rem->parity = IR_PARITY_NONE;
			break;
		case 'E':
			rem->parity = IR_PARITY_EVEN;
			break;
		case 'O':
			rem->parity = IR_PARITY_ODD;
			break;
		default:
			logprintf(LOG_ERR, "error in configfile line %d:", line);
			logprintf(LOG_ERR, "unsupported parity mode");
			parse_error = 1;
			return 0;
		}
		if (strcmp(val + 2, "1.5") == 0) {
			rem->stop_bits = 3;
		} else {
			rem->stop_bits = s_strtoui(val + 2) * 2;
		}
		return (1);
	} else if (val2 != NULL) {
		if (strcasecmp("header", key) == 0) {
			rem->phead = s_strtolirc_t(val);
			rem->shead = s_strtolirc_t(val2);
			return (2);
		} else if (strcasecmp("three", key) == 0) {
			rem->pthree = s_strtolirc_t(val);
			rem->sthree = s_strtolirc_t(val2);
			return (2);
		} else if (strcasecmp("two", key) == 0) {
			rem->ptwo = s_strtolirc_t(val);
			rem->stwo = s_strtolirc_t(val2);
			return (2);
		} else if (strcasecmp("one", key) == 0) {
			rem->pone = s_strtolirc_t(val);
			rem->sone = s_strtolirc_t(val2);
			return (2);
		} else if (strcasecmp("zero", key) == 0) {
			rem->pzero = s_strtolirc_t(val);
			rem->szero = s_strtolirc_t(val2);
			return (2);
		} else if (strcasecmp("foot", key) == 0) {
			rem->pfoot = s_strtolirc_t(val);
			rem->sfoot = s_strtolirc_t(val2);
			return (2);
		} else if (strcasecmp("repeat", key) == 0) {
			rem->prepeat = s_strtolirc_t(val);
			rem->srepeat = s_strtolirc_t(val2);
			return (2);
		} else if (strcasecmp("pre", key) == 0) {
			rem->pre_p = s_strtolirc_t(val);
			rem->pre_s = s_strtolirc_t(val2);
			return (2);
		} else if (strcasecmp("post", key) == 0) {
			rem->post_p = s_strtolirc_t(val);
			rem->post_s = s_strtolirc_t(val2);
			return (2);
		}
	}
	if (val2) {
		logprintf(LOG_ERR, "error in configfile line %d:", line);
		logprintf(LOG_ERR, "unknown definiton: \"%s %s %s\"", key, val, val2);
	} else {
		logprintf(LOG_ERR, "error in configfile line %d:", line);
		logprintf(LOG_ERR, "unknown definiton or too few arguments: \"%s %s\"", key, val);
	}
	parse_error = 1;
	return (0);
}

static int sanityChecks(struct ir_remote *rem)
{
	struct ir_ncode *codes;
	struct ir_code_node *node;

	if (!rem->name) {
		logprintf(LOG_ERR, "you must specify a remote name");
		return 0;
	}
	if (rem->gap == 0) {
		logprintf(LOG_WARNING, "you should specify a valid gap value");
	}
	if (has_repeat_gap(rem) && is_const(rem)) {
		logprintf(LOG_WARNING, "repeat_gap will be ignored if CONST_LENGTH flag is set");
	}

	if (is_raw(rem))
		return 1;

	if ((rem->pre_data & gen_mask(rem->pre_data_bits)) != rem->pre_data) {
		logprintf(LOG_WARNING, "invalid pre_data found for %s", rem->name);
		rem->pre_data &= gen_mask(rem->pre_data_bits);
	}
	if ((rem->post_data & gen_mask(rem->post_data_bits)) != rem->post_data) {
		logprintf(LOG_WARNING, "invalid post_data found for %s", rem->name);
		rem->post_data &= gen_mask(rem->post_data_bits);
	}
	for (codes = rem->codes; codes->name != NULL; codes++) {
		if ((codes->code & gen_mask(rem->bits)) != codes->code) {
			logprintf(LOG_WARNING, "invalid code found for %s: %s", rem->name, codes->name);
			codes->code &= gen_mask(rem->bits);
		}
		for (node = codes->next; node != NULL; node = node->next) {
			if ((node->code & gen_mask(rem->bits)) != node->code) {
				logprintf(LOG_WARNING, "invalid code found for %s: %s", rem->name, codes->name);
				node->code &= gen_mask(rem->bits);
			}
		}
	}

	return 1;
}

struct ir_remote *sort_by_bit_count(struct ir_remote *remotes)
{
	struct ir_remote *top, *rem, *next, *prev, *scan;

	rem = remotes;
	top = NULL;
	while (rem != NULL) {
		next = rem->next;

		scan = top;
		prev = NULL;
		while (scan && bit_count(scan) <= bit_count(rem)) {
			prev = scan;
			scan = scan->next;
		}
		if (prev) {
			prev->next = rem;
		} else {
			top = rem;
		}
		if (scan) {
			rem->next = scan;
		} else {
			rem->next = NULL;
		}

		rem = next;
	}

	return top;
}

static const char *lirc_parse_include(char *s)
{
	char *last;
	size_t len;

	len = strlen(s);
	if (len < 2) {
		return NULL;
	}
	last = s + len - 1;
	while (last > s && strchr(whitespace, *last) != NULL) {
		last--;
	}
	if (last <= s) {
		return NULL;
	}
	if (*s != '"' && *s != '<') {
		return NULL;
	}
	if (*s == '"' && *last != '"') {
		return NULL;
	} else if (*s == '<' && *last != '>') {
		return NULL;
	}
	*last = 0;
	memmove(s, s + 1, len - 2 + 1);	/* terminating 0 is copied, and
					   maybe more, but we don't care */
	return s;
}

static const char *lirc_parse_relative(char *dst, size_t dst_size, const char *child, const char *current)
{
	char *dir;
	size_t dirlen;

	if (!current)
		return child;

	/* Not a relative path */
	if (*child == '/')
		return child;

	if (strlen(current) >= dst_size) {
		return NULL;
	}
	strcpy(dst, current);
	dir = dirname(dst);
	dirlen = strlen(dir);
	if (dir != dst) {
		memmove(dst, dir, dirlen + 1);
	}

	if (dirlen + 1 + strlen(child) + 1 > dst_size) {
		return NULL;
	}
	strcat(dst, "/");
	strcat(dst, child);

	return dst;
}

struct ir_remote *read_config(FILE * f, const char *name)
{
	return read_config_recursive(f, name, 0);
}

static struct ir_remote *read_config_recursive(FILE * f, const char *name, int depth)
{
	char buf[LINE_LEN + 1], *key, *val, *val2;
	int len, argc;
	struct ir_remote *top_rem = NULL, *rem = NULL;
	struct void_array codes_list, raw_codes, signals;
	struct ir_ncode raw_code = { NULL, 0, 0, NULL };
	struct ir_ncode name_code = { NULL, 0, 0, NULL };
	struct ir_ncode *code;
	int mode = ID_none;

	line = 0;
	parse_error = 0;

	while (fgets(buf, LINE_LEN, f) != NULL) {
		line++;
		len = strlen(buf);
		if (len == LINE_LEN && buf[len - 1] != '\n') {
			logprintf(LOG_ERR, "line %d too long in config file", line);
			parse_error = 1;
			break;
		}

		if (len > 0) {
			len--;
			if (buf[len] == '\n')
				buf[len] = 0;
		}
		if (len > 0) {
			len--;
			if (buf[len] == '\r')
				buf[len] = 0;
		}
		/* ignore comments */
		if (buf[0] == '#') {
			continue;
		}
		key = strtok(buf, whitespace);
		/* ignore empty lines */
		if (key == NULL)
			continue;
		val = strtok(NULL, whitespace);
		if (val != NULL) {
			val2 = strtok(NULL, whitespace);
			if (strcasecmp("include", key) == 0) {
				FILE *childFile;
				const char *childName;
				const char *fullPath;
				char result[FILENAME_MAX + 1];

				if (depth > MAX_INCLUDES) {
					logprintf(LOG_ERR, "error opening child file defined at %s:%d", name, line);
					logprintf(LOG_ERR, "too many files included");
					parse_error = -1;
					break;
				}

				childName = lirc_parse_include(val);
				if (!childName) {
					logprintf(LOG_ERR, "error parsing child file value defined at line %d:", line);
					logprintf(LOG_ERR, "invalid quoting");
					parse_error = -1;
					break;
				}

				fullPath = lirc_parse_relative(result, sizeof(result), childName, name);
				if (!fullPath) {
					logprintf(LOG_ERR, "error composing relative file path defined at line %d:",
						  line);
					logprintf(LOG_ERR, "resulting path too long");
					parse_error = -1;
					break;
				}

				childFile = fopen(fullPath, "r");
				if (childFile == NULL) {
					logprintf(LOG_ERR, "error opening child file '%s' defined at line %d:",
						  fullPath, line);
					logprintf(LOG_ERR, "ignoring this child file for now.");
				} else {
					int save_line = line;

					if (!top_rem) {
						/* create first remote */
						rem = read_config_recursive(childFile, fullPath, depth + 1);
						if (rem != (void *)-1 && rem != NULL) {
							top_rem = rem;
						} else {
							rem = NULL;
						}
					} else {
						/* create new remote */
						rem->next = read_config_recursive(childFile, fullPath, depth + 1);
						if (rem->next != (void *)-1 && rem->next != NULL) {
							rem = rem->next;
						} else {
							rem->next = NULL;
						}
					}
					fclose(childFile);
					line = save_line;
				}
			} else if (strcasecmp("begin", key) == 0) {
				if (strcasecmp("codes", val) == 0) {
					/* init codes mode */
					if (!checkMode(mode, ID_remote, "begin codes"))
						break;
					if (rem->codes) {
						logprintf(LOG_ERR, "error in configfile line %d:", line);
						logprintf(LOG_ERR, "codes are already defined");
						parse_error = 1;
						break;
					}

					init_void_array(&codes_list, 30, sizeof(struct ir_ncode));
					mode = ID_codes;
				} else if (strcasecmp("raw_codes", val) == 0) {
					/* init raw_codes mode */
					if (!checkMode(mode, ID_remote, "begin raw_codes"))
						break;
					if (rem->codes) {
						logprintf(LOG_ERR, "error in configfile line %d:", line);
						logprintf(LOG_ERR, "codes are already defined");
						parse_error = 1;
						break;
					}
					set_protocol(rem, RAW_CODES);
					raw_code.code = 0;
					init_void_array(&raw_codes, 30, sizeof(struct ir_ncode));
					mode = ID_raw_codes;
				} else if (strcasecmp("remote", val) == 0) {
					/* create new remote */
					if (!checkMode(mode, ID_none, "begin remote"))
						break;
					mode = ID_remote;
					if (!top_rem) {
						/* create first remote */
						rem = top_rem = s_malloc(sizeof(struct ir_remote));
					} else {
						/* create new remote */
						rem->next = s_malloc(sizeof(struct ir_remote));;
						rem = rem->next;
					}
				} else if (mode == ID_codes) {
					code = defineCode(key, val, &name_code);
					while (!parse_error && val2 != NULL) {
						if (val2[0] == '#')
							break;	/* comment */
						val2 = strtok(NULL, whitespace);
					}
					code->current = NULL;
					add_void_array(&codes_list, code);
				} else {
					logprintf(LOG_ERR, "error in configfile line %d:", line);
					logprintf(LOG_ERR, "unknown section \"%s\"", val);
					parse_error = 1;
				}
				if (!parse_error && val2 != NULL) {
					logprintf(LOG_WARNING, "garbage after '%s' token in line %d ignored", val,
						  line);
				}
			} else if (strcasecmp("end", key) == 0) {

				if (strcasecmp("codes", val) == 0) {
					/* end Codes mode */
					if (!checkMode(mode, ID_codes, "end codes"))
						break;
					rem->codes = get_void_array(&codes_list);
					mode = ID_remote;	/* switch back */

				} else if (strcasecmp("raw_codes", val) == 0) {
					/* end raw codes mode */

					if (mode == ID_raw_name) {
						raw_code.signals = get_void_array(&signals);
						raw_code.length = signals.nr_items;
						if (raw_code.length % 2 == 0) {
							logprintf(LOG_ERR, "error in configfile line %d:", line);
							logprintf(LOG_ERR, "bad signal length");
							parse_error = 1;
						}
						if (!add_void_array(&raw_codes, &raw_code))
							break;
						mode = ID_raw_codes;
					}
					if (!checkMode(mode, ID_raw_codes, "end raw_codes"))
						break;
					rem->codes = get_void_array(&raw_codes);
					mode = ID_remote;	/* switch back */
				} else if (strcasecmp("remote", val) == 0) {
					/* end remote mode */
					/* print_remote(rem); */
					if (!checkMode(mode, ID_remote, "end remote"))
						break;
					if (!sanityChecks(rem)) {
						parse_error = 1;
						break;
					}
#                                       ifdef DYNCODES
					if (rem->dyncodes_name == NULL) {
						rem->dyncodes_name = s_strdup("unknown");
					}
					rem->dyncodes[0].name = rem->dyncodes_name;
					rem->dyncodes[1].name = rem->dyncodes_name;
#                                       endif
					/* not really necessary because we
					   clear the alloced memory */
					rem->next = NULL;
					rem->last_code = NULL;
					mode = ID_none;	/* switch back */
				} else if (mode == ID_codes) {
					code = defineCode(key, val, &name_code);
					while (!parse_error && val2 != NULL) {
						if (val2[0] == '#')
							break;	/* comment */
						val2 = strtok(NULL, whitespace);
					}
					code->current = NULL;
					add_void_array(&codes_list, code);
				} else {
					logprintf(LOG_ERR, "error in configfile line %d:", line);
					logprintf(LOG_ERR, "unknown section %s", val);
					parse_error = 1;
				}
				if (!parse_error && val2 != NULL) {
					logprintf(LOG_WARNING, "garbage after '%s'" " token in line %d ignored", val,
						  line);
				}
			} else {
				switch (mode) {
				case ID_remote:
					argc = defineRemote(key, val, val2, rem);
					if (!parse_error
					    && ((argc == 1 && val2 != NULL)
						|| (argc == 2 && val2 != NULL && strtok(NULL, whitespace) != NULL))) {
						logprintf(LOG_WARNING, "garbage after '%s'" " token in line %d ignored",
							  key, line);
					}
					break;
				case ID_codes:
					code = defineCode(key, val, &name_code);
					while (!parse_error && val2 != NULL) {
						if (val2[0] == '#')
							break;	/* comment */
						val2 = strtok(NULL, whitespace);
					}
					code->current = NULL;
					add_void_array(&codes_list, code);
					break;
				case ID_raw_codes:
				case ID_raw_name:
					if (strcasecmp("name", key) == 0) {
						if (mode == ID_raw_name) {
							raw_code.signals = get_void_array(&signals);
							raw_code.length = signals.nr_items;
							if (raw_code.length % 2 == 0) {
								logprintf(LOG_ERR, "error in configfile line %d:",
									  line);
								logprintf(LOG_ERR, "bad signal length");
								parse_error = 1;
							}
							if (!add_void_array(&raw_codes, &raw_code))
								break;
						}
						if (!(raw_code.name = s_strdup(val))) {
							break;
						}
						raw_code.code++;
						init_void_array(&signals, 50, sizeof(lirc_t));
						mode = ID_raw_name;
						if (!parse_error && val2 != NULL) {
							logprintf(LOG_WARNING,
								  "garbage after '%s'" " token in line %d ignored", key,
								  line);
						}
					} else {
						if (mode == ID_raw_codes) {
							logprintf(LOG_ERR, "no name for signal defined at line %d",
								  line);
							parse_error = 1;
							break;
						}
						if (!addSignal(&signals, key))
							break;
						if (!addSignal(&signals, val))
							break;
						if (val2) {
							if (!addSignal(&signals, val2)) {
								break;
							}
						}
						while ((val = strtok(NULL, whitespace))) {
							if (!addSignal(&signals, val))
								break;
						}
					}
					break;
				}
			}
		} else if (mode == ID_raw_name) {
			if (!addSignal(&signals, key)) {
				break;
			}
		} else {
			logprintf(LOG_ERR, "error in configfile line %d", line);
			parse_error = 1;
			break;
		}
		if (parse_error) {
			break;
		}
	}
	if (mode != ID_none) {
		switch (mode) {
		case ID_raw_name:
			if (raw_code.name != NULL) {
				free(raw_code.name);
				if (get_void_array(&signals) != NULL)
					free(get_void_array(&signals));
			}
		case ID_raw_codes:
			rem->codes = get_void_array(&raw_codes);
			break;
		case ID_codes:
			rem->codes = get_void_array(&codes_list);
			break;
		}
		if (!parse_error) {
			logprintf(LOG_ERR, "unexpected end of file");
			parse_error = 1;
		}
	}
	if (parse_error) {
		static int print_error = 1;

		if (print_error) {
			logprintf(LOG_ERR, "reading of file '%s' failed", name);
			print_error = 0;
		}
		free_config(top_rem);
		if (depth == 0)
			print_error = 1;
		return ((void *)-1);
	}
	/* kick reverse flag */
	/* handle RC6 flag to be backwards compatible: previous RC-6
	   config files did not set rc6_mask */
	rem = top_rem;
	while (rem != NULL) {
		if ((!is_raw(rem)) && rem->flags & REVERSE) {
			struct ir_ncode *codes;

			if (has_pre(rem)) {
				rem->pre_data = reverse(rem->pre_data, rem->pre_data_bits);
			}
			if (has_post(rem)) {
				rem->post_data = reverse(rem->post_data, rem->post_data_bits);
			}
			codes = rem->codes;
			while (codes->name != NULL) {
				codes->code = reverse(codes->code, rem->bits);
				codes++;
			}
			rem->flags = rem->flags & (~REVERSE);
			rem->flags = rem->flags | COMPAT_REVERSE;
			/* don't delete the flag because we still need
			   it to remain compatible with older versions
			 */
		}
		if (rem->flags & RC6 && rem->rc6_mask == 0 && rem->toggle_bit > 0) {
			int all_bits = bit_count(rem);

			rem->rc6_mask = ((ir_code) 1) << (all_bits - rem->toggle_bit);
		}
		if (rem->toggle_bit > 0) {
			int all_bits = bit_count(rem);

			if (has_toggle_bit_mask(rem)) {
				logprintf(LOG_WARNING, "%s uses both toggle_bit and toggle_bit_mask", rem->name);
			} else {
				rem->toggle_bit_mask = ((ir_code) 1) << (all_bits - rem->toggle_bit);
			}
			rem->toggle_bit = 0;
		}
		if (has_toggle_bit_mask(rem)) {
			if (!is_raw(rem) && rem->codes) {
				rem->toggle_bit_mask_state = (rem->codes->code & rem->toggle_bit_mask);
				if (rem->toggle_bit_mask_state) {
					/* start with state set to 0 for backwards compatibility */
					rem->toggle_bit_mask_state ^= rem->toggle_bit_mask;
				}
			}
		}
		if (is_serial(rem)) {
			lirc_t base;

			if (rem->baud > 0) {
				base = 1000000 / rem->baud;
				if (rem->pzero == 0 && rem->szero == 0) {
					rem->pzero = base;
				}
				if (rem->pone == 0 && rem->sone == 0) {
					rem->sone = base;
				}
			}
			if (rem->bits_in_byte == 0) {
				rem->bits_in_byte = 8;
			}
		}
		if (rem->min_code_repeat > 0) {
			if (!has_repeat(rem) || rem->min_code_repeat > rem->min_repeat) {
				logprintf(LOG_WARNING, "invalid min_code_repeat value");
				rem->min_code_repeat = 0;
			}
		}
		calculate_signal_lengths(rem);
		rem = rem->next;
	}

	top_rem = sort_by_bit_count(top_rem);
#       if defined(DEBUG) && !defined(DAEMONIZE)
	/*fprint_remotes(stderr, top_rem); */
#       endif
	return (top_rem);
}

void calculate_signal_lengths(struct ir_remote *remote)
{
	if (is_const(remote)) {
		remote->min_total_signal_length = min_gap(remote);
		remote->max_total_signal_length = max_gap(remote);
	} else {
		remote->min_gap_length = min_gap(remote);
		remote->max_gap_length = max_gap(remote);
	}

	lirc_t min_signal_length = 0, max_signal_length = 0;
	lirc_t max_pulse = 0, max_space = 0;
	int first_sum = 1;
	struct ir_ncode *c = remote->codes;
	int i;

	while (c->name) {
		struct ir_ncode code = *c;
		struct ir_code_node *next = code.next;
		int first = 1;
		int repeat = 0;
		do {
			if (first) {
				first = 0;
			} else {
				code.code = next->code;
				next = next->next;
			}
			for (repeat = 0; repeat < 2; repeat++) {
				if (init_sim(remote, &code, repeat)) {
					lirc_t sum = send_buffer.sum;

					if (sum) {
						if (first_sum || sum < min_signal_length) {
							min_signal_length = sum;
						}
						if (first_sum || sum > max_signal_length) {
							max_signal_length = sum;
						}
						first_sum = 0;
					}
					for (i = 0; i < send_buffer.wptr; i++) {
						if (i & 1) {	/* space */
							if (send_buffer.data[i] > max_space) {
								max_space = send_buffer.data[i];
							}
						} else {	/* pulse */

							if (send_buffer.data[i] > max_pulse) {
								max_pulse = send_buffer.data[i];
							}
						}
					}
				}
			}
		} while (next);
		c++;
	}
	if (first_sum) {
		/* no timing data, so assume gap is the actual total
		   length */
		remote->min_total_signal_length = min_gap(remote);
		remote->max_total_signal_length = max_gap(remote);
		remote->min_gap_length = min_gap(remote);
		remote->max_gap_length = max_gap(remote);
	} else if (is_const(remote)) {
		if (remote->min_total_signal_length > max_signal_length) {
			remote->min_gap_length = remote->min_total_signal_length - max_signal_length;
		} else {
			logprintf(LOG_WARNING, "min_gap_length is 0 for '%s' remote", remote->name);
			remote->min_gap_length = 0;
		}
		if (remote->max_total_signal_length > min_signal_length) {
			remote->max_gap_length = remote->max_total_signal_length - min_signal_length;
		} else {
			logprintf(LOG_WARNING, "max_gap_length is 0 for '%s' remote", remote->name);
			remote->max_gap_length = 0;
		}
	} else {
		remote->min_total_signal_length = min_signal_length + remote->min_gap_length;
		remote->max_total_signal_length = max_signal_length + remote->max_gap_length;
	}
}

void free_config(struct ir_remote *remotes)
{
	struct ir_remote *next;
	struct ir_ncode *codes;

	while (remotes != NULL) {
		next = remotes->next;

#               ifdef DYNCODES
		if (remotes->dyncodes_name != NULL)
			free(remotes->dyncodes_name);
#               endif
		if (remotes->name != NULL)
			free(remotes->name);
		if (remotes->codes != NULL) {
			codes = remotes->codes;
			while (codes->name != NULL) {
				struct ir_code_node *node, *next_node;

				free(codes->name);
				if (codes->signals != NULL)
					free(codes->signals);
				node = codes->next;
				while (node) {
					next_node = node->next;
					free(node);
					node = next_node;
				}
				codes++;
			}
			free(remotes->codes);
		}
		free(remotes);
		remotes = next;
	}
}
