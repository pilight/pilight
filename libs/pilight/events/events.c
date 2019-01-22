/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#ifndef _WIN32
	#ifdef __mips__
		#define __USE_UNIX98
	#endif
	#include <pthread.h>
	#include <sys/time.h>
	#include <unistd.h>
#endif

#include "../core/pilight.h"
#include "../core/common.h"
#include "../core/log.h"
#include "../core/cast.h"
#include "../core/options.h"
#include "../core/json.h"
#include "../core/ssdp.h"
#include "../core/socket.h"
#include "../datatypes/stack.h"

#include "../lua_c/lua.h"

#include "../protocols/protocol.h"
#include "../storage/storage.h"

#include "events.h"

#include "operator.h"
#include "function.h"
#include "action.h"

#ifdef _WIN32
	#include <windows.h>
#endif

typedef struct lexer_t {
	int pos;
	int ppos;
	int len;
	char *current_char;
	struct token_t *current_token;
	char *text;
} lexer_t;

typedef struct token_t {
	int type;
	int pos;
	char *value;
} token_t;

typedef struct tree_t {
	struct token_t *token;
	struct tree_t **child;
	int nrchildren;
} tree_t;

// static struct token_string {
	// char *name;
// } token_string[] = {
	// { "TOPERATOR" },
	// { "TFUNCTION" },
	// { "TSTRING" },
	// { "TINTEGER" },
	// { "TEOF" },
	// { "LPAREN" },
	// { "RPAREN" },
	// { "TCOMMA" },
	// { "TIF" },
	// { "TELSE" },
	// { "TTHEN" },
	// { "TEND" },
	// { "TACTION" }
// };

typedef enum {
	TOPERATOR = 0,
	TFUNCTION = 1,
	TSTRING = 2,
	TINTEGER = 3,
	TEOF = 4,
	LPAREN = 5,
	RPAREN = 6,
	TCOMMA = 7,
	TIF = 8,
	TELSE = 9,
	TTHEN = 10,
	TEND = 11,
	TACTION = 12
} token_types;

static unsigned short loop = 1;
static char *dummy = "a";
static char true_[2];
static char false_[2];
static char dot_[2];

static int running = 0;

static int get_precedence(char *symbol) {
	struct plua_module_t *modules = plua_get_modules();
	int len = 0, x = 0;
	while(modules) {
		if(modules->type == OPERATOR) {
			len = strlen(modules->name);
			if(strnicmp(symbol, modules->name, len) == 0) {
				event_operator_associativity(modules->name, &x);
				return x;
			}
		}
		modules = modules->next;
	}
	return -1;
}

static int get_associativity(char *symbol) {
	struct plua_module_t *modules = plua_get_modules();
	int len = 0, x = 0;
	while(modules) {
		if(modules->type == OPERATOR) {
			len = strlen(modules->name);
			if(strnicmp(symbol, modules->name, len) == 0) {
				event_operator_precedence(modules->name, &x);
				return x;
			}
		}
		modules = modules->next;
	}
	return -1;
}

static int is_action(char *symbol, int size) {
	struct plua_module_t *modules = plua_get_modules();
	int len = 0;
	while(modules) {
		if(modules->type == ACTION) {
			len = strlen(modules->name);
			if((size == -1 && strnicmp(symbol, modules->name, len) == 0) ||
				(size == len && strnicmp(symbol, modules->name, len) == 0)) {
				return len;
			}
		}
		modules = modules->next;
	}
	return -1;
}

static int is_function(char *symbol, int size) {
	struct plua_module_t *modules = plua_get_modules();
	int len = 0;
	while(modules) {
		if(modules->type == FUNCTION) {
			len = strlen(modules->name);
			if((size == -1 && strnicmp(symbol, modules->name, len) == 0) ||
				(size == len && strnicmp(symbol, modules->name, len) == 0)) {
				return len;
			}
		}
		modules = modules->next;
	}
	return -1;
}

static int is_operator(char *symbol, int size) {
	struct plua_module_t *modules = plua_get_modules();
	int len = 0;

	while(modules) {
		if(modules->type == OPERATOR) {
			len = strlen(modules->name);
			if((size == -1 && strnicmp(symbol, modules->name, len) == 0) ||
				(size == len && strnicmp(symbol, modules->name, len) == 0)) {
				return len;
			}
		}
		modules = modules->next;
	}
	return -1;
}

void events_tree_gc(struct tree_t *tree) {
	int i = 0;
	if(tree == NULL) {
		return;
	}
	for(i=0;i<tree->nrchildren;i++) {
		events_tree_gc(tree->child[i]);
	}
	if(tree->token != NULL) {
		FREE(tree->token->value);
		FREE(tree->token);
	}
	if(tree->nrchildren > 0) {
		FREE(tree->child);
	}
	FREE(tree);
}

int events_gc(void) {
	loop = 0;

	while(running == 1) {
#ifdef _WIN32
		SleepEx(10, TRUE);
#else
		usleep(10);
#endif
	}

	event_operator_gc();
	event_action_gc();
	event_function_gc();
	logprintf(LOG_DEBUG, "garbage collected events library");
	return 1;
}

static int print_error(struct lexer_t *lexer, struct tree_t *p, struct tree_t *node, struct token_t *token, int err, char *expected, int pos, int trunc) {
	char *p_elipses = "...";
	char *s_elipses = "...";
	char *s_tmp = "";
	char *p_tmp = "";
	int length = lexer->ppos+25;

	trunc -= 15;

	if(trunc < 0) {
		trunc = 0;
	}

	if(err == -1) {
		while(trunc > 0 && lexer->text[trunc] != ' ') {
			trunc--;
		}
		if(trunc > 0) {
			p_tmp = p_elipses;
		}
		if(length < strlen(&lexer->text[trunc])) {
			while(length < strlen(&lexer->text[trunc]) && lexer->text[length+trunc] != ' ') {
				length++;
			}
			s_tmp = s_elipses;
		}
		if(expected != NULL) {
			logprintf(LOG_ERR,
				"\n%s%.*s %s\n%*s^ unexpected symbol, expected %s",
				p_tmp, length, &lexer->text[trunc], s_tmp, pos+strlen(p_tmp)-trunc, " ", expected);
		} else {
			logprintf(LOG_ERR,
				"\n%s%.*s %s\n%*s^ unexpected symbol",
				p_tmp, length, &lexer->text[trunc], s_tmp, pos+strlen(p_tmp)-trunc, " ");
		}
		err = -2;
	}
	if(node != NULL) {
		events_tree_gc(node);
		node = NULL;
	}
	if(lexer->current_token != NULL) {
		FREE(lexer->current_token->value);
		FREE(lexer->current_token);
	}
	if(token != NULL) {
		FREE(token->value);
		FREE(token);
	}
	if(p != NULL) {
		events_tree_gc(p);
	}

	return err;
}

/*
 * TESTME: Check if right devices are cached.
 */
void event_cache_device(struct rules_t *obj, char *device) {
	int exists = 0;
	int o = 0;

	if(obj != NULL) {
		for(o=0;o<obj->nrdevices;o++) {
			if(strcmp(obj->devices[o], device) == 0) {
				exists = 1;
				break;
			}
		}
		if(exists == 0) {
			/* Store all devices that are present in this rule */
			if((obj->devices = REALLOC(obj->devices, sizeof(char *)*(unsigned int)(obj->nrdevices+1))) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			if((obj->devices[obj->nrdevices] = MALLOC(strlen(device)+1)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			strcpy(obj->devices[obj->nrdevices], device);
			obj->nrdevices++;
		}
	}
}

/*
 * This functions checks if the defined event variable
 * is part of one of devices in the config. If it is,
 * replace the variable with the actual value
 *
 * Return codes:
 * -1: An error was found and abort rule parsing
 * 0: Found variable and filled varcont
 * 1: Did not find variable and did not fill varcont
 */
static int event_lookup_variable(char *var, struct rules_t *obj, struct varcont_t *varcont, unsigned short validate, int in_action) {
	int recvtype = 0;
	// int cached = 0;
	if(strcmp(true_, "1") != 0) {
		strcpy(true_, "1");
	}
	if(strcmp(dot_, ".") != 0) {
		strcpy(dot_, ".");
	}
	if(strcmp(false_, "0") != 0) {
		strcpy(false_, "0");
	}
	if(strcmp(var, "1") == 0 || strcmp(var, "0") == 0 ||
		 strcmp(var, "true") == 0 ||
		 strcmp(var, "false") == 0) {
		if(strcmp(var, "true") == 0) {
			varcont->number_ = 1;
		} else if(strcmp(var, "false") == 0) {
			varcont->number_ = 0;
		} else {
			varcont->number_ = atof(var);
		}
		varcont->decimals_ = 0;
		varcont->type_ = JSON_NUMBER;

		return 0;
	}

	int len = (int)strlen(var);
	int i = 0, ret = 0;
	int nrdots = 0;
	for(i=0;i<len;i++) {
		if(var[i] == '.') {
			nrdots++;
		}
	}

	if(nrdots == 1) {
		char **array = NULL;
		unsigned int n = explode(var, ".", &array);

		if(n < 2) {
			varcont->string_ = dot_;
			array_free(&array, n);
			varcont->type_ = JSON_STRING;
			return 0;
		}

		char *device = array[0];
		char *name = array[1];

		recvtype = 0;
		struct protocols_t *tmp_protocols = protocols;
		if(devices_select(ORIGIN_MASTER, device, NULL) == 0) {
			recvtype = 1;
		}
		if(recvtype == 0) {
			while(tmp_protocols) {
				if(strcmp(tmp_protocols->listener->id, device) == 0) {
					recvtype = 2;
					break;
				}
				tmp_protocols = tmp_protocols->next;
			}
		}

		unsigned int match1 = 0, match2 = 0, has_state = 0;

		if(recvtype == 2) {
			if(validate == 1) {
				if(in_action == 0) {
					event_cache_device(obj, device);
				}
				if(strcmp(name, "repeats") != 0 && strcmp(name, "uuid") != 0) {
					struct options_t *options = tmp_protocols->listener->options;
					while(options) {
						if(options->conftype == DEVICES_STATE) {
							has_state = 1;
						}
						if(strcmp(options->name, name) == 0) {
							match2 = 1;
							if(options->vartype == JSON_NUMBER) {
								varcont->number_ = 0;
								varcont->decimals_ = 0;
								varcont->type_ = JSON_NUMBER;
							}
							if(options->vartype == JSON_STRING) {
								varcont->string_ = dummy;
								varcont->type_ = JSON_STRING;
							}
						}
						options = options->next;
					}
					if(match2 == 0 && ((!(strcmp(name, "state") == 0 && has_state == 1)) || (strcmp(name, "state") != 0))) {
						logprintf(LOG_ERR, "rule #%d invalid: protocol \"%s\" has no field \"%s\"", obj->nr, device, name);
						varcont->string_ = NULL;
						varcont->number_ = 0;
						varcont->decimals_ = 0;
						array_free(&array, n);
						return -1;
					}
				} else if(!(strcmp(name, "repeats") == 0 || strcmp(name, "uuid") == 0)) {
					logprintf(LOG_ERR, "rule #%d invalid: protocol \"%s\" has no field \"%s\"", obj->nr, device, name);
					varcont->string_ = NULL;
					varcont->number_ = 0;
					varcont->decimals_ = 0;
					array_free(&array, n);
					return -1;
				}
			}
			struct JsonNode *jmessage = NULL, *jnode = NULL;
			if(obj->jtrigger != NULL) {
				if(((jnode = json_find_member(obj->jtrigger, name)) != NULL) ||
					 ((jmessage = json_find_member(obj->jtrigger, "message")) != NULL &&
					 (jnode = json_find_member(jmessage, name)) != NULL)) {
					if(jnode->tag == JSON_STRING) {
						varcont->string_ = jnode->string_;
						varcont->type_ = JSON_STRING;
						array_free(&array, n);
						return 0;
					} else if(jnode->tag == JSON_NUMBER) {
						varcont->number_ = jnode->number_;
						varcont->decimals_ = jnode->decimals_;
						varcont->type_ = JSON_NUMBER;
						array_free(&array, n);
						return 0;
					}
				}
			}
			array_free(&array, n);
			return 0;
		} else if(recvtype == 1) {
			if(validate == 1) {
				if(in_action == 0) {
					event_cache_device(obj, device);
				}
				char *tmp_state = NULL;
				i = 0, ret = 0;
				while(devices_get_state(ORIGIN_MASTER, device, i++, &tmp_state) != -1) {
					if(strcmp("state", name) == 0) {
						match1 = 1;
						match2 = 1;
						break;
					}
				}
				if(match1 == 0) {
					i = 0, ret = 0;
					while((ret = devices_has_parameter(ORIGIN_MASTER, device, i++, name)) != -1) {
						if(ret == 0) {
							match1 = 1;
							break;
						}
					}
					i = 0, ret = 0;
					while(devices_get_value(ORIGIN_MASTER, device, i++, name, NULL) != -1) {
						if(ret == 0) {
							match2 = 1;
							break;
						}
					}
					if(match2 == 0) {
						i = 0, ret = 0;
						while(devices_is_state(ORIGIN_MASTER, device, i++, name) != -1) {
							if(ret == 0) {
								match2 = 1;
								break;
							}
						}
					}
					if(match2 == 0) {
						i = 0, ret = 0;
						while(devices_get_string_setting(ORIGIN_MASTER, device, i++, name, NULL) != -1) {
							if(ret == 0) {
								match2 = 1;
								break;
							}
						}
					}
					if(match2 == 0) {
						i = 0, ret = 0;
						while(devices_get_number_setting(ORIGIN_MASTER, device, i++, name, NULL) != -1) {
							if(ret == 0) {
								match2 = 1;
								break;
							}
						}
					}
				}
				if(match1 == 0) {
					logprintf(LOG_ERR, "rule #%d invalid: device \"%s\" has no variable \"%s\"", obj->nr, device, name);
				} else if(match2 == 0) {
					logprintf(LOG_ERR, "rule #%d invalid: variable \"%s\" of device \"%s\" cannot be used in event rules", obj->nr, name, device);
				}
				if(match1 == 0 || match2 == 0) {
					varcont->string_ = NULL;
					varcont->number_ = 0;
					varcont->decimals_ = 0;
					array_free(&array, n);
					return -1;
				}
			}
			char *setting = NULL;
			struct varcont_t val;
			i = 0;

			while(devices_select_settings(ORIGIN_MASTER, device, i++, &setting, &val) == 0) {
				if(strcmp(setting, name) == 0) {
					if(val.type_ == JSON_STRING) {
						/* Cache values for faster future lookup */
						// if(obj != NULL) {
							// event_store_val_ptr(obj, device, name, tmp_settings);
						// }
						varcont->string_ = val.string_;
						varcont->type_ = JSON_STRING;
						array_free(&array, n);
						return 0;
					} else if(val.type_ == JSON_NUMBER) {
						/* Cache values for faster future lookup */
						// if(obj != NULL) {
							// event_store_val_ptr(obj, device, name, tmp_settings);
						// }
						varcont->number_ = val.number_;
						varcont->decimals_ = val.decimals_;
						varcont->type_ = JSON_NUMBER;
						array_free(&array, n);
						return 0;
					}
				}
			}
			logprintf(LOG_ERR, "rule #%d invalid: device \"%s\" has no variable \"%s\"", obj->nr, device, name);
			varcont->string_ = NULL;
			varcont->number_ = 0;
			varcont->decimals_ = 0;
			array_free(&array, n);
			return -1;
		}
		/*
		 * '21.00' comparison should be allowed and not be seen as config device
		 */
		/*else {
			logprintf(LOG_ERR, "rule #%d invalid: device \"%s\" does not exist in the config", obj->nr, device);
			varcont->string_ = NULL;
			varcont->number_ = 0;
			varcont->decimals_ = 0;
			array_free(&array, n);
			return -1;
		}*/
		array_free(&array, n);
	}
	/*
	 * Multiple dots should also be allowed and not be seen as config device
	 * e.g.: http://192.168.1.1/
	 */
	/*
		else if(nrdots > 2) {
		logprintf(LOG_ERR, "rule #%d invalid: variable \"%s\" is invalid", obj->nr, var);
		varcont->string_ = NULL;
		varcont->number_ = 0;
		varcont->decimals_ = 0;
		return -1;
	} */

	if(isNumeric(var) == 0) {
		varcont->number_ = atof(var);
		varcont->decimals_ = nrDecimals(var);
		varcont->type_ = JSON_NUMBER;
	} else {
		if((varcont->string_ = STRDUP(var)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		varcont->free_ = 1;
		varcont->type_ = JSON_STRING;
	}
	return 0;				
}

static int lexer_parse_integer(struct lexer_t *lexer, struct stack_dt *t) {
	int i = 0, nrdot = 0;
	if(isdigit(lexer->current_char[0])) {
		/*
		 * The dot cannot be the first character
		 * and we cannot have more than 1 dot
		 */
		while(lexer->pos <= lexer->len &&
				(
					isdigit(lexer->current_char[0]) ||
					(i > 0 && nrdot == 0 && lexer->current_char[0] == '.')
				)
			) {
			if(lexer->current_char[0] == '.') {
				nrdot++;
			}
			dt_stack_push(t, sizeof(char *), lexer->current_char);
			lexer->current_char = &lexer->text[lexer->pos++];
			i++;
		}
		return 0;
	} else {
		return -1;
	}
}

static int lexer_parse_quoted_string(struct lexer_t *lexer, struct stack_dt *t) {
	int x = lexer->current_char[0];
	if(lexer->pos < lexer->len) {
		lexer->current_char = &lexer->text[lexer->pos++];

		while(lexer->pos < lexer->len) {
			if(lexer->pos > 0 && strnicmp(&lexer->text[lexer->pos-1], "\\'", 2) == 0) {
				/*
				 * Skip escape
				 */
				lexer->current_char = &lexer->text[lexer->pos++];
				dt_stack_push(t, sizeof(char *), lexer->current_char);
				lexer->current_char = &lexer->text[lexer->pos++];
				continue;
			}
			if(lexer->pos > 0 && strnicmp(&lexer->text[lexer->pos-1], "\\\"", 2) == 0) {
				/*
				 * Skip escape
				 */
				lexer->current_char = &lexer->text[lexer->pos++];
				dt_stack_push(t, sizeof(char *), lexer->current_char);
				lexer->current_char = &lexer->text[lexer->pos++];
				continue;
			}
			if(lexer->current_char[0] == x) {
				break;
			}
			dt_stack_push(t, sizeof(char *), lexer->current_char);
			lexer->current_char = &lexer->text[lexer->pos++];
		}
		if(lexer->current_char[0] != '"' && lexer->current_char[0] != '\'') {
			dt_stack_free(t, NULL);
			return print_error(lexer, NULL, NULL, NULL, -1, "a ending quote", lexer->pos, lexer->ppos-1);
		} else if(lexer->pos <= lexer->len) {
			lexer->current_char = &lexer->text[lexer->pos++];
			return 0;
		} else {
			return -1;
		}
	}
	return -1;
}

static int lexer_parse_string(struct lexer_t *lexer, struct stack_dt *t) {
	while(lexer->pos <= lexer->len &&
				lexer->current_char[0] != ' ' &&
				lexer->current_char[0] != ',' &&
				lexer->current_char[0] != ')') {
		dt_stack_push(t, sizeof(char *), lexer->current_char);
		lexer->current_char = &lexer->text[lexer->pos++];
	}
	return 0;
}

static int lexer_parse_space(struct lexer_t *lexer) {
	while(lexer->pos <= lexer->len && lexer->current_char[0] == ' ') {
		lexer->current_char = &lexer->text[lexer->pos++];
	}
	return 0;
}

static char *stack_to_char(struct stack_dt *stack) {
	char *c = NULL;
	char *str = MALLOC(dt_stack_top(stack)+1);
	int i = 0;

	if(str == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	while((c = dt_stack_pop(stack, 0)) != NULL) {
		str[i++] = c[0];
	}
	str[i] = '\0';
	dt_stack_free(stack, NULL);
	return str;
}

static char *min(char *numbers[], int n){
	char *m = NULL;
	int	i = 0;

	for(i=0;i<n;i++) {
		if(numbers[i] != NULL) {
			m = numbers[i];
			break;
		}
	}
	if(m == NULL) {
		return NULL;
	}

	for(i=1;i<n;i++) {
		if(numbers[i] != NULL && m > numbers[i]) {
			m = numbers[i];
		}
	}

	return m;
}

static int lexer_next_token(struct lexer_t *lexer) {
	struct stack_dt *t = MALLOC(sizeof(struct stack_dt));
	int type = TEOF, ret = 0;

	if(t == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	memset(t, 0, sizeof(struct stack_dt));

	while(lexer->pos <= lexer->len) {
		lexer->ppos = lexer->pos;
		if(lexer->current_char[0] == ' ') {
			if(lexer_parse_space(lexer) == -1) {
				return -1;
			}
		}

		int x = 0;
		if(lexer->current_char[0] == '\'' || lexer->current_char[0] == '"') {
			ret = lexer_parse_quoted_string(lexer, t);
			type = TSTRING;
		} else if(isdigit(lexer->current_char[0])) {
			ret = lexer_parse_integer(lexer, t);
			type = TINTEGER;
		} else if(strnicmp(lexer->current_char, "if", 2) == 0) {
			dt_stack_push(t, sizeof(char *), &lexer->current_char[0]);
			dt_stack_push(t, sizeof(char *), &lexer->current_char[1]);
			type = TIF;
			lexer->pos += 2;
			lexer->current_char = &lexer->text[lexer->pos-1];
		} else if(strnicmp(lexer->current_char, "else", 4) == 0) {
			dt_stack_push(t, sizeof(char *), &lexer->current_char[0]);
			dt_stack_push(t, sizeof(char *), &lexer->current_char[1]);
			dt_stack_push(t, sizeof(char *), &lexer->current_char[2]);
			dt_stack_push(t, sizeof(char *), &lexer->current_char[3]);
			type = TELSE;
			lexer->pos += 4;
			lexer->current_char = &lexer->text[lexer->pos-1];
		} else if(strnicmp(lexer->current_char, "then", 4) == 0) {
			dt_stack_push(t, sizeof(char *), &lexer->current_char[0]);
			dt_stack_push(t, sizeof(char *), &lexer->current_char[1]);
			dt_stack_push(t, sizeof(char *), &lexer->current_char[2]);
			dt_stack_push(t, sizeof(char *), &lexer->current_char[3]);
			type = TTHEN;
			lexer->pos += 4;
			lexer->current_char = &lexer->text[lexer->pos-1];
		} else if(strnicmp(lexer->current_char, "end", 3) == 0) {
			dt_stack_push(t, sizeof(char *), &lexer->current_char[0]);
			dt_stack_push(t, sizeof(char *), &lexer->current_char[1]);
			dt_stack_push(t, sizeof(char *), &lexer->current_char[2]);
			type = TEND;
			lexer->pos += 3;
			lexer->current_char = &lexer->text[lexer->pos-1];
		} else if(lexer->current_char[0] == ',') {
			dt_stack_push(t, sizeof(char *), lexer->current_char);
			type = TCOMMA;
			lexer->current_char = &lexer->text[lexer->pos++];
		} else if(lexer->current_char[0] == '(') {
			dt_stack_push(t, sizeof(char *), lexer->current_char);
			type = LPAREN;
			lexer->current_char = &lexer->text[lexer->pos++];
		} else if(lexer->current_char[0] == ')') {
			dt_stack_push(t, sizeof(char *), lexer->current_char);
			type = RPAREN;
			lexer->current_char = &lexer->text[lexer->pos++];
		} else {
			char *pos[3] = { NULL }, *m = NULL;
			int len = lexer->len-(lexer->pos-1), len1 = 0;
			pos[0] = strstr(lexer->current_char, " ");
			pos[1] = strstr(lexer->current_char, "(");
			pos[2] = strstr(lexer->current_char, ",");
			if((m = min(pos, 3)) != NULL) {
				len = m-lexer->current_char;
			}
			if((len1 = is_function(lexer->current_char, len)) > 0) {
				for(x=0;x<len1;x++) {
					dt_stack_push(t, sizeof(char *), &lexer->current_char[x]);
				}
				type = TFUNCTION;
				lexer->current_char = &lexer->text[lexer->pos+(len1-1)];
				lexer->pos += len1;
			} else if((len1 = is_operator(lexer->current_char, len)) > 0) {
				for(x=0;x<len1;x++) {
					dt_stack_push(t, sizeof(char *), &lexer->current_char[x]);
				}
				type = TOPERATOR;
				lexer->current_char = &lexer->text[lexer->pos+(len1-1)];
				lexer->pos += len1;
			} else if((len1 = is_action(lexer->current_char, len)) > 0) {
				for(x=0;x<len1;x++) {
					dt_stack_push(t, sizeof(char *), &lexer->current_char[x]);
				}
				type = TACTION;
				lexer->current_char = &lexer->text[lexer->pos+(len1-1)];
				lexer->pos += len1;
			}
		}
		if(type == TEOF) {
			ret = lexer_parse_string(lexer, t);
			type = TSTRING;
		}
		if(type != TEOF && ret == 0) {
			if((lexer->current_token = MALLOC(sizeof(struct token_t))) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			lexer->current_token->value = stack_to_char(t);
			lexer->current_token->type = type;
			lexer->current_token->pos = lexer->pos;
			return 0;
		} else {
			return ret;
		}
	}
	if(t != NULL) {
		FREE(t);
	}

	lexer->current_token = NULL;
	return 0;
}

static struct tree_t *ast_parent(struct token_t *token) {
	struct tree_t *tree = MALLOC(sizeof(struct tree_t));
	if(tree == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	tree->child = NULL;
	tree->nrchildren = 0;
	tree->token = token;
	return tree;
}

static struct tree_t *ast_child(struct tree_t *p, struct tree_t *c) {
	if((p->child = REALLOC(p->child, sizeof(struct tree_t)*(p->nrchildren+1))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	p->child[p->nrchildren] = c;
	p->nrchildren++;
	return p;
}

static int lexer_peek(struct lexer_t *lexer, int skip, int type, char *val) {
	int pos = lexer->pos, i = 0, ret = -1;
	struct token_t *tmp = lexer->current_token;
	char *c = lexer->current_char;

	if(skip > 0) {
		for(i=0;i<skip;i++) {
			if((ret = lexer_next_token(lexer)) < 0) {
				goto bad;
			}
			if(i < skip-1 && lexer->current_token != NULL) {
				FREE(lexer->current_token->value);
				FREE(lexer->current_token);
			}
		}
	}

	if(type == TEOF && lexer->current_token == NULL) {
		goto good;
	} else if(lexer->current_token != NULL && lexer->current_token->type == type) {
		if(val != NULL) {
			if(stricmp(lexer->current_token->value, val) == 0) {
				goto good;
			} else {
				ret = -1;
				goto bad;
			}
		} else {
			goto good;
		}
	} else {
		ret = -1;
		goto bad;
	}

good:
	if(skip > 0 && lexer->current_token != NULL) {
		FREE(lexer->current_token->value);
		FREE(lexer->current_token);
	}
	lexer->pos = pos;
	lexer->current_token = tmp;
	lexer->current_char = c;
	return 0;
bad:
	if(skip > 0 && lexer->current_token != NULL) {
		FREE(lexer->current_token->value);
		FREE(lexer->current_token);
	}
	lexer->pos = pos;
	lexer->current_token = tmp;
	lexer->current_char = c;

	return ret;
}

static int lexer_eat(struct lexer_t *lexer, int type, struct token_t **token_out) {
	struct token_t *token = lexer->current_token;
	int ret = -1;

	if((type == TEOF && lexer->current_token == NULL) ||
	   (lexer->current_token != NULL && lexer->current_token->type == type)) {
		if((ret = lexer_next_token(lexer)) < 0) {
			*token_out = NULL;
			return ret;
		}
	} else {
		if(lexer->current_token != NULL) {
			FREE(lexer->current_token->value);
			FREE(lexer->current_token);
		}
		*token_out = NULL;
		return -1;
	}
	*token_out = token; 
	return 0;
}

static int lexer_expr(struct lexer_t *lexer, struct tree_t *tree, struct tree_t **tree_out);
static int lexer_term(struct lexer_t *lexer, struct tree_t *tree, int precedence, struct tree_t **tree_out);

static int lexer_factor(struct lexer_t *lexer, struct tree_t *tree_in, struct tree_t **tree_out) {
	struct token_t *token = lexer->current_token;
	struct token_t *token_ret = NULL;
	char *expected = NULL;
	int pos = 0, err = -2;

	if(token != NULL) {
		switch(token->type) {
			case TSTRING: {
				if((err = lexer_eat(lexer, TSTRING, &token_ret)) < 0) {
					goto error;
				}
				*tree_out = ast_parent(token);
				return 0;
			} break;
			case TINTEGER: {
				if((err = lexer_eat(lexer, TINTEGER, &token_ret)) < 0) {
					goto error;
				}
				*tree_out = ast_parent(token);
				return 0;
			} break;
			case LPAREN: {
				if((err = lexer_eat(lexer, LPAREN, &token_ret)) < 0) {
					goto error;
				}
				FREE(token_ret->value);
				FREE(token_ret);
				if((err = lexer_expr(lexer, tree_in, tree_out)) < 0) {
					goto error;
				}
				pos = (*tree_out)->token->pos+1;
				if((err = lexer_eat(lexer, RPAREN, &token_ret)) < 0) {
					char *tmp = "a closing parenthesis";
					expected = tmp;
					pos += 1;
					goto error;
				}
				FREE(token_ret->value);
				FREE(token_ret);
				return 0;
			} break;
		}
	}

	*tree_out = NULL;
	return -1;

error:
	return print_error(lexer, NULL, *tree_out, token_ret, err, expected, pos, lexer->ppos-1);
}

static int lexer_parse_function(struct lexer_t *lexer, struct tree_t *tree_in, struct tree_t **tree_out) {
	struct tree_t *node = NULL;
	struct token_t *token = lexer->current_token, *token_ret = NULL;
	struct tree_t *p = ast_parent(token);
	char *expected = NULL;
	int pos = 0, err = -1, loop = 1;

	if((err = lexer_eat(lexer, TFUNCTION, &token_ret)) < 0) {
		*tree_out = NULL;
		return err;
	}
	if((err = lexer_eat(lexer, LPAREN, &token_ret)) < 0) {
		*tree_out = NULL;
		return err;
	}
	FREE(token_ret->value);
	FREE(token_ret);

	if((err = lexer_term(lexer, tree_in, 0, &node)) == 0) {
		ast_child(p, node);
	} else {
		events_tree_gc(p);
		*tree_out = NULL;
		return err;
	}
	pos = node->token->pos+1;

	if(lexer_peek(lexer, 0, RPAREN, NULL) == 0) {
		loop = 0;
	}
	while(loop) {
		if((err = lexer_eat(lexer, TCOMMA, &token_ret)) < 0) {
			char *tmp = "a comma or closing parenthesis";
			pos -= strlen(node->token->value);
			lexer->ppos -= 2;
			expected = tmp;
			events_tree_gc(p);
			err = -1;
			goto error;
		}
		FREE(token_ret->value);
		FREE(token_ret);
		if(lexer_term(lexer, tree_in, 0, &node) == 0) {
			ast_child(p, node);
		}
		if(lexer_peek(lexer, 0, RPAREN, NULL) == 0) {
			break;
		}
		pos = node->token->pos+1;
	}

	if(lexer->current_token == NULL) {
		char *tmp = "a closing parenthesis";
		expected = tmp;
		err = -1;
		goto error;
	}

	if((err = lexer_eat(lexer, RPAREN, &token_ret)) < 0) {
		char *tmp = "a closing parenthesis";
		expected = tmp;
		pos = lexer->pos;
		err = -1;
		goto error;
	}
	FREE(token_ret->value);
	FREE(token_ret);

	*tree_out = p;
	return 0;

error:
	return print_error(lexer, NULL, NULL, NULL, err, expected, pos, lexer->ppos-1);
}

static void print_ast(struct tree_t *tree);

static int lexer_parse_action(struct lexer_t *lexer, struct tree_t *tree_in, struct tree_t **tree_out) {
	struct tree_t *node = NULL;
	struct tree_t *p = NULL;
	struct tree_t *p1 = NULL;
	struct tree_t *tree_ret = NULL;
	struct token_t *token = lexer->current_token, *token_ret = NULL;
	char *expected = NULL;
	int len = strlen(token->value), match = 0, pos = 0, err = -1;

	p = ast_parent(token);

	if(is_action(token->value, len) > 0) {
		if((err = lexer_eat(lexer, TACTION, &token_ret)) < 0) {
			*tree_out = NULL;
			return -1;
		}
		/*
		 * In case we enter a recursive action action, we
		 * want to be able to free the string leading to
		 * that situation. E.g.:
		 *
		 * ... THEN switch DEVICE switch
		 *
		 * We want to be able to free the 'DEVICE' string.
		 */
		if(lexer_peek(lexer, 0, TSTRING, NULL) < 0) {
			if(lexer_peek(lexer, 0, TACTION, NULL) == 0) {
				char *tmp = "a string but got an action";
				expected = tmp;
			} else {
				char *tmp = "an action argument";
				expected = tmp;
			}
			pos = token->pos;
			err = -1;
			goto error;
		}
		while(1) {
			pos = lexer->pos;
			if(lexer_peek(lexer, 0, TEOF, NULL) == 0 ||
				 lexer_peek(lexer, 0, TELSE, NULL) == 0 ||
				 lexer_peek(lexer, 0, TEND, NULL) == 0) {
				break;
			}
			if(lexer_peek(lexer, 0, TACTION, NULL) == 0) {
				if(lexer_term(lexer, tree_in, 0, &tree_ret) == 0) {
					ast_child(p, tree_ret);
				}
				break;
			}
			if((err = lexer_eat(lexer, TSTRING, &token_ret)) < 0) {
				char *tmp = "an action argument";
				if(token_ret != NULL) {
					pos -= strlen(token_ret->value)+1;
				}
				lexer->ppos -= lexer->ppos-pos;
				expected = tmp;
				goto error;
			} else {
				match = 0;
				char **ret = NULL;
				int nr = 0, i = 0;
				if(event_action_get_parameters(token->value, &nr, &ret) == 0) {
					for(i=0;i<nr;i++) {
						if(stricmp(ret[i], token_ret->value) == 0 && match == 0) {
							match = 1;
						}
						FREE(ret[i]);
					}
					FREE(ret);
				}
				if(match == 0) {
					char *tmp = "an action argument";
					pos -= strlen(token_ret->value)+1;
					lexer->ppos -= lexer->ppos-pos;
					expected = tmp;
					FREE(token_ret->value);
					FREE(token_ret);
					err = -1;
					goto error;
				}
				p1 = ast_parent(token_ret);
				ast_child(p, p1);
				/*
				 * In case we a arguments is combined like this
				 * THEN switch BETWEEN on AND off
				 */
				if(lexer_peek(lexer, 1, TOPERATOR, "and") == 0) {
					while(lexer_peek(lexer, 1, TOPERATOR, "and") == 0) {
						if(lexer_peek(lexer, 0, TINTEGER, NULL) == 0) {
							if((err = lexer_eat(lexer, TINTEGER, &token_ret)) < 0) {
								char *tmp = "a string,  number or function";
								expected = tmp;
								err = -1;
								goto error;
							}
							ast_child(p1, ast_parent(token_ret));
						} else if(lexer_peek(lexer, 0, TSTRING, NULL) == 0) {
							if((err = lexer_eat(lexer, TSTRING, &token_ret)) < 0) {
								char *tmp = "a string,  number or function";
								expected = tmp;
								err = -1;
								goto error;
							}
							ast_child(p1, ast_parent(token_ret));
						}
						if((err = lexer_eat(lexer, TOPERATOR, &token_ret)) < 0) {
							char *tmp = "an 'and' operator";
							expected = tmp;
							err = -1;
							goto error;
						}
						FREE(token_ret->value);
						FREE(token_ret);
					}
					if(lexer_peek(lexer, 0, TINTEGER, NULL) == 0) {
						if((err = lexer_eat(lexer, TINTEGER, &token_ret)) < 0) {
							char *tmp = "a string, number or function";
							expected = tmp;
							goto error;
						}
						ast_child(p1, ast_parent(token_ret));
					} else if(lexer_peek(lexer, 0, TSTRING, NULL) == 0) {
						if((err = lexer_eat(lexer, TSTRING, &token_ret)) < 0) {
							char *tmp = "a string, number or function";
							expected = tmp;
							err = -1;
							goto error;
						}
						ast_child(p1, ast_parent(token_ret));
					}
				} else {
					if(lexer_peek(lexer, 0, TACTION, NULL) == 0) {
						char *tmp = "a string, number, or function, got an action";
						expected = tmp;
						lexer->ppos -= strlen(lexer->current_token->value)+1;
						err = -1;
						goto error;
					}
					if((err = lexer_term(lexer, tree_in, 0, &node)) < 0) {
						char *tmp = "a string, number or function";
						expected = tmp;
						goto error;
					}
					ast_child(p1, node);
				}
			}
		}
	} else {
		*tree_out = NULL;
		return -1;
	}

	*tree_out = p;
	return 0;

error:
	return print_error(lexer, p, NULL, NULL, err, expected, pos, lexer->ppos-1);
}

static int lexer_parse_if(struct lexer_t *lexer, struct tree_t *tree, struct tree_t **tree_out) {
	struct tree_t *node = NULL;
	struct token_t *token = lexer->current_token, *token_ret = NULL;
	struct tree_t *p = ast_parent(token);
	char *expected = NULL;
	int pos = 0, err = -1;

	if((err = lexer_eat(lexer, TIF, &token_ret)) < 0) {
		*tree_out = NULL;
		return -1;
	}
	if((err = lexer_term(lexer, tree, 0, &node)) == 0) {
		ast_child(p, node);
	} else {
		char *tmp = "a condition";
		expected = tmp;
		pos = lexer->pos;
		goto error;
	}

	token = lexer->current_token;
	pos = token->pos;

	if((err = lexer_eat(lexer, TTHEN, &token_ret)) < 0) {
		char *tmp = "a condition";
		expected = tmp;
		err = -1;
		goto error;
	}
	pos = token->pos;
	FREE(token->value);
	FREE(token);
	if(lexer_peek(lexer, 0, TACTION, NULL) < 0 && lexer_peek(lexer, 0, TIF, NULL) < 0) {
		if((err = lexer_peek(lexer, 0, TACTION, NULL)) < 0) {
			char *tmp = "an action";
			expected = tmp;
			err = -1;
			goto error;
		}
	}
	if((err = lexer_term(lexer, tree, 0, &node)) == 0) {
		ast_child(p, node);
	} else {
		char *tmp = "an action";
		expected = tmp;
		goto error;
	}
	if(lexer_peek(lexer, 0, TELSE, NULL) == 0) {
		token = lexer->current_token;
		if((err = lexer_eat(lexer, TELSE, &token_ret)) < 0) {
			*tree_out = NULL;
			return -1;
		}
		FREE(token->value);
		FREE(token);
		if(lexer_term(lexer, tree, 0, &node) == 0) {
			ast_child(p, node);
		}
		if(lexer_peek(lexer, 0, TACTION, NULL) < 0 && lexer_peek(lexer, 0, TIF, NULL) < 0 &&
			lexer_peek(lexer, 0, TEND, NULL) < 0 && lexer_peek(lexer, 0, TEOF, NULL) < 0) {
			if((err = lexer_peek(lexer, 0, TACTION, NULL)) < 0) {
				char *tmp = "an action";
				expected = tmp;
				err = -1;
				goto error;
			}
		}
	}
	if(lexer_peek(lexer, 0, TEND, NULL) == 0) {
		token = lexer->current_token;
		if((err = lexer_eat(lexer, TEND, &token_ret)) < 0) {
			*tree_out = NULL;
			return -1;
		}
		FREE(token->value);
		FREE(token);
	} else if(lexer_peek(lexer, 0, TEOF, NULL) == 0) {
		if((err = lexer_eat(lexer, TEOF, &token_ret)) < 0) {
			*tree_out = NULL;
			return -1;
		}
	} else {
		token = lexer->current_token;
		if((err = lexer_eat(lexer, TEND, &token_ret)) < 0) {
			*tree_out = NULL;
			return -1;
		}
		FREE(token->value);
		FREE(token);
	}

	*tree_out = p;
	return 0;

error:
	return print_error(lexer, p, NULL, NULL, err, expected, pos, lexer->ppos-1);
}

/*
 * Always annoying when you reinvented an already existing
 * algorithm without knowing it :)
 *
 * An implementation of precedence climbing
 */
static int lexer_term(struct lexer_t *lexer, struct tree_t *tree_in, int precedence, struct tree_t **tree_out) {
	struct token_t *token = NULL, *token_ret = NULL;
	struct tree_t *node = NULL, *tree_ret = NULL;
	struct tree_t *p = NULL;
	char *expected = NULL;
	int pos = 0, err = -1;

	int match = 0, x = 0;
	struct plua_module_t *modules = plua_get_modules();
	while(modules) {
		if(modules->type == OPERATOR) {
			event_operator_associativity(modules->name, &x);
			if(x > precedence) {
				match = 1;
				break;
			}
		}
		modules = modules->next;
	}

	if(match == 0) {
		if((err = lexer_factor(lexer, tree_in, &tree_ret)) < 0) {
			*tree_out = NULL;
			return err;
		} else {
			*tree_out = tree_ret;
			return 0;
		}
	}

	err = lexer_term(lexer, tree_in, precedence+1, &node);
	if(lexer->current_token != NULL) {
		switch(lexer->current_token->type) {
			case TIF: {
				if((err = lexer_parse_if(lexer, tree_in, &tree_ret)) < 0) {
					*tree_out = NULL;
					char *tmp = "an action";
					expected = tmp;
					pos = lexer->pos;
					goto error;
				}
				*tree_out = tree_ret;
				return 0;
			} break;
			case TACTION: {
				if(node != NULL) {
					if(node->token->value != NULL) {
						FREE(node->token->value);
					}
					if(node->token != NULL) {
						FREE(node->token);
					}
					FREE(node);
					node = NULL;
				}
				if((err = lexer_parse_action(lexer, tree_in, &tree_ret)) < 0) {
					*tree_out = NULL;
					goto error;
				}
				*tree_out = tree_ret;
				return 0;
			} break;
			case TFUNCTION: {
				if((err = lexer_parse_function(lexer, tree_in, &tree_ret)) < 0) {
					*tree_out = NULL;
					char *tmp = "an action";
					expected = tmp;
					pos = lexer->pos;
					goto error;
				}
				if(lexer_peek(lexer, 0, TFUNCTION, NULL) == 0) {
					*tree_out = NULL;
					char *tmp = "an operator";
					expected = tmp;
					pos = lexer->pos-strlen(lexer->current_token->value)-1;
					events_tree_gc(tree_ret);
					err = -1;
					goto error;
				}
				if(node != NULL) {
					ast_child(node, tree_ret);
					*tree_out = node;
				} else {
					*tree_out = tree_ret;
				}
				return 0;
			} break;
			case TOPERATOR: {
				int tpos = lexer->ppos;
				int z = get_precedence(lexer->current_token->value);
				int y = get_associativity(lexer->current_token->value);
				if((z == precedence && y == 1) || z > precedence) {
					if(lexer->current_token->type == TEOF) {
						*tree_out = node;
						goto error;
					}
					token = lexer->current_token;
					if((err = lexer_eat(lexer, TOPERATOR, &token_ret)) < 0) {
						*tree_out = NULL;
						return -1;
					}
					if(z == -1) {
						if((err = lexer_term(lexer, tree_in, precedence, &tree_ret)) < 0) {
							char *tmp = "an operand";
							expected = tmp;
							lexer->ppos = tpos;
							pos = token->pos;
							goto error;
						}
						if(tree_ret->token->type == TACTION) {
							char *tmp = "an operand, got an action";
							expected = tmp;
							lexer->ppos = tpos;
							pos = token->pos;
							err = -1;
							events_tree_gc(tree_ret);
							goto error;
						}
						p = ast_parent(token);
						ast_child(p, node);
						ast_child(p, tree_ret);
						*tree_out = p;
						return 0;
					} else {
						if((err = lexer_term(lexer, tree_in, precedence+1, &tree_ret)) < 0) {
							char *tmp = "an operand";
							expected = tmp;
							pos = token->pos;
							lexer->ppos = tpos;
							goto error;
						}
						if(tree_ret->token->type == TACTION) {
							char *tmp = "an operand, got an action";
							expected = tmp;
							pos = token->pos;
							lexer->ppos = tpos;
							err = -1;
							events_tree_gc(tree_ret);
							goto error;
						}
						p = ast_parent(token);
						ast_child(p, node);
						ast_child(p, tree_ret);
						*tree_out = p;
						return 0;
					}
				}
			} break;
		}
	}

	if((*tree_out = node) == NULL) {
		return err;
	}
	return 0;

error:
	return print_error(lexer, p, node, token, err, expected, pos, lexer->ppos-1);
}

static int lexer_expr(struct lexer_t *lexer, struct tree_t *tree, struct tree_t **out) {
	return lexer_term(lexer, tree, 0, out);
}

static int lexer_parse(struct lexer_t *lexer, struct tree_t **tree_out) {
	struct tree_t *tree = MALLOC(sizeof(struct tree_t));
	int err = -1;

	if(tree == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	memset(tree, 0, sizeof(struct tree_t));

	if((err = lexer_expr(lexer, tree, tree_out)) < 0) {
		if(err == -1) {
			err = print_error(lexer, tree, NULL, NULL, err, NULL, lexer->pos+1, lexer->ppos-1);
		} else {
			events_tree_gc(tree);
		}
		return err;
	} else {
		if(lexer->current_token != NULL && lexer->current_token->type != TEOF) {
			FREE(tree);
			return -1;
		}

		FREE(tree);
	}
	return 0;
}

static void print_ast(struct tree_t *tree) {
	int i = 0;
	if(tree == NULL) {
		return;
	}
	for(i=0;i<tree->nrchildren;i++) {
		if(tree->child[i]->token != NULL) {
			printf("\"%p\" [label=\"%s\"];\n", tree->token->value, tree->token->value);
			printf("\"%p\" [label=\"%s\"];\n", tree->child[i]->token->value, tree->child[i]->token->value);
			printf("\"%p\" -> ", tree->token->value);
			printf("\"%p\";\n", tree->child[i]->token->value);
		}
		print_ast(tree->child[i]);
	}

	if(tree->nrchildren == 0 && tree->token != NULL) {
		printf("\"%p\" [label=\"%s\"];\n", tree->token->value, tree->token->value);
		printf("\"%p\";\n", tree->token->value);
	}
}

static int interpret(struct tree_t *tree, int in_action, struct rules_t *obj, unsigned short validate, struct varcont_t *v);

static void varcont_free(struct varcont_t *v) {
	if(v->free_ == 1) {
		if(v->type_ == JSON_STRING) {
			FREE(v->string_);
		}
		v->free_ = 0;
	}
}

static int run_function(struct tree_t *tree, int in_action, struct rules_t *obj, unsigned short validate, struct varcont_t *v) {
	struct event_function_args_t *args = NULL;
	struct device_t *dev = NULL;
	struct varcont_t v_res, v1;
	int i = 0;

	for(i=0;i<tree->nrchildren;i++) {
		memset(&v_res, '\0', sizeof(struct varcont_t));
		memset(&v1, '\0', sizeof(struct varcont_t));

		if(interpret(tree->child[i], in_action, obj, validate, &v_res) == -1) {
			v = NULL;
			return -1;
		}
		if(v_res.type_ == JSON_STRING) {
			if(event_lookup_variable(v_res.string_, obj, &v1, validate, in_action) == -1) {
				varcont_free(&v1);
				varcont_free(&v_res);
				return -1;
			} else {
				if(v1.type_ == JSON_STRING) {
					if(in_action == 0 && devices_select_struct(ORIGIN_RULE, v1.string_, &dev) == 0) {
						event_cache_device(obj, v1.string_);
					}
				}
				args = event_function_add_argument(&v1, args);
				varcont_free(&v1);
				varcont_free(&v_res);
			}
		} else {
			args = event_function_add_argument(&v_res, args);
		}
	}

	memset(&v_res, '\0', sizeof(struct varcont_t));
	if(event_function_callback(tree->token->value, args, v) == -1) {
		return -1;
	}

	return 0;
}

static int run_action(struct tree_t *tree, struct rules_t *obj, unsigned short validate, struct varcont_t *v_out) {
	struct event_action_args_t *args = NULL;
	struct varcont_t v_res, v_res1, v1, v;
	char *key = NULL;
	int i = 0, x = 0, len = 0;

	len = is_action(tree->token->value, strlen(tree->token->value));

	for(i=0;i<tree->nrchildren;i++) {
		if(tree->child[i]->token->type == TACTION) {
			if(interpret(tree->child[i], 1, obj, validate, &v_res) == -1) {
				v_out = NULL;
				return -1;
			}
			continue;
		}
		memset(&v_res1, '\0', sizeof(struct varcont_t));
		memset(&v_res, '\0', sizeof(struct varcont_t));
		memset(&v1, '\0', sizeof(struct varcont_t));

		if(interpret(tree->child[i], 1, obj, validate, &v_res) == -1) {
			v_out = NULL;
			return -1;
		}
		key = v_res.string_;

		for(x=0;x<tree->child[i]->nrchildren;x++) {
			if(interpret(tree->child[i]->child[x], 1, obj, validate, &v_res1) == -1) {
				v_out = NULL;
				return -1;
			}

			switch(v_res1.type_) {
				case JSON_STRING: {
					if(event_lookup_variable(v_res1.string_, obj, &v1, validate, 1) == -1) {
						varcont_free(&v1);
						varcont_free(&v_res);
						varcont_free(&v_res1);
						v_out = NULL;
						return -1;
					} else {
						switch(v1.type_) {
							case JSON_NUMBER: {
								memset(&v, 0, sizeof(struct varcont_t));
								v.number_ = v1.number_; v.decimals_ = v1.decimals_; v.type_ = JSON_NUMBER;
								args = event_action_add_argument(args, key, &v);
							} break;
							case JSON_STRING: {
								memset(&v, 0, sizeof(struct varcont_t));
								v.string_ = STRDUP(v1.string_); v.type_ = JSON_STRING;
								args = event_action_add_argument(args, key, &v);
								FREE(v.string_);
							} break;
							case JSON_BOOL: {
								memset(&v, 0, sizeof(struct varcont_t));
								v.bool_ = v1.bool_; v.type_ = JSON_BOOL;
								args = event_action_add_argument(args, key, &v);
							} break;
						}
						varcont_free(&v1);
					}
				} break;
				case JSON_NUMBER: {
					memset(&v, 0, sizeof(struct varcont_t));
					v.number_ = v_res1.number_; v.decimals_ = v_res1.decimals_; v.type_ = JSON_NUMBER;
					args = event_action_add_argument(args, key, &v);
				} break;
				case JSON_BOOL: {
					memset(&v, 0, sizeof(struct varcont_t));
					v.bool_ = v_res1.bool_; v.type_ = JSON_BOOL;
					args = event_action_add_argument(args, key, &v);
					FREE(v.string_);
				} break;
			}
			varcont_free(&v_res1);
		}
		varcont_free(&v_res);
	}

	if(len > 0) {
		if(validate == 1) {
			if(event_action_check_arguments(tree->token->value, args) == -1) {
				return -1;
			}
		} else {
			if(event_action_run(tree->token->value, args) == -1) {
				return -1;
			}
		}
	}

	v_out->bool_ = 1;
	v_out->type_ = JSON_BOOL;
	return 0;
}

static int interpret(struct tree_t *tree, int in_action, struct rules_t *obj, unsigned short validate, struct varcont_t *v_out) {
	struct varcont_t v_res;
	memset(&v_res, 0, sizeof(struct varcont_t));
	if(tree == NULL) {
		return -1;
	}

	switch(tree->token->type) {
		case TACTION: {
			if(run_action(tree, obj, validate, v_out) == -1) {
				return -1;
			}
			return 0;
		} break;
		case TFUNCTION: {
			if(run_function(tree, in_action, obj, validate, v_out) == -1) {
				return -1;
			}
			return 0;
		} break;
		case TSTRING: {
			v_out->string_ = tree->token->value;
			v_out->type_ = JSON_STRING;
			return 0;
		} break;
		case TINTEGER: {
			v_out->number_ = atof(tree->token->value);
			v_out->decimals_ = nrDecimals(tree->token->value);
			v_out->type_ = JSON_NUMBER;
			return 0;
		} break;
		case TIF: {
			if(interpret(tree->child[0], 0, obj, validate, &v_res) == -1) {
				varcont_free(&v_res);
				return -1;
			} else {
				if(v_res.type_ != JSON_BOOL) {
					logprintf(LOG_ERR, "If condition did not result in a boolean expression");
					return -1;
				}
				if(validate == 1 || v_res.bool_ == 1) {
					varcont_free(&v_res);
					if(interpret(tree->child[1], 0, obj, validate, &v_res) == -1) {
						varcont_free(&v_res);
						return -1;
					}
					memcpy(v_out, &v_res, sizeof(struct varcont_t));
					varcont_free(&v_res);
					if(validate == 0) {
						return 0;
					}
				}
				if((validate == 1 || v_res.bool_ == 0) && tree->nrchildren == 3) {
					varcont_free(&v_res);
					if(interpret(tree->child[2], 0, obj, validate, &v_res) == -1) {
						varcont_free(&v_res);
						return -1;
					}
					memcpy(v_out, &v_res, sizeof(struct varcont_t));
					varcont_free(&v_res);
					if(validate == 0) {
						return 0;
					}
				}
				if(validate == 1 || (v_res.bool_ == 0 && tree->nrchildren != 3)) {
					v_out->bool_ = 1;
					v_out->type_ = JSON_BOOL;
					varcont_free(&v_res);
					return 0;
				}
			}
			varcont_free(&v_res);
			return -1;
		} break;
		case TOPERATOR: {
			struct varcont_t v1, v2, v3, v4;

			memset(&v1, '\0', sizeof(struct varcont_t));
			memset(&v2, '\0', sizeof(struct varcont_t));
			memset(&v3, '\0', sizeof(struct varcont_t));
			memset(&v4, '\0', sizeof(struct varcont_t));

			if(event_operator_exists(tree->token->value) == 0) {
				if(interpret(tree->child[0], in_action, obj, validate, &v1) == -1) {
					varcont_free(&v1);
					return -1;
				}
				if(interpret(tree->child[1], in_action, obj, validate, &v2) == -1) {
					varcont_free(&v2);
					return -1;
				}
				if(v1.type_ == JSON_STRING) {
					if(event_lookup_variable(v1.string_, obj, &v3, validate, in_action) == -1) {
						varcont_free(&v1);
						return -1;
					} else {
						varcont_free(&v1);
						switch(v3.type_) {
							case JSON_STRING: {
								if((v1.string_ = STRDUP(v3.string_)) == NULL) {
									OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
								}
								v1.free_ = 1;
								v1.type_ = JSON_STRING;
							} break;
							case JSON_NUMBER: {
								v1.number_ = v3.number_;
								v1.decimals_ = v3.decimals_;
								v1.type_ = JSON_NUMBER;
							} break;
							case JSON_BOOL: {
								v1.bool_ = v3.bool_;
								v1.type_ = JSON_BOOL;
							} break;
						}
						varcont_free(&v3);
					}
				}
				if(v2.type_ == JSON_STRING) {
					if(event_lookup_variable(v2.string_, obj, &v4, validate, in_action) == -1) {
						varcont_free(&v2);
						return -1;
					} else {
						varcont_free(&v2);
						switch(v4.type_) {
							case JSON_STRING: {
								if((v2.string_ = STRDUP(v4.string_)) == NULL) {
									OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
								}
								v2.free_ = 1;
								v2.type_ = JSON_STRING;
							} break;
							case JSON_NUMBER: {
								v2.number_ = v4.number_;
								v2.decimals_ = v4.decimals_;
								v2.type_ = JSON_NUMBER;
							} break;
							case JSON_BOOL: {
								v2.bool_ = v4.bool_;
								v2.type_ = JSON_BOOL;
							} break;
						}
						varcont_free(&v4);
					}
				}
				if(event_operator_callback(tree->token->value, &v1, &v2, v_out) != 0) {
					logprintf(LOG_ERR, "rule #%d: an unexpected error occurred while parsing", obj->nr);
					varcont_free(&v1);
					varcont_free(&v2);
					return -1;
				}
				varcont_free(&v1);
				varcont_free(&v2);
				return 0;
			} else {
				return -1;
			}
		} break;
	}

	v_out->bool_ = 0;
	v_out->type_ = JSON_BOOL;
	return 0;
}

int event_parse_rule(char *rule, struct rules_t *obj, int depth, unsigned short validate) {
	struct varcont_t v_res;

	if(validate == 1) {
		struct lexer_t *lexer = MALLOC(sizeof(struct lexer_t));
		if(lexer == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		memset(lexer, 0, sizeof(struct lexer_t));

		if((lexer->text = STRDUP(rule)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		lexer->pos = 0;
		lexer->len = strlen(lexer->text);
		lexer->current_char = &lexer->text[lexer->pos++];

		if(lexer_next_token(lexer) == -1) {
			FREE(lexer->text);
			FREE(lexer);
			return -1;
		}
		if(lexer_parse(lexer, &obj->tree) == -1) {
			FREE(lexer->text);
			FREE(lexer);
			return -1;
		}
		if(pilight.debuglevel >= 1) {
			logprintf(LOG_DEBUG, "%s", lexer->text);
			print_ast(obj->tree);
		}
		FREE(lexer->text);
		FREE(lexer);
	}

	if(interpret(obj->tree, 0, obj, validate, &v_res) == -1) {
		return -1;
	}
	if(v_res.type_ == JSON_BOOL) {
		return v_res.bool_;
	}
	return -1;
}

struct rule_list_t {
	int ptr;
	int nr;
	int size;
	struct rules_t **rules;
} rule_list_t;

static void fib_free(uv_work_t *req, int status) {
	FREE(req);
}

static void events_iterate(uv_work_t *req) {
	running = 1;
	struct rule_list_t *list = req->data;
	struct rules_t *tmp_rules = NULL;
	char *str = NULL;

	tmp_rules = list->rules[list->ptr++];

	if((str = MALLOC(strlen(tmp_rules->rule)+1)) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(str, tmp_rules->rule);
#ifndef WIN32
	clock_gettime(CLOCK_MONOTONIC, &tmp_rules->timestamp.first);
#endif
	if(event_parse_rule(str, tmp_rules, 0, 0) == 0) {
		if(tmp_rules->status == 1) {
			logprintf(LOG_INFO, "executed rule: %s", tmp_rules->name);
		}
	}
#ifndef WIN32
	clock_gettime(CLOCK_MONOTONIC, &tmp_rules->timestamp.second);

	logprintf(LOG_DEBUG, "rule #%d %s was parsed in %.6f seconds", tmp_rules->nr, tmp_rules->name,
		((double)tmp_rules->timestamp.second.tv_sec + 1.0e-9*tmp_rules->timestamp.second.tv_nsec) -
		((double)tmp_rules->timestamp.first.tv_sec + 1.0e-9*tmp_rules->timestamp.first.tv_nsec));
#endif
	FREE(str);

	tmp_rules->status = 0;
	if(list->ptr < list->nr) {
		uv_work_t *tp_work_req = MALLOC(sizeof(uv_work_t));
		if(tp_work_req == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		tp_work_req->data = list;
		if(uv_queue_work(uv_default_loop(), tp_work_req, "rules loop", events_iterate, fib_free) < 0) {
			/*
			 * FIXME
			 */
		}
	} else {
		int i = 0;
		for(i=0;i<list->nr;i++) {
			if(list->rules[i]->jtrigger != NULL) {
				json_delete(list->rules[i]->jtrigger);
				list->rules[i]->jtrigger = NULL;
			}
		}
		FREE(list->rules);
		FREE(list);
	}
	running = 0;
	return;
}

void *events_loop(int reason, void *param, void *userdata) {
	struct rules_t *tmp_rules = NULL;
	struct rule_list_t *list = NULL;
	struct reason_config_update_t *data1 = NULL;
	struct reason_code_received_t *data2 = NULL;
	int i = 0, x = 0, match = 0;

	running = 1;

	switch(reason) {
		case REASON_CODE_RECEIVED: {
			data2 = param;
		} break;
		case REASON_CONFIG_UPDATED: {
			data1 = param;
		} break;
	}

	if(reason == REASON_CONFIG_UPDATED) {
		for(x=0;x<data1->nrdev;x++) {
			/*
			 * Running actions will be aborted when
			 * a new execution id is set.
			 */
			event_action_set_execution_id(data1->devices[x]);
		}
	}


	if(rules_select_struct(ORIGIN_MASTER, NULL, &tmp_rules) == 0) {
		while(tmp_rules) {
			if(tmp_rules->active == 1) {
				if(tmp_rules->nrdevices == 0) {
					match = 1;
				} else {
					if(reason == REASON_CONFIG_UPDATED) {
						/* Only run those events that affect the updates devices */
						for(x=0;x<data1->nrdev;x++) {
							for(i=0;i<tmp_rules->nrdevices;i++) {
								if(data1->devices[x] && strcmp(data1->devices[x], tmp_rules->devices[i]) == 0) {
									match = 1;
									break;
								}
							}
						}
					}
					if(reason == REASON_CODE_RECEIVED) {
						if(strcmp(data2->origin, "sender") == 0 || strcmp(data2->origin, "receiver") == 0) {
							for(i=0;i<tmp_rules->nrdevices;i++) {
								if(strcmp(tmp_rules->devices[i], data2->protocol) == 0) {
									match = 1;
									tmp_rules->jtrigger = json_decode(data2->message);
									break;
								}
							}
						}
					}
				}
				if(match == 1 && tmp_rules->status == 0) {
					if(list == NULL) {
						if((list = MALLOC(sizeof(struct rule_list_t))) == NULL) {
							OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
						}
						list->ptr = 0;
						list->nr = 0;
						list->size = 0;
						list->rules = NULL;
					}
					if(list->size <= list->nr) {
						list->size += 16;
						if((list->rules = REALLOC(list->rules, list->size*sizeof(struct rules_t *))) == NULL) {
							OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
						}
					}

					list->rules[list->nr++] = tmp_rules;
				}
			}
			tmp_rules = tmp_rules->next;
		}
	}

	if(list != NULL && list->nr > 0) {
		uv_work_t *tp_work_req = MALLOC(sizeof(uv_work_t));
		if(tp_work_req == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		tp_work_req->data = list;

		if(uv_queue_work(uv_default_loop(), tp_work_req, "rules loop", events_iterate, fib_free) < 0) {
			/*
			 * FIXME
			 */
			// if(node[i]->done != NULL) {
				// node[i]->done((void *)node[i]->userdata);
			// }
			// FREE(tpdata);
			// FREE(node[i]->ref);
		}
	}

	running = 0;

	return (void *)NULL;
}

void event_init(void) {
	eventpool_callback(REASON_CONFIG_UPDATED, events_loop, NULL);
	eventpool_callback(REASON_CODE_RECEIVED, events_loop, NULL);

	event_operator_init();
	event_action_init();
	event_function_init();
}
