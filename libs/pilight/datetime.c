/*
	Copyright (C) 2013 CurlyMo

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

#include <stdlib.h>
#include <stdio.h>
#define __USE_XOPEN
#include <sys/time.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

#include "json.h"
#include "common.h"
#include "log.h"

#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))

double ***tzcoords;
unsigned int tznrpolys[410];
int tzdatafilled = 0;

char tznames[410][30] = {
    "unknown",
    "America/Dominica",
    "America/St_Vincent",
    "Australia/Lord_Howe",
    "Asia/Kashgar",
    "Pacific/Wallis",
    "Europe/Berlin",
    "America/Manaus",
    "Asia/Jerusalem",
    "America/Phoenix",
    "Australia/Darwin",
    "Asia/Seoul",
    "Africa/Gaborone",
    "Indian/Chagos",
    "America/Argentina/Mendoza",
    "Asia/Hong_Kong",
    "America/Godthab",
    "Africa/Dar_es_Salaam",
    "Pacific/Majuro",
    "America/Port-au-Prince",
    "America/Montreal",
    "Atlantic/Reykjavik",
    "America/Panama",
    "America/Sitka",
    "Asia/Ho_Chi_Minh",
    "America/Danmarkshavn",
    "Asia/Jakarta",
    "America/Boise",
    "Asia/Baghdad",
    "Africa/El_Aaiun",
    "Europe/Zagreb",
    "America/Santiago",
    "America/Merida",
    "Africa/Nouakchott",
    "America/Bahia_Banderas",
    "Australia/Perth",
    "Asia/Sakhalin",
    "Asia/Vladivostok",
    "Africa/Bissau",
    "America/Los_Angeles",
    "Asia/Rangoon",
    "America/Belize",
    "Asia/Harbin",
    "Australia/Currie",
    "Pacific/Pago_Pago",
    "America/Vancouver",
    "Asia/Magadan",
    "Asia/Tbilisi",
    "Asia/Yerevan",
    "Europe/Tallinn",
    "Pacific/Johnston",
    "Asia/Baku",
    "America/North_Dakota/New_Salem",
    "Europe/Vilnius",
    "America/Indiana/Petersburg",
    "Asia/Tehran",
    "America/Inuvik",
    "Europe/Lisbon",
    "Europe/Vatican",
    "Pacific/Chatham",
    "Antarctica/Macquarie",
    "America/Araguaina",
    "Asia/Thimphu",
    "Atlantic/Madeira",
    "America/Coral_Harbour",
    "Pacific/Funafuti",
    "Indian/Mahe",
    "Australia/Adelaide",
    "Africa/Freetown",
    "Atlantic/South_Georgia",
    "Africa/Accra",
    "America/North_Dakota/Beulah",
    "America/Jamaica",
    "America/Scoresbysund",
    "America/Swift_Current",
    "Europe/Tirane",
    "Asia/Ashgabat",
    "America/Moncton",
    "Europe/Vaduz",
    "Australia/Eucla",
    "America/Montserrat",
    "America/Glace_Bay",
    "Atlantic/Stanley",
    "Africa/Bujumbura",
    "Africa/Porto-Novo",
    "America/Argentina/Rio_Gallegos",
    "America/Grenada",
    "Asia/Novokuznetsk",
    "America/Argentina/Catamarca",
    "America/Indiana/Indianapolis",
    "America/Indiana/Tell_City",
    "America/Curacao",
    "America/Miquelon",
    "America/Detroit",
    "America/Menominee",
    "Asia/Novosibirsk",
    "Africa/Lagos",
    "Indian/Cocos",
    "America/Yakutat",
    "Europe/Volgograd",
    "Asia/Qatar",
    "Indian/Antananarivo",
    "Pacific/Marquesas",
    "America/Grand_Turk",
    "Asia/Khandyga",
    "America/North_Dakota/Center",
    "Pacific/Guam",
    "Pacific/Pitcairn",
    "America/Cambridge_Bay",
    "Asia/Bahrain",
    "America/Kentucky/Monticello",
    "Arctic/Longyearbyen",
    "Africa/Cairo",
    "Australia/Hobart",
    "Pacific/Galapagos",
    "Asia/Oral",
    "America/Dawson_Creek",
    "Africa/Mbabane",
    "America/Halifax",
    "Pacific/Tongatapu",
    "Asia/Aqtau",
    "Asia/Hovd",
    "uninhabited",
    "Africa/Nairobi",
    "Asia/Ulaanbaatar",
    "Indian/Christmas",
    "Asia/Taipei",
    "Australia/Melbourne",
    "America/Argentina/Salta",
    "Australia/Broken_Hill",
    "America/Argentina/Tucuman",
    "America/Kentucky/Louisville",
    "Asia/Jayapura",
    "Asia/Macau",
    "America/Ojinaga",
    "America/Nome",
    "Pacific/Wake",
    "Europe/Andorra",
    "America/Iqaluit",
    "America/Kralendijk",
    "Europe/Jersey",
    "Asia/Ust-Nera",
    "Asia/Yakutsk",
    "America/Yellowknife",
    "America/Fortaleza",
    "Asia/Irkutsk",
    "America/Tegucigalpa",
    "Europe/Zaporozhye",
    "Pacific/Fiji",
    "Pacific/Tarawa",
    "Africa/Asmara",
    "Asia/Dhaka",
    "Asia/Pyongyang",
    "Europe/Athens",
    "America/Resolute",
    "Africa/Brazzaville",
    "Africa/Libreville",
    "Atlantic/St_Helena",
    "Europe/Samara",
    "America/Adak",
    "America/Argentina/Jujuy",
    "America/Chicago",
    "Africa/Sao_Tome",
    "Europe/Bratislava",
    "Asia/Riyadh",
    "America/Lima",
    "America/New_York",
    "America/Pangnirtung",
    "Asia/Samarkand",
    "America/Port_of_Spain",
    "Africa/Johannesburg",
    "Pacific/Port_Moresby",
    "America/Bahia",
    "Europe/Zurich",
    "America/St_Barthelemy",
    "Asia/Nicosia",
    "Europe/Kaliningrad",
    "America/Anguilla",
    "Europe/Ljubljana",
    "Asia/Yekaterinburg",
    "Africa/Kampala",
    "America/Rio_Branco",
    "Africa/Bamako",
    "America/Goose_Bay",
    "Europe/Moscow",
    "Africa/Conakry",
    "America/Chihuahua",
    "Europe/Warsaw",
    "Pacific/Palau",
    "Europe/Mariehamn",
    "Africa/Windhoek",
    "America/La_Paz",
    "America/Recife",
    "America/Mexico_City",
    "Asia/Amman",
    "America/Tijuana",
    "America/Metlakatla",
    "Pacific/Midway",
    "Europe/Simferopol",
    "Europe/Budapest",
    "Pacific/Apia",
    "America/Paramaribo",
    "Africa/Malabo",
    "Africa/Ndjamena",
    "Asia/Choibalsan",
    "America/Antigua",
    "Europe/Istanbul",
    "Africa/Blantyre",
    "Australia/Sydney",
    "Asia/Dushanbe",
    "Europe/Belgrade",
    "Asia/Karachi",
    "Europe/Luxembourg",
    "Europe/Podgorica",
    "Australia/Lindeman",
    "Africa/Bangui",
    "Asia/Aden",
    "Pacific/Chuuk",
    "Asia/Brunei",
    "Indian/Comoro",
    "America/Asuncion",
    "Europe/Prague",
    "America/Cayman",
    "Pacific/Pohnpei",
    "America/Atikokan",
    "Pacific/Norfolk",
    "Africa/Dakar",
    "America/Argentina/Buenos_Aires",
    "America/Edmonton",
    "America/Barbados",
    "America/Santo_Domingo",
    "Asia/Bishkek",
    "Asia/Kuwait",
    "Pacific/Efate",
    "Indian/Mauritius",
    "America/Aruba",
    "Australia/Brisbane",
    "Indian/Kerguelen",
    "Pacific/Kiritimati",
    "America/Toronto",
    "Asia/Qyzylorda",
    "Asia/Aqtobe",
    "America/Eirunepe",
    "Europe/Isle_of_Man",
    "America/Blanc-Sablon",
    "Pacific/Honolulu",
    "America/Montevideo",
    "Asia/Tashkent",
    "Pacific/Kosrae",
    "America/Indiana/Winamac",
    "America/Argentina/La_Rioja",
    "Africa/Mogadishu",
    "Asia/Phnom_Penh",
    "Africa/Banjul",
    "America/Creston",
    "Europe/Brussels",
    "Asia/Gaza",
    "Atlantic/Bermuda",
    "America/Indiana/Knox",
    "America/El_Salvador",
    "America/Managua",
    "Africa/Niamey",
    "Europe/Monaco",
    "Africa/Ouagadougou",
    "Pacific/Easter",
    "Atlantic/Canary",
    "Asia/Vientiane",
    "Europe/Bucharest",
    "Africa/Lusaka",
    "Asia/Kathmandu",
    "Africa/Harare",
    "Asia/Bangkok",
    "Europe/Rome",
    "Africa/Lome",
    "America/Denver",
    "Indian/Reunion",
    "Europe/Kiev",
    "Europe/Vienna",
    "America/Guadeloupe",
    "America/Argentina/Cordoba",
    "Asia/Manila",
    "Asia/Tokyo",
    "America/Nassau",
    "Pacific/Enderbury",
    "Atlantic/Azores",
    "America/Winnipeg",
    "Europe/Dublin",
    "Asia/Kuching",
    "America/Argentina/Ushuaia",
    "Asia/Colombo",
    "Asia/Krasnoyarsk",
    "America/St_Johns",
    "Asia/Shanghai",
    "Pacific/Kwajalein",
    "Africa/Kigali",
    "Europe/Chisinau",
    "America/Noronha",
    "Europe/Guernsey",
    "Europe/Paris",
    "America/Guyana",
    "Africa/Luanda",
    "Africa/Abidjan",
    "America/Tortola",
    "Europe/Malta",
    "Europe/London",
    "Pacific/Guadalcanal",
    "Pacific/Gambier",
    "America/Thule",
    "America/Rankin_Inlet",
    "America/Regina",
    "America/Indiana/Vincennes",
    "America/Santarem",
    "Africa/Djibouti",
    "Pacific/Tahiti",
    "Europe/San_Marino",
    "America/Argentina/San_Luis",
    "Africa/Ceuta",
    "Asia/Singapore",
    "America/Campo_Grande",
    "Africa/Tunis",
    "Europe/Copenhagen",
    "Asia/Pontianak",
    "Asia/Dubai",
    "Africa/Khartoum",
    "Europe/Helsinki",
    "America/Whitehorse",
    "America/Maceio",
    "Africa/Douala",
    "Asia/Kuala_Lumpur",
    "America/Martinique",
    "America/Sao_Paulo",
    "America/Dawson",
    "Africa/Kinshasa",
    "Europe/Riga",
    "Africa/Tripoli",
    "Europe/Madrid",
    "America/Nipigon",
    "Pacific/Fakaofo",
    "Europe/Skopje",
    "America/St_Thomas",
    "Africa/Maseru",
    "Europe/Sofia",
    "America/Porto_Velho",
    "America/St_Kitts",
    "Africa/Casablanca",
    "Asia/Hebron",
    "Asia/Dili",
    "America/Argentina/San_Juan",
    "Asia/Almaty",
    "Europe/Sarajevo",
    "America/Boa_Vista",
    "Africa/Addis_Ababa",
    "Indian/Mayotte",
    "Africa/Lubumbashi",
    "Atlantic/Cape_Verde",
    "America/Lower_Princes",
    "Europe/Oslo",
    "Africa/Monrovia",
    "Asia/Muscat",
    "America/Thunder_Bay",
    "America/Juneau",
    "Pacific/Rarotonga",
    "Atlantic/Faroe",
    "America/Cayenne",
    "America/Cuiaba",
    "Africa/Maputo",
    "Asia/Anadyr",
    "Asia/Kabul",
    "America/Santa_Isabel",
    "Asia/Damascus",
    "Pacific/Noumea",
    "America/Anchorage",
    "Asia/Kolkata",
    "Pacific/Niue",
    "Asia/Kamchatka",
    "America/Matamoros",
    "Europe/Stockholm",
    "America/Havana",
    "Pacific/Auckland",
    "America/Rainy_River",
    "Asia/Omsk",
    "Africa/Algiers",
    "America/Guayaquil",
    "Indian/Maldives",
    "Asia/Makassar",
    "America/Monterrey",
    "Europe/Amsterdam",
    "America/St_Lucia",
    "Europe/Uzhgorod",
    "America/Indiana/Marengo",
    "Pacific/Saipan",
    "America/Bogota",
    "America/Indiana/Vevay",
    "America/Guatemala",
    "America/Puerto_Rico",
    "America/Marigot",
    "Africa/Juba",
    "America/Costa_Rica",
    "America/Caracas",
    "Pacific/Nauru",
    "Europe/Minsk",
    "America/Belem",
    "America/Cancun",
    "America/Hermosillo",
    "Asia/Chongqing",
    "Asia/Beirut",
    "Europe/Gibraltar",
    "Asia/Urumqi",
    "America/Mazatlan"
};

int fillTZData(void) {
	if(tzdatafilled == 1) {
		return EXIT_SUCCESS;
	}
	char *content = NULL;
	JsonNode *root = NULL;
	FILE *fp;
	size_t bytes;
	struct stat st;

	char tzdatafile[] = "/etc/pilight/tzdata.json";
	/* Read JSON config file */
	if(!(fp = fopen(tzdatafile, "rb"))) {
		printf("cannot read config file: %s", tzdatafile);
		return EXIT_FAILURE;
	}

	fstat(fileno(fp), &st);
	bytes = (size_t)st.st_size;

	if(!(content = calloc(bytes+1, sizeof(char)))) {
		logprintf(LOG_ERR, "out of memory");
		return EXIT_FAILURE;
	}

	if(fread(content, sizeof(char), bytes, fp) == -1) {
		logprintf(LOG_ERR, "cannot read tzdata file: %s", tzdatafile);
	}
	fclose(fp);

	/* Validate JSON and turn into JSON object */
	if(json_validate(content) == false) {
		logprintf(LOG_ERR, "tzdata is not in a valid json format");
		sfree((void *)&content);
		return EXIT_FAILURE;
	}
	root = json_decode(content);

	JsonNode *alist = json_first_child(root);
	unsigned int i = 0, x = 0, y = 0;
	while(alist) {
		JsonNode *country = json_first_child(alist);
		while(country) {
			if(!(tzcoords = realloc(tzcoords, sizeof(double **)*(i+1)))) {
				logprintf(LOG_ERR, "out of memory");
			}
			tzcoords[i] = NULL;
			JsonNode *coords = json_first_child(country);
			x = 0;
			while(coords) {
				y = 0;
				if(!(tzcoords[i] = realloc(tzcoords[i], sizeof(double *)*(x+1)))) {
					logprintf(LOG_ERR, "out of memory");
				}
				tzcoords[i][x] = NULL;
				if(!(tzcoords[i][x] = malloc(sizeof(double)*2))) {
					logprintf(LOG_ERR, "out of memory");
				}
				JsonNode *lonlat = json_first_child(coords);
				while(lonlat) {
					tzcoords[i][x][y] = lonlat->number_;
					y++;
					lonlat = lonlat->next;
				}
				x++;
				coords = coords->next;
			}
			tznrpolys[i] = x;
			i++;
			country = country->next;
		}
		alist = alist->next;
	}
	json_delete(root);
	sfree((void *)&content);
	tzdatafilled = 1;
	return EXIT_SUCCESS;
}

int datetime_gc(void) {
	int i = 0, a = 0;
	if(tzdatafilled) {
		for(i=0;i<410;i++) {
			unsigned int n = tznrpolys[i];
			for(a=0;a<n;a++) {
				free(tzcoords[i][a]);
			}
			free(tzcoords[i]);
		}
		free(tzcoords);
		logprintf(LOG_DEBUG, "garbage collected datetime library");
	}
	return EXIT_SUCCESS;
}

char *coord2tz(double longitude, double latitude) {
	if(fillTZData() == EXIT_FAILURE) {
		return NULL;
	}

	double x = latitude;
	double y = longitude;

	int i = 0, a = 0, margin = 1, inside = 0;
	char *timezone = NULL;

	while(!inside) {
		for(i=0;i<410;i++) {
			unsigned int n = tznrpolys[i];
			if(n > 0) {
				double p1x = 0;
				double p1y = 0;
				for(a=0;a<n;a++) {
					if(tzcoords[i][a][0] < p1x || ((int)p1x == 0)) {
						p1x = tzcoords[i][a][0];
					}
					if(tzcoords[i][a][1] < p1y && ((int)p1y == 0)) {
						p1y = tzcoords[i][a][1];
					}
				} 
				for(a=0;a<n+1;a++) {
					double p2x = tzcoords[i][a % (int)n][0];
					double p2y = tzcoords[i][a % (int)n][1];
					if((round(p2x)-margin < round(x) && round(p2x)+margin > round(x))
					   &&(round(p2y)-margin < round(y) && round(p2y)+margin > round(y))) {				   
						double xinters = 0;
						if(y > min(p1y, p2y)) {
							if(y <= max(p1y, p2y)) {
								if(x <= max(p1x, p2x)) {
									if(fabs(p1y-p2y) >= 0.00001) {
										xinters = (y-p1y)*(p2x-p1x)/(p2y-p1y)+p1x;
									}
									if(fabs(p1x-p2x) < 0.00001 || x <= xinters) {
										timezone = tznames[i];
										inside = 1;
										break;
									}
								}
							}
						}
						p1x = p2x;
						p1y = p2y;
					}
				}
				if(inside == 1) {
					break;
				}
			}
		}
		margin++;
	}
	return timezone;
}

time_t datetime2ts(int year, int month, int day, int hour, int minutes, int seconds, char *tz) {
	char date[20];
	time_t t;
	struct tm tm = {0};
	sprintf(date, "%d-%d-%d %d:%d:%d", year, month, day, hour, minutes, seconds);
	strptime(date, "%Y-%m-%d %T", &tm);
	if(tz) {
		setenv("TZ", tz, 1);	
	}
	t = mktime(&tm);
	if(tz) {
		unsetenv("TZ");
	}
	return t;
}

int tzoffset(char *tz1, char *tz2) {
    time_t utc, tzsearch, now;
	struct tm *tm = NULL;
	now = time(NULL);
	tm = localtime(&now);

	setenv("TZ", tz1, 1);	
	utc = mktime(tm);
	setenv("TZ", tz2, 1);
	tzsearch = mktime(tm);
	unsetenv("TZ");
	return (int)((utc-tzsearch)/3600);
}

int ctzoffset(void) {
    time_t tm1, tm2;
	struct tm *t2;
    tm1 = time(NULL);
    t2 = gmtime(&tm1);
    tm2 = mktime(t2);
	localtime(&tm1);
	return ((tm1 - tm2)/3600);
}

int isdst(char *tz) {
	char UTC[] = "UTC";
	time_t now = 0;
	struct tm *tm = NULL;
	now = time(NULL);
	tm = gmtime(&now);
	tm->tm_hour += tzoffset(UTC, tz);

	setenv("TZ", tz, 1);
	mktime(tm);
	unsetenv("TZ");

	return tm->tm_isdst;
}