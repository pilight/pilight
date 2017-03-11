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

#include "action.h"
#include "options.h"
#include "log.h"
#include "devices.h"
#include "dso.h"
#include "common.h"
#include "config.h"
#include "pilight.h"
#include "mail.h"
#include "sendmail.h"
#include "settings.h"


//check arguments and settings
static int actionSendmailArguments(struct JsonNode *arguments) {
	struct JsonNode *jsubject = NULL;
	struct JsonNode *jmessage = NULL;
	struct JsonNode *jto = NULL;
	struct JsonNode *jvalues = NULL;
	struct JsonNode *jchild = NULL;
	char *stmp = NULL;
	int nrvalues = 0, itmp = 0;
	
	jsubject = json_find_member(arguments, "SUBJECT");
	jmessage = json_find_member(arguments, "MESSAGE");
	jto = json_find_member(arguments, "TO");
	
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
	if((jto = json_find_member(jmessage, "value")) != NULL) {
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

static void *actionSendmailThread(void *param) {
	struct JsonNode *arguments = (struct JsonNode *)param;
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
	char *message = NULL, *to = NULL, *subject = NULL;
	char *sfrom = NULL, *shost = NULL, *suser = NULL, *spassword = NULL;
	int sport= 0;	

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
				subject = jval1->string_;
				message = jval2->string_;
				to = jval3->string_;
				
				//smtp settings
				settings_find_string("smtp-sender", &sfrom);
				settings_find_string("smtp-host", &shost);
				settings_find_number("smtp-port", &sport);
				settings_find_string("smtp-user", &suser);
				settings_find_string("smtp-password", &spassword);
							
				mail.from = MALLOC(strlen(sfrom)+1);
				mail.to = MALLOC(strlen(to)+1);
				mail.subject = MALLOC(strlen(subject)+1);
				mail.message = MALLOC(strlen(message)+1);
			
				strcpy(mail.from, sfrom);
				strcpy(mail.to, to);
				strcpy(mail.subject, subject);
				strcpy(mail.message, message);
				if(sendmail(shost, suser, spassword, sport, &mail) != 0) {
					logprintf(LOG_ERR, "Sendmail failed to send message \"%s\"", message);
				}
				FREE(mail.from);
				FREE(mail.to);
				FREE(mail.subject);
				FREE(mail.message);
			}
		}
	}

action_sendmail->nrthreads--;
	return (void *)NULL;
}

static int actionSendmailRun(struct JsonNode *arguments) {
	pthread_t pth;
	threads_create(&pth, NULL, actionSendmailThread, (void *)arguments);
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

	action_sendmail->run = &actionSendmailRun;
	action_sendmail->checkArguments = &actionSendmailArguments;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "sendmail";
	module->version = "1.0";
	module->reqversion = "6.0";
	module->reqcommit = "51";
}

void init(void) {
	actionSendmailInit();
}
#endif
