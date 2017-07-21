/*
	Copyright (C) 2015 CurlyMo & Niek

	This file is part of pilight.

	pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

	pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
	#include <unistd.h>
#endif

#include "../action.h"
#include "../../core/options.h"
#include "../../core/dso.h"
#include "../../core/pilight.h"
#include "../../core/http.h"
#include "../../core/common.h"
#include "../../core/log.h"
#include "../../core/mail.h"
#include "../../storage/storage.h"
#include "sendmail.h"

#ifndef _WIN32
	#include <regex.h>
#endif

//check arguments and settings
static int checkArguments(struct rules_actions_t *obj) {
	struct JsonNode *jsubject = NULL;
	struct JsonNode *jmessage = NULL;
	struct JsonNode *jto = NULL;
	struct JsonNode *jvalues = NULL;
	struct JsonNode *jval = NULL;
	struct JsonNode *jchild = NULL;
	char *stmp = NULL;
	double itmp = 0.0;
	int nrvalues = 0;
#if !defined(__FreeBSD__) && !defined(_WIN32)
	regex_t regex;
	int reti;
#endif

	if(obj == NULL) {
		/* Internal error */
		return -1;
	}

	if(obj->parsedargs == NULL) {
		/* Internal error */
		return -1;
	}

	jsubject = json_find_member(obj->parsedargs, "SUBJECT");
	jmessage = json_find_member(obj->parsedargs, "MESSAGE");
	jto = json_find_member(obj->parsedargs, "TO");

	if(jsubject == NULL) {
		logprintf(LOG_ERR, "sendmail action is missing a \"SUBJECT\"");
		return -1;
	}

	if(jmessage == NULL) {
		logprintf(LOG_ERR, "sendmail action is missing a \"MESSAGE\"");
		return -1;
	}

	if(jto == NULL) {
		logprintf(LOG_ERR, "sendmail action is missing a \"TO\"");
		return -1;
	}

	nrvalues = 0;
	if((jvalues = json_find_member(jsubject, "value")) != NULL) {
		if(jvalues->tag == JSON_ARRAY) {
			jchild = json_first_child(jvalues);
			while(jchild) {
				nrvalues++;
				jchild = jchild->next;
			}
		} else {
			/* Internal error */
			return -1;
		}
	}
	if(nrvalues != 1) {
		logprintf(LOG_ERR, "sendmail action \"SUBJECT\" only takes one argument");
		return -1;
	}

	nrvalues = 0;
	if((jvalues = json_find_member(jmessage, "value")) != NULL) {
		if(jvalues->tag == JSON_ARRAY) {
			jchild = json_first_child(jvalues);
			while(jchild) {
				nrvalues++;
				jchild = jchild->next;
			}
		} else {
			/* Internal error */
			return -1;
		}
	}

	if(nrvalues != 1) {
		logprintf(LOG_ERR, "sendmail action \"MESSAGE\" only takes one argument");
		return -1;
	}

	nrvalues = 0;
	if((jvalues = json_find_member(jto, "value")) != NULL) {
		jchild = json_first_child(jvalues);
		while(jchild) {
			nrvalues++;
			jchild = jchild->next;
		}
	}

	if(nrvalues != 1) {
		logprintf(LOG_ERR, "sendmail action \"TO\" only takes one argument");
		return -1;
	}

	if((jval = json_find_element(jvalues, 0)) != NULL) {
		if(jval->tag != JSON_STRING || jval->string_ == NULL) {
			logprintf(LOG_ERR, "sendmail action \"TO\" must contain an e-mail address");
			return -1;
		} else if(strlen(jval->string_) > 0) {
#if !defined(__FreeBSD__) && !defined(_WIN32)
			char validate[] = "^[a-zA-Z0-9_.]+@[a-zA-Z0-9]+\\.+[a-zA-Z0-9]{2,3}$";
			reti = regcomp(&regex, validate, REG_EXTENDED);
			if(reti) {
				logprintf(LOG_ERR, "could not compile regex for \"TO\"");
				return -1;
			}
			reti = regexec(&regex, jval->string_, 0, NULL, 0);
			if(reti == REG_NOMATCH || reti != 0) {
				logprintf(LOG_ERR, "sendmail action \"TO\" must contain an e-mail address");
				regfree(&regex);
				return -1;
			}
			regfree(&regex);
#endif
		}
	} else {
		/* Internal error */
		return -1;
	}

	// Check if mandatory settings are present in config
	if(settings_select_string(ORIGIN_ACTION, "smtp-host", &stmp) != EXIT_SUCCESS) {
		logprintf(LOG_ERR, "sendmail: setting \"smtp-host\" is missing in config");
		return -1;
	}
	if(settings_select_number(ORIGIN_ACTION, "smtp-port", &itmp) != EXIT_SUCCESS) {
		logprintf(LOG_ERR, "sendmail: setting \"smtp-port\" is missing in config");
		return -1;
	}
	if(settings_select_string(ORIGIN_ACTION, "smtp-user", &stmp) != EXIT_SUCCESS) {
		logprintf(LOG_ERR, "sendmail: setting \"smtp-user\" is missing in config");
		return -1;
	}
	if(settings_select_string(ORIGIN_ACTION, "smtp-password", &stmp) != EXIT_SUCCESS) {
		logprintf(LOG_ERR, "sendmail: setting \"smtp-password\" is missing in config");
		return -1;
	}
	if(settings_select_string(ORIGIN_ACTION, "smtp-sender", &stmp) != EXIT_SUCCESS) {
		logprintf(LOG_ERR, "sendmail: setting \"smtp-sender\" is missing in config");
		return -1;
	}

	return 0;
}

static void callback(int status, struct mail_t *mail) {
	if(status == 0) {
		logprintf(LOG_INFO, "successfully sent sendmail action message");
	} else {
		logprintf(LOG_INFO, "failed to send sendmail action message");
	}
	FREE(mail->from);
	FREE(mail->to);
	FREE(mail->message);
	FREE(mail->subject);
	FREE(mail);
}

static int run(struct rules_actions_t *obj) {
	struct JsonNode *json = obj->parsedargs;

	struct JsonNode *jsubject = NULL;
	struct JsonNode *jmessage = NULL;
	struct JsonNode *jto = NULL;
	struct JsonNode *jvalues1 = NULL;
	struct JsonNode *jvalues2 = NULL;
	struct JsonNode *jvalues3 = NULL;
	struct JsonNode *jval1 = NULL;
	struct JsonNode *jval2 = NULL;
	struct JsonNode *jval3 = NULL;

	struct mail_t *mail = MALLOC(sizeof(struct mail_t));
	char *shost = NULL, *suser = NULL, *spassword = NULL, *stmp = NULL;
	int sport = 0, is_ssl = 0;
	double itmp = 0.0;

	if(mail == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	jmessage = json_find_member(json, "MESSAGE");
	jsubject = json_find_member(json, "SUBJECT");
	jto = json_find_member(json, "TO");

	if(jsubject != NULL && jmessage != NULL && jto != NULL) {
		jvalues1 = json_find_member(jsubject, "value");
		jvalues2 = json_find_member(jmessage, "value");
		jvalues3 = json_find_member(jto, "value");
		if(jvalues1 != NULL && jvalues2 != NULL && jvalues3 != NULL) {
			jval1 = json_find_element(jvalues1, 0);
			jval2 = json_find_element(jvalues2, 0);
			jval3 = json_find_element(jvalues3, 0);
			if(jval1 != NULL && jval2 != NULL && jval3 != NULL &&
				jval1->tag == JSON_STRING && jval2->tag == JSON_STRING && jval3->tag == JSON_STRING) {

				//smtp settings
				if(settings_select_string(ORIGIN_ACTION, "smtp-sender", &stmp) == 0) {
					if((mail->from = MALLOC(strlen(stmp)+1)) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}
					strcpy(mail->from, stmp);
				}
				settings_select_string(ORIGIN_ACTION, "smtp-host", &shost);

				if(settings_select_number(ORIGIN_ACTION, "smtp-port", &itmp) == 0) {
					sport = (int)itmp;
				}

				if(settings_select_number(ORIGIN_ACTION, "smtp-ssl", &itmp) == 0) {
					is_ssl = (int)itmp;
				}

				settings_select_string(ORIGIN_ACTION, "smtp-user", &suser);
				settings_select_string(ORIGIN_ACTION, "smtp-password", &spassword);

				if((mail->subject = MALLOC(strlen(jval1->string_)+1)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				if((mail->message = MALLOC(strlen(jval2->string_)+1)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				if((mail->to = MALLOC(strlen(jval3->string_)+1)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				strcpy(mail->subject, jval1->string_);
				strcpy(mail->message, jval2->string_);
				strcpy(mail->to, jval3->string_);

				if(sendmail(shost, suser, spassword, sport, is_ssl, mail, callback) != 0) {
					logprintf(LOG_ERR, "sendmail action failed to send message \"%s\"", jval2->string_);
					return -1;
				}
			}
		}
	}

	return 0;
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void actionSendmailInit(void) {
	event_action_register(&action_sendmail, "sendmail");

	options_add(&action_sendmail->options, 'a', "SUBJECT", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_sendmail->options, 'b', "MESSAGE", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_sendmail->options, 'c', "TO", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);

	action_sendmail->run = &run;
	action_sendmail->checkArguments = &checkArguments;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "sendmail";
	module->version = "2.1";
	module->reqversion = "7.0";
	module->reqcommit = "94";
}

void init(void) {
	actionSendmailInit();
}
#endif
