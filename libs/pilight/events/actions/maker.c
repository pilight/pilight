/*
	Copyright (C) 2013 - 2014 CurlyMo

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
#include "../../core/log.h"
#include "../../core/dso.h"
#include "../../core/pilight.h"
#include "../../core/http.h"
#include "../../core/common.h"
#include "maker.h"

const char* MAKER_WEBREQUEST_URL_FRONT = "https://maker.ifttt.com/trigger/";
const char* MAKER_WEBREQUEST_URL_MIDDLE = "/with/key/";

const int MAKER_VALUE_PARAMS_COUNT = 3;
const char* MAKER_VALUE_PARAMS[] = {
  "VALUE1",
  "VALUE2",
  "VALUE3"
};
const char* MAKER_VALUE_URLPARAMS[] = {
  "value1",
  "value2",
  "value3"
};



static int checkArguments(struct rules_actions_t *obj) {
	struct JsonNode *japikey = NULL;
	struct JsonNode *jevent = NULL;
	struct JsonNode *jvalues = NULL;
	struct JsonNode *jchild = NULL;
	int nrvalues = 0;

	japikey = json_find_member(obj->arguments, "APIKEY");
	jevent = json_find_member(obj->arguments, "EVENT");

	if(japikey == NULL) {
		logprintf(LOG_ERR, "maker web requests is missing an \"APIKEY\"");
		return -1;
	}
	if(jevent == NULL) {
		logprintf(LOG_ERR, "maker web action is missing a \"EVENT\"");
		return -1;
	}

	nrvalues = 0;
	if((jvalues = json_find_member(japikey, "value")) != NULL) {
		jchild = json_first_child(jvalues);
		while(jchild) {
			nrvalues++;
			jchild = jchild->next;
		}
	}
	if(nrvalues != 1) {
		logprintf(LOG_ERR, "maker web request action \"APIKEY\" only takes one argument");
		return -1;
	}
	nrvalues = 0;
	if((jvalues = json_find_member(jevent, "value")) != NULL) {
		jchild = json_first_child(jvalues);
		while(jchild) {
			nrvalues++;
			jchild = jchild->next;
		}
	}
	if(nrvalues != 1) {
		logprintf(LOG_ERR, "maker web request action \"EVENT\" only takes one argument");
		return -1;
	}

	//check the n different value parameters
	int i;
	for(i=0;i<MAKER_VALUE_PARAMS_COUNT;++i) {
	  struct JsonNode *jvalueParam = NULL;

	  jvalueParam = json_find_member(obj->arguments, MAKER_VALUE_PARAMS[i]);

	  nrvalues = 0;
	  if (jvalueParam != NULL) {
	    if((jvalues = json_find_member(jvalueParam, "value")) != NULL) {
	      jchild = json_first_child(jvalues);
	      while(jchild) {
	        nrvalues++;
	        jchild = jchild->next;
	      }
	    }

      if(nrvalues != 1) {
        logprintf(LOG_ERR,
                  "maker web request action \"%s\" only takes one argument",
                  MAKER_VALUE_PARAMS[i]);
        return -1;
	    }
  	}
	}

  return 0;
}

	/**
	 * read a parameter
	 */
static char* strcpyParameter(const char* key, struct JsonNode* arguments) {

  struct JsonNode *jvalueParam = json_find_member(arguments, key);
  char* value = NULL;
  char* param = NULL;
  char val[50];

  if (jvalueParam != NULL) {
    struct JsonNode *jvalue = json_find_member(jvalueParam, "value");

    if (jvalue != NULL) {
      struct JsonNode *jval = json_find_element(jvalue, 0);

      if(jval != NULL) {

         switch (jval->tag) {
          case JSON_STRING:
            value = jval->string_;
            break;
          case JSON_NUMBER:
            sprintf(val, "%.4f", jval->number_);
            value = val;
            break;
          default:
            break;
        }
      }
    }
  }

  if (value != NULL)
  {
    if ((param = (char*)MALLOC(strlen(value)+1)))
    {
      strcpy(param, value);
    }
  }

  return param;
}

/**
 * read the three optional value parameters and provide and add them to the URL
 */
static char* addKeyEventAndValueParamsToUrl(char* apikey, char* event,
                                            struct JsonNode* arguments) {

  char* url = NULL;

  int urllen = strlen(MAKER_WEBREQUEST_URL_FRONT) +
               strlen(MAKER_WEBREQUEST_URL_MIDDLE) +
               strlen(apikey) +
               strlen(event);

  if ((url = (char*)MALLOC(urllen+1)) == NULL) {
    logerror("Out of Memory!");
    exit(EXIT_FAILURE);
  }

  strcpy(url, MAKER_WEBREQUEST_URL_FRONT);
  strcat(url, event);
  strcat(url, MAKER_WEBREQUEST_URL_MIDDLE);
  strcat(url, apikey);
  int valcount = 0;

  int i;
  for(i=0;i<MAKER_VALUE_PARAMS_COUNT;++i) {
    char* pvalue = NULL;
    pvalue = strcpyParameter(MAKER_VALUE_PARAMS[i], arguments);
    if (pvalue) {
      urllen += strlen(pvalue) + strlen(MAKER_VALUE_PARAMS[i]) + 2;

      char* newurl;
      if ((newurl = MALLOC(urllen+1))) {

        strcpy(newurl, url);
        strcat(newurl, (valcount++ == 0) ? "?" : "&");
        strcat(strcat(strcat(newurl, MAKER_VALUE_URLPARAMS[i]), "="), pvalue);

        FREE(url);
        url = newurl;
      }
      FREE(pvalue);

    }
  }

  return url;

}


static void *thread(void *param) {
	struct rules_actions_t *pth = (struct rules_actions_t *)param;
	struct JsonNode *arguments = pth->arguments;
	action_maker->nrthreads++;
  logprintf(LOG_INFO, "start maker message");

	int ret = 0;
	int size = 0;
  char typebuf[70];
  char *data;
  char *tp = typebuf;
  char *apikey = NULL;
  char *event = NULL;

  apikey = strcpyParameter("APIKEY", arguments);
  event = strcpyParameter("EVENT",  arguments);

  char* url = addKeyEventAndValueParamsToUrl(apikey, event, arguments);

  data = http_post_content(url, &tp, &ret, &size, "application/x-www-form-urlencoded", "");
  if(ret == 200) {
    logprintf(LOG_INFO, "maker webrequest action succeeded with message: %s", data);
  } else {
    logprintf(LOG_NOTICE, "maker webrequest action failed (%d) with message: %s", ret, data);
  }

  FREE(url);
  FREE(apikey);
  FREE(event);
  if(data != NULL) {
    FREE(data);
  }

	action_maker->nrthreads--;

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
void actionMakerInit(void) {
	event_action_register(&action_maker, "maker");

	options_add(&action_maker->options, 'a', "APIKEY", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_maker->options, 'b', "EVENT", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
  options_add(&action_maker->options, 'c', "VALUE1", OPTION_OPT_VALUE, DEVICES_VALUE, JSON_STRING | JSON_NUMBER, NULL, NULL);
  options_add(&action_maker->options, 'd', "VALUE2", OPTION_OPT_VALUE, DEVICES_VALUE, JSON_STRING | JSON_NUMBER, NULL, NULL);
	options_add(&action_maker->options, 'e', "VALUE3", OPTION_OPT_VALUE, DEVICES_VALUE, JSON_STRING | JSON_NUMBER, NULL, NULL);

	action_maker->run = &run;
	action_maker->checkArguments = &checkArguments;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "maker";
	module->version = "0.9";
	module->reqversion = "7.0";
	module->reqcommit = "87";
}

void init(void) {
	actionMakerInit();
}
#endif
