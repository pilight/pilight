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
#include <unistd.h>

#include "../../core/threads.h"
#include "../action.h"
#include "../../core/options.h"
#include "../../config/devices.h"
#include "../../core/dso.h"
#include "../../core/pilight.h"
#include "../../core/http.h"
#include "../../core/common.h"
#include "../../config/settings.h"
#include "../../core/log.h"
#include "../../core/mail.h"
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
	int nrvalues = 0, itmp = 0;
#if !defined(__FreeBSD__) && !defined(_WIN32)
	regex_t regex;
	int reti;
#endif
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
		jchild = json_first_child(jvalues);
		while(jchild) {
			nrvalues++;
			jchild = jchild->next;
		}
	}
	if(nrvalues > 1) {
		logprintf(LOG_ERR, "sendmail action \"SUBJECT\" only takes one argument");
		return -1;
	}
	nrvalues = 0;
	if((jvalues = json_find_member(jmessage, "value")) != NULL) {
		jchild = json_first_child(jvalues);
		while(jchild) {
			nrvalues++;
			jchild = jchild->next;
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
	if(nrvalues > 1) {
		logprintf(LOG_ERR, "sendmail action \"TO\" only takes one argument");
		return -1;
	}
	jval = json_find_element(jvalues, 0);
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
	// Check if mandatory settings are present in config
	if(settings_find_string("smtp-host", &stmp) != EXIT_SUCCESS) {
		logprintf(LOG_ERR, "Sendmail: setting \"smtp-host\" is missing in config");
		return -1;
	}
	if(settings_find_number("smtp-port", &itmp) != EXIT_SUCCESS) {
		logprintf(LOG_ERR, "Sendmail: setting \"smtp-port\" is missing in config");
		return -1;
	}
	if(settings_find_string("smtp-user", &stmp) != EXIT_SUCCESS) {
		logprintf(LOG_ERR, "Sendmail: setting \"smtp-user\" is missing in config");
		return -1;
	}
	if(settings_find_string("smtp-password", &stmp) != EXIT_SUCCESS) {
		logprintf(LOG_ERR, "Sendmail: setting \"smtp-password\" is missing in config");
		return -1;
	}
	if(settings_find_string("smtp-sender", &stmp) != EXIT_SUCCESS) {
		logprintf(LOG_ERR, "Sendmail: setting \"smtp-sender\" is missing in config");
		return -1;
	}
	return 0;
}

static void *thread(void *param) {
	struct rules_actions_t *pth = (struct rules_actions_t *)param;
	struct JsonNode *arguments = pth->parsedargs;
	struct JsonNode *jsubject = NULL;
	struct JsonNode *jmessage = NULL;
	struct JsonNode *jto = NULL;
	struct JsonNode *jvalues1 = NULL;
	struct JsonNode *jvalues2 = NULL;
	struct JsonNode *jvalues3 = NULL;
	struct JsonNode *jval1 = NULL;
	struct JsonNode *jval2 = NULL;
	struct JsonNode *jval3 = NULL;

	action_sendmail->nrthreads++;

	struct mail_t mail;
	char *shost = NULL, *suser = NULL, *spassword = NULL;
	int sport = 0;

	jmessage = json_find_member(arguments, "MESSAGE");
	jsubject = json_find_member(arguments, "SUBJECT");
	jto = json_find_member(arguments, "TO");

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
				settings_find_string("smtp-sender", &mail.from);
				settings_find_string("smtp-host", &shost);
				settings_find_number("smtp-port", &sport);
				settings_find_string("smtp-user", &suser);
				settings_find_string("smtp-password", &spassword);

				mail.subject = jval1->string_;
				mail.message = jval2->string_;
				mail.to = jval3->string_;

				if(sendmail(shost, suser, spassword, sport, &mail) != 0) {
					logprintf(LOG_ERR, "Sendmail failed to send message \"%s\"", jval2->string_);
				}
			}
		}
	}

	action_sendmail->nrthreads--;

	return (void *)NULL;
}

static int run(struct rules_actions_t *obj) {
	pthread_t pth;
	threads_create(&pth, NULL, thread, (void *)obj);
	pthread_detach(pth);
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
	module->version = "1.1";
	module->reqversion = "6.0";
	module->reqcommit = "152";
}

void init(void) {
	actionSendmailInit();
}
#endif
