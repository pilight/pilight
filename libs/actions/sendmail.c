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
#include <fcntl.h>

#include "action.h"
#include "log.h"
#include "dso.h"
#include "json.h"
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
	struct JsonNode *temp = NULL;
	char *stmp = NULL;
	int nrvalues = 0, itmp = 0;
	
	jsubject = json_find_member(arguments, "SUBJECT");
	jmessage = json_find_member(arguments, "MESSAGE");
	jto = json_find_member(arguments, "TO");
	
	if(jmessage == NULL) {
		logprintf(LOG_ERR, "sendmail action is missing a \"MESSAGE\"");
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
	if(settings_find_string("mail-to", &stmp) != EXIT_SUCCESS) {
		logprintf(LOG_ERR, "Sendmail: setting \"mail-to\" is missing in config");
		return -1;
	} 
	if(settings_find_string("mail-from", &stmp) != EXIT_SUCCESS) {
		logprintf(LOG_ERR, "Sendmail: setting \"mail-from\" is missing in config");
		return -1;
	} 
	if(settings_find_string("mail-subject", &stmp) != EXIT_SUCCESS) {
		logprintf(LOG_ERR, "Sendmail: setting \"mail-subject\" is missing in config");
		return -1;
	} 
	return 0;
}

static int actionSendmailRun(struct JsonNode *arguments) {
	struct JsonNode *jsubject = NULL;
	struct JsonNode *jmessage = NULL;
	struct JsonNode *jto = NULL;
	struct JsonNode *jvalues1 = NULL;
	struct JsonNode *jvalues2 = NULL;
	struct JsonNode *jvalues3 = NULL;
	struct JsonNode *jval1 = NULL;
	struct JsonNode *jval2 = NULL;
	struct JsonNode *jval3 = NULL;
	struct mail_t mail;
	char *message = NULL, *to = NULL, *subject = NULL;
	char *sfrom = NULL, *sto = NULL, *ssubject = NULL;
	char *shost = NULL, *suser = NULL, *spassword = NULL;
	int sport= 0;	

	// mail settings
	settings_find_string("mail-from", &sfrom);
	settings_find_string("mail-to", &sto);
	settings_find_string("mail-subject", &ssubject);

	//smtp settings
	settings_find_string("smtp-host", &shost);
	settings_find_number("smtp-port", &sport);
	settings_find_string("smtp-user", &suser);
	settings_find_string("smtp-password", &spassword);

	// arguments
	jsubject = json_find_member(arguments, "SUBJECT");
	jmessage = json_find_member(arguments, "MESSAGE");
	jto = json_find_member(arguments, "TO");
	
	if(jmessage != NULL) {	
		if((jvalues1 = json_find_member(jmessage, "value")) != NULL) {
			if((jval1 = json_find_element(jvalues1, 0)) != NULL && jval1->tag == JSON_STRING){
				message = jval1->string_;
			}
			subject = ssubject;
			if(jsubject != NULL) {	
				if((jvalues2 = json_find_member(jsubject, "value")) != NULL) {
					if((jval2 = json_find_element(jvalues2, 0)) != NULL && jval2->tag == JSON_STRING){
						subject = jval2->string_;
					} 
				}
			}
			to = sto;
			if(jto != NULL) {	
				if((jvalues3 = json_find_member(jto, "value")) != NULL) {
					if((jval3 = json_find_element(jvalues3, 0)) != NULL && jval3->tag == JSON_STRING){
						to = jval3->string_;
					}
				}
			}
			
			mail.from = MALLOC(strlen(sfrom)+1);
			mail.to = MALLOC(strlen(to)+1);
			mail.subject = MALLOC(strlen(subject)+1);
			mail.message = MALLOC(strlen(message)+1);

			strcpy(mail.from, sfrom);
			strcpy(mail.to, to);
			strcpy(mail.subject, subject);
			strcpy(mail.message, message);
			
			if(sendmail(shost, suser, spassword, sport, &mail) != 0) {
				logprintf(LOG_DEBUG, "Sendmail failed with message \"%s\"", message);
			}
			
			FREE(mail.from);
			FREE(mail.to);
			FREE(mail.subject);
			FREE(mail.message);
		}
	}
	return 0;
}

#ifndef MODULE
__attribute__((weak))
#endif
void actionSendmailInit(void) {
	event_action_register(&action_sendmail, "sendmail");

	options_add(&action_sendmail->options, 'a', "SUBJECT", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);
	options_add(&action_sendmail->options, 'b', "MESSAGE", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_sendmail->options, 'c', "TO", OPTION_HAS_VALUE, DEVICES_SETTING, JSON_STRING, NULL, NULL);

	action_sendmail->run = &actionSendmailRun;
	action_sendmail->checkArguments = &actionSendmailArguments;
}

#ifdef MODULE
void compatibility(struct module_t *module) {
	module->name = "sendmail";
	module->version = "1.0";
	module->reqversion = "6.0";
	module->reqcommit = NULL;
}

void init(void) {
	actionSendmailInit();
}
#endif