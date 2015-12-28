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
	for(int i=0;i<MAKER_VALUE_PARAMS_COUNT;++i) {
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
static char* getParameter(char* key, arguments*) {

  struct JsonNode *jvalueParam = json_find_member(arguments, key);
  char* value = NULL;

  if (jvalueParam != NULL) {
    struct JsonNode jvalue = json_find_member(japikey, "value");

    if (jvalue != NULL) {
      struct JsonNode jval = json_find_element(jvalue, 0);

      if(jvalue != NULL) {
         switch (jvalue->tag) {
          case JSON_STRING:
            value = jvalue->string_;
            break;
          case JSON_NUMBER:
            char val[50];
            sprintf(val, "%.4f", jvalue->number_);
            value = val;
          default:
            break;
        }
      }
    }
  }

  return (value == NULL) ? "" : value;
}

/**
 * read the three optional value parameters and provide and add them to the URL
 */
static char* addKeyEventAndValueParamsToUrl(char* apikey, char* event,
                                            arguments*) {

  int urllen = strlen(MAKER_WEBREQUEST_URL_FRONT) +
               strlen(MAKER_WEBREQUEST_URL_MIDDLE) +
               strlen(apikey) +
               strlen(event);

  char* url = (char*)malloc(urllen*sizeof(char));
  strcat(url, MAKER_WEBREQUEST_URL_FRONT);
  strcat(url, event);
  strcat(url, MAKER_WEBREQUEST_URL_MIDDLE);
  strcat(url, apikey);
  int valcount = 0;

  for(int i=0;i<MAKER_VALUE_PARAMS_COUNT;++i) {
    char* pvalue = getParameter(MAKER_VALUE_PARAMS[i], arguments);
    if (pvalue != "") {
      int urlen += strlen(pvalue) + strlen(MAKER_VALUE_PARAMS[i]) + 2;
      url = (char*)realloc(url, urllen*sizeof(char));

      strcat(url, (valcount++ == 0) ? "?" : "&");
      strcat(strcat(strcat(url, MAKER_VALUE_PARAMS[i]), "="), pvalue);
    }
  }

  return url;

}


static void *thread(void *param) {
	struct rules_actions_t *pth = (struct rules_actions_t *)param;
	struct JsonNode *arguments = pth->arguments;
	action_maker->nrthreads++;

	int ret = 0;
	int size = 0;
  char typebuf[70];
  char *data;
  char *tp = typebuf;

  char* url = addKeyEventAndValueParamsToUrl(
    getParameter("APIKEY", arguments),
    getParameter("EVENT",  arguments),
    arguments);

  data = http_post_content(url, &tp, &ret, &size, "application/x-www-form-urlencoded", content);
  if(ret == 200) {
    logprintf(LOG_DEBUG, "maker webrequest action succeeded with message: %s", data);
  } else {
    logprintf(LOG_NOTICE, "maker webrequest action failed (%d) with message: %s", ret, data);
  }

  FREE(url);
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
	event_action_register(&action_pushover, "maker");

	options_add(&action_pushover->options, 'a', "APIKEY", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_pushover->options, 'b', "EVENT", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_pushover->options, 'c', "VALUE1", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&action_pushover->options, 'd', "VALUE2", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
  options_add(&action_pushover->options, 'e', "VALUE3", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);

	action_pushover->run = &run;
	action_pushover->checkArguments = &checkArguments;
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
