/*	
	Copyright 2013 CurlyMo
	
	This file is part of the Raspberry Pi 433.92Mhz transceiver.

    Raspberry Pi 433.92Mhz transceiver is free software: you can redistribute 
	it and/or modify it under the terms of the GNU General Public License as 
	published by the Free Software Foundation, either version 3 of the License, 
	or (at your option) any later version.

    Raspberry Pi 433.92Mhz transceiver is distributed in the hope that it will 
	be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Raspberry Pi 433.92Mhz transceiver. If not, see 
	<http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <libconfig.h>

#include "config.h"
#include "log.h"
#include "options.h"
#include "protocol.h"

int read_config(char *configfile) {
	config_t cfg, *cf = NULL;
	/* Holds the root settings pointer */
	config_setting_t *conf_root = NULL;
	/* Holds the location settings pointer */ 
	config_setting_t *conf_locations = NULL;
	/* Holds the device nodes settings pointer */
	config_setting_t *conf_node = NULL;
	/* Hold the device settings value pointer */
	config_setting_t *conf_value = NULL;

	/* Struct to store the locations */
	struct locations_t *lnode = NULL;
	/* Struct to store the devices per location */
	struct devices_t *dnode = NULL;
	/* Struct to store the device specific settings */
	struct settings_t *snode = NULL;	
	
	/* Location id */
	const char *locid = NULL;
	/* Location readible name */
	const char *locname = NULL;
	/* Device id */
	const char *devid = NULL;
	/* Device user readible name */
	const char *devname = NULL;	
	/* Device specific protocol name */
	const char *protoname = NULL;
	/* Device specific settings name */
	const char *setting = NULL;	
	/* Character pointer for temporary usage */
	const char *stemp = NULL;
	
	/* Character array for temporary usage */
	char temp[255];
	
	/* The number of locations in the config file */
	int nrlocations = 0;
	/* The number of device nodes per location */
	int nrnodes = 0;
	/* The number of settings per device node */
	int nrvalues = 0;
	/* The type of the settings value */
	int conf_type = 0;	
	/* Did we match a protocol */
	int match = 0;
	/* The number of errors in the config file */
	int errors = 0;
	/* Is the specific device setting valid */
	int valid_setting = 0;
	/* Integer pointer for temporary usage */
	int itemp = 0;
	/* For loop integers */
	int a = 0, b = 0, i = 0, c = 0;

	/* Pointer to the match protocol */
	protocol_t *device = malloc(sizeof(protocol_t));

	/* Temporarily options pointer */
	struct options_t *backup_options;	

	/* Initialize and open config file */
	cf = &cfg;
	config_init(cf);
	
	if(!config_read_file(cf, configfile)) {
		logprintf(LOG_ERR, "%s:%d - %s\n", config_error_file(cf), config_error_line(cf), config_error_text(cf));
		config_destroy(cf);
		return (EXIT_FAILURE);
	}
	
	/* Top level setting */
	if((conf_root = config_root_setting(cf)) != NULL) {
		
		nrlocations = config_setting_length(config_setting_get_elem(conf_root, 0));

		/* Second level settings - locations */
		for(a=nrlocations-1;a>=0;a--) {
			conf_locations = config_setting_get_elem(config_setting_get_elem(conf_root, 0), a);
			if(config_setting_lookup_string(conf_locations, "name", &locname) != CONFIG_TRUE) {
				logprintf(LOG_ERR, "location #%d does not have a name", (a+1));
				return (EXIT_FAILURE);
			}
			
			nrnodes = config_setting_length(conf_locations);

			/* Store location in locations struct */
			locid = config_setting_name(conf_locations);
		
			lnode = malloc(sizeof(struct locations_t));
			lnode->id = strdup(locid);
			lnode->name = strdup(locname);
			lnode->next = locations;
						
			/* Third level settings - devices */
			for(b=nrnodes-1;b>=0;b--) {
			
				/* First element is the name field of the location, so start from the second element */
				conf_node = config_setting_get_elem(conf_locations, b);
				if(b > 0) {
					/* Check if the device has a name */
					if(config_setting_lookup_string(conf_node, "name", &devname) != CONFIG_TRUE) {
						logprintf(LOG_ERR, "device #%d of \"%s\", name field missing", b, locname);
						errors++;
					}
					/* Check if the device has a value field */
					if(config_setting_lookup_int(conf_node, "value", &itemp) != CONFIG_TRUE) {
						logprintf(LOG_ERR, "device #%d of \"%s\", value field missing", b, locname);
						errors++;
					}
					/* Check if the device has a protocol defined */
					if(config_setting_lookup_string(conf_node, "protocol", &protoname) != CONFIG_TRUE) {
						logprintf(LOG_ERR, "device #%d of \"%s\", protocol field missing", b, locname);
						errors++;
					} else {

						/* Store devices in their locations */
						devid = config_setting_name(conf_node);

						dnode = malloc(sizeof(struct devices_t));
						dnode->id = strdup(devid);
						dnode->name = strdup(devname);
						dnode->next = devices;

						match=0;
						
						/* Check settings against protocols */
						nrvalues = config_setting_length(conf_node);
						for(i=0;i<protocols.nr; ++i) {
							device = protocols.listeners[i];
							
							/* Check if the protocol exists */
							if(strcmp(device->id,protoname) == 0 && match == 0) {
								match = 1;
								if(device->options != NULL) {
									
									/* Check if all required values are present, and all non-required values are not present */
									backup_options=device->options;
									while(backup_options != NULL && backup_options->name != NULL) {
										valid_setting = 0;
										for(c=0;c<nrvalues;c++) {
											/* Copy all CLI options from the specific protocol */
											conf_value = config_setting_get_elem(conf_node, c);
											setting = config_setting_name(conf_value);
											if(!(strcmp(setting,"protocol") == 0 || strcmp(setting,"value") == 0 || strcmp(setting,"name") == 0)) {
												if(strcmp(setting,backup_options->name) == 0) {
													valid_setting = 1;
													break;
												}
											}
										}
										/* Check if the settings value types are either int or string */
										conf_type = config_setting_type(conf_value);
										if(conf_type != 2 && conf_type != 5 && valid_setting == 1) {
											valid_setting = 0;
											logprintf(LOG_ERR, "in \"%s\", %s is of an incorrect type\n", devname, backup_options->name);
											errors++;
										} else if(valid_setting == 1 && backup_options->conftype == config_required) {
											/* Store settings their device */
											snode = malloc(sizeof(struct settings_t));
											snode->name = strdup(setting);

											/* Store and cast the specific settings values */
											switch(conf_type) {
												case 2:
													config_setting_lookup_int(conf_node,setting,&itemp);
													sprintf(temp,"%d",itemp);
													snode->value = strdup(temp);
													snode->type = CONFIG_TYPE_INT;
												break;
												case 5:
													config_setting_lookup_string(conf_node,setting,&stemp);
													snode->value = strdup(stemp);
													snode->type = CONFIG_TYPE_STRING;
												break;
											}
											snode->next = settings;
											settings = snode;
										/* If settings is not valid but required */
										} else if(valid_setting == 0 && backup_options->conftype == config_required) {
											logprintf(LOG_ERR, "in \"%s\", missing %s field for %s\n", devname, backup_options->name, protoname);
											errors++;
										/* If settings is valid but not required */
										} else if(valid_setting == 1 && backup_options->conftype != config_required) {
											logprintf(LOG_ERR, "in \"%s\", field %s is invalid for %s\n", devname, backup_options->name, protoname);
											errors++;
										}
										backup_options = backup_options->next;
									}
									/* Check if the device contains settings unknown to this protocol */
									for(c=0;c<nrvalues;c++) {
										
										conf_value = config_setting_get_elem(conf_node, c);
										setting = config_setting_name(conf_value);

										if(!(strcmp(setting,"protocol") == 0 || strcmp(setting,"value") == 0 || strcmp(setting,"name") == 0)) {
											valid_setting = 0;
											backup_options=device->options;
											while(backup_options != NULL) {
												if(strcmp(setting,backup_options->name) == 0) {
													valid_setting = 1;
													break;
												}
											backup_options = backup_options->next;
											}
											if(valid_setting == 0) {
												logprintf(LOG_ERR, "in \"%s\", %s is an invalid setting for %s\n", devname, setting, protoname);
												errors++;
											}
										}
									}
								}
								break;
							}
						}
						/* No valid protoocol was given */
						if(match == 0) {
							logprintf(LOG_ERR, "in \"%s\", %s is an invalid protocol\n", devname, protoname);
							errors++;
						} else if(errors == 0) {							
							dnode->protocol = device;
							dnode->settings = malloc(MAX_SETTINGS*sizeof(struct settings_t));
							/* Only store settings if they are present */
							if(settings != NULL) {
								memcpy(dnode->settings,settings,(MAX_SETTINGS*sizeof(struct settings_t)));
							} else {
								dnode->settings = NULL;
							}
							devices = dnode;
							/* Clear the settings struct for the next device */
							settings = NULL;
							if(snode != NULL && snode->next != NULL)
								snode->next = NULL;
						}
					}
				}
			}
			if(errors == 0) {
				lnode->devices = malloc(MAX_DEVICES*sizeof(struct devices_t));
				/* Only store devices if they are present */
				if(devices != NULL) {
					memcpy(lnode->devices,devices,(MAX_DEVICES*sizeof(struct devices_t)));
				} else {
					lnode->devices = NULL;
				}
				locations = lnode;
				/* Clear the locations struct for the next location */
				devices = NULL;
				if(dnode != NULL && dnode->next != NULL)
					dnode->next = NULL;
			}
		}
	}
	config_destroy(cf);	

	if(errors == 0)
		return (EXIT_SUCCESS);
	else
		return (EXIT_FAILURE);
}