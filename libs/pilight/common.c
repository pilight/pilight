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

#ifndef __FreeBSD__
	#define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <libgen.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#ifndef __USE_XOPEN
	#define __USE_XOPEN
#endif
#include <sys/time.h>
#include <time.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "settings.h"
#include "common.h"
#include "log.h"

static unsigned int ***whitelist_cache = NULL;
static unsigned int whitelist_number;
static unsigned char validchar[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#ifdef __FreeBSD__
int findproc(char *cmd, char *args, int loosely) {
#else
pid_t findproc(char *cmd, char *args, int loosely) {
#endif
	DIR* dir;
	struct dirent* ent;
	char *pch = NULL, fname[512], cmdline[1024];
	int fd = 0, ptr = 0, match = 0, i = 0, y = '\n';

	if(!(dir = opendir("/proc"))) {
#ifdef __FreeBSD__
		system("mount -t procfs proc /proc");
		if(!(dir = opendir("/proc"))) {
			return -1;
		}
#else
       	return -1;
#endif
	}

    while((ent = readdir(dir)) != NULL) {
		if(isNumeric(ent->d_name) == 0) {
			sprintf(fname, "/proc/%s/cmdline", ent->d_name);
			if((fd = open(fname, O_RDONLY, 0)) > -1) {
				memset(cmdline, '\0', sizeof(cmdline));
				if((ptr = (int)read(fd, cmdline, sizeof(cmdline)-1)) > -1) {
					i = 0, match = 0, y = '\n';
					/* Replace all NULL terminators for newlines */
					for(i=0;i<ptr;i++) {
						if(i < ptr && cmdline[i] == '\0') {
							cmdline[i] = (char)y;
							y = ' ';
						}
					}
					cmdline[ptr] = '\0';
					match = 0;
					/* Check if program matches */
					if((pch = strtok(cmdline, "\n")) == NULL) {
						close(fd);
						continue;
					}
					if((strcmp(pch, cmd) == 0 && loosely == 0)
					   || (strstr(pch, cmd) != NULL && loosely == 1)) {
						match++;
					}

					if(args != NULL && match == 1) {
						if((pch = strtok(NULL, "\n")) == NULL) {
							close(fd);
							continue;
						}
						if(strcmp(pch, args) == 0) {
							match++;
						}

						if(match == 2) {
							pid_t pid = (pid_t)atol(ent->d_name);
							close(fd);
							closedir(dir);
							return pid;
						}
					} else if(match) {
						pid_t pid = (pid_t)atol(ent->d_name);
						close(fd);
						closedir(dir);
						return pid;
					}
				}
				close(fd);
			}
		}
	}
	closedir(dir);
	return -1;
}

int isNumeric(char * s) {
    if(s == NULL || *s == '\0' || *s == ' ')
		return EXIT_FAILURE;
    char *p;
    strtod(s, &p);
    return (*p == '\0') ? EXIT_SUCCESS : EXIT_FAILURE;
}

void sfree(void **addr) {
	void ** __p = addr;
	if(*(__p) != NULL) {
		free(*(__p));
		*(__p) = NULL;
	}
}

int name2uid(char const *name) {
	if(name) {
		struct passwd *pwd = getpwnam(name); /* don't free, see getpwnam() for details */
		if(pwd) {
			return (int)pwd->pw_uid;
		}
	}
	return -1;
}


int which(const char *program) {
	char path[1024];
	strcpy(path, getenv("PATH"));
	char *pch = strtok(path, ":");
	while(pch) {
		char exec[strlen(pch)+8];
		strcpy(exec, pch);
		strcat(exec, "/");
		strcat(exec, program);

		if(access(exec, X_OK) != -1) {
			return 0;
		}
		pch = strtok(NULL, ":");
	}
	return -1;
}

int ishex(int x) {
	return(x >= '0' && x <= '9') || (x >= 'a' && x <= 'f') || (x >= 'A' && x <= 'F');
}

const char *rstrstr(const char* haystack, const char* needle) {
	char* loc = 0;
	char* found = 0;
	size_t pos = 0;

	while ((found = strstr(haystack + pos, needle)) != 0) {
		loc = found;
		pos = (size_t)((found - haystack) + 1);
	}

	return loc;
}

void alpha_random(char *s, const int len) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
	int i = 0;

    for(i = 0; i < len; ++i) {
        s[i] = alphanum[(unsigned int)rand() % (sizeof(alphanum) - 1)];
    }

    s[len] = 0;
}

int urldecode(const char *s, char *dec) {
	char *o;
	const char *end = s + strlen(s);
	int c;

	for(o = dec; s <= end; o++) {
		c = *s++;
		if(c == '+') {
			c = ' ';
		} else if(c == '%' && (!ishex(*s++) || !ishex(*s++)	|| !sscanf(s - 2, "%2x", &c))) {
			return -1;
		}
		if(dec) {
			sprintf(o, "%c", c);
		}
	}

	return (int)(o - dec);
}

int base64decode(unsigned char *dest, unsigned char *src, int l) {
	static char inalphabet[256], decoder[256];
	int i, bits, c, char_count;
	int rpos;
	int wpos = 0;

	for(i=(sizeof validchar)-1;i>=0;i--) {
		inalphabet[validchar[i]] = 1;
		decoder[validchar[i]] = (char)i;
	}

	char_count = 0;
	bits = 0;
	for(rpos=0;rpos<l;rpos++) {
		c = src[rpos];

		if(c == '=') {
			break;
		}

		if(c > 255 || !inalphabet[c]) {
			continue;
		}

		bits += decoder[c];
		char_count++;
		if(char_count < 4) {
			bits <<= 6;
		} else {
			dest[wpos++] = (unsigned char)(bits >> 16);
			dest[wpos++] = (unsigned char)((bits >> 8) & 0xff);
			dest[wpos++] = (unsigned char)(bits & 0xff);
			bits = 0;
			char_count = 0;
		}
	}

	switch(char_count) {
		case 1:
		return -1;
		case 2:
			dest[wpos++] = (unsigned char)(bits >> 10);
		break;
		case 3:
			dest[wpos++] = (unsigned char)(bits >> 16);
			dest[wpos++] = (unsigned char)((bits >> 8) & 0xff);
		break;
		default:
		break;
	}

	return wpos;
}

void rmsubstr(char *s, const char *r) {
	while((s=strstr(s, r))) {
		size_t l = strlen(r);
		memmove(s ,s+l, 1+strlen(s+l));
	}
}

char *hostname(void) {
	char name[255] = {'\0'};
	char *host = NULL;
	gethostname(name, 254);
	if(strlen(name) > 0) {
		char *pch = strtok(name, ".");
		if(pch != NULL) {
			if(!(host = malloc(strlen(pch)+1))) {
				logprintf(LOG_ERR, "out of memory");
				exit(EXIT_FAILURE);
			}
			strcpy(host, pch);
		}
	}
	return host;
}


char *distroname(void) {
	char dist[32];
	memset(dist, '\0', 32);
	char *distro = NULL;

#ifdef __FreeBSD__
	strcpy(dist, "FreeBSD/0.0");
#else
	int rc = 1;
	struct stat sb;
	if((rc = stat("/etc/redhat-release", &sb)) == 0) {
		strcpy(dist, "RedHat/0.0");
	} else if((rc = stat("/etc/SuSE-release", &sb)) == 0) {
		strcpy(dist, "SuSE/0.0");
	} else if((rc = stat("/etc/mandrake-release", &sb)) == 0) {
		strcpy(dist, "Mandrake/0.0");
	} else if((rc = stat("/etc/debian-release", &sb)) == 0) {
		strcpy(dist, "Debian/0.0");
	} else if((rc = stat("/etc/debian_version", &sb)) == 0) {
		strcpy(dist, "Debian/0.0");
	} else {
		strcpy(dist, "Unknown/0.0");
	}
#endif
	if(strlen(dist) > 0) {
		if(!(distro = malloc(strlen(dist)+1))) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		strcpy(distro, dist);
		return distro;
	} else {
		return NULL;
	}
}


char *genuuid(char *ifname) {
	char *mac = NULL, *upnp_id = NULL;
	char a[1024], serial[UUID_LENGTH+1];

	memset(serial, '\0', UUID_LENGTH+1);
	FILE *fp = fopen("/proc/cpuinfo", "r");
	if(fp != NULL) {
		while(!feof(fp)) {
			if(fgets(a, 1024, fp) == 0) {
				break;
			}
			if(strstr(a, "Serial") != NULL) {
				sscanf(a, "Serial          : %16s%*[ \n\r]", (char *)&serial);
				memmove(&serial[5], &serial[4], 16);
				serial[4] = '-';
				memmove(&serial[8], &serial[7], 13);
				serial[7] = '-';
				memmove(&serial[11], &serial[10], 10);
				serial[10] = '-';
				memmove(&serial[14], &serial[13], 7);
				serial[13] = '-';
				upnp_id = malloc(UUID_LENGTH+1);
				strcpy(upnp_id, serial);
				fclose(fp);
				return upnp_id;
			}
		}
		fclose(fp);
	}

#if defined(SIOCGIFHWADDR)
	int i = 0;
	int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	if(!(mac = malloc(13))) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	memset(mac, '\0', 13);
	struct ifreq s;

	memset(&s, '\0', sizeof(struct ifreq));
	strcpy(s.ifr_name, ifname);
	if(ioctl(fd, SIOCGIFHWADDR, &s) == 0) {
		for(i = 0; i < 12; i+=2) {
			sprintf(&mac[i], "%02x", (unsigned char)s.ifr_addr.sa_data[i/2]);
		}
	}
	close(fd);
#elif defined(SIOCGIFADDR)
	int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	if(!(mac = malloc(13))) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	memset(mac, '\0', 13);
	struct ifreq s;
	memset(&s.ifr_name, '\0', sizeof(s.ifr_name));
	memset(&s, '\0', sizeof(struct ifreq));
	strcpy(s.ifr_name, ifname);
	if(ioctl(fd, SIOCGIFADDR, &s) == 0) {
		int i;
		for(i = 0; i < 12; i+=2) {
			sprintf(&mac[i], "%02x", (unsigned char)s.ifr_addr.sa_data[i/2]);
		}
	}
	close(fd);
#elif defined(HAVE_GETIFADDRS)
	ifaddrs* iflist;
	if(getifaddrs(&iflist) == 0) {
		for(ifaddrs* cur = iflist; cur; cur = cur->ifa_next) {
			if((cur->ifa_addr->sa_family == AF_LINK) && (strcmp(cur->ifa_name, if_name) == 0) && cur->ifa_addr) {
				sockaddr_dl* sdl = (sockaddr_dl*)cur->ifa_addr;
				memcpy(&mac[i], LLADDR(sdl), sdl->sdl_alen);
				break;
			}
		}

		freeifaddrs(iflist);
	}
#endif

	if(strlen(mac) > 0) {
		upnp_id = malloc(UUID_LENGTH);
		sprintf(upnp_id,
				"0000-%c%c-%c%c-%c%c-%c%c%c%c%c%c",
				mac[0], mac[1], mac[2],
				mac[3], mac[4], mac[5],
				mac[6], mac[7], mac[9],
				mac[10], mac[11], mac[12]);
		sfree((void *)&mac);
		return upnp_id;
	}
	return NULL;
}

#ifdef __FreeBSD__
struct sockaddr *sockaddr_dup(struct sockaddr *sa) {
	struct sockaddr *ret;
	socklen_t socklen;
#ifdef HAVE_SOCKADDR_SA_LEN
	socklen = sa->sa_len;
#else
	socklen = sizeof(struct sockaddr_storage);
#endif
	if(!(ret = calloc(1, socklen))) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	if (ret == NULL)
		return NULL;
	memcpy(ret, sa, socklen);
	return ret;
}

int rep_getifaddrs(struct ifaddrs **ifap) {
	struct ifconf ifc;
	char buff[8192];
	int fd, i, n;
	struct ifreq ifr, *ifrp=NULL;
	struct ifaddrs *curif = NULL, *ifa = NULL;
	struct ifaddrs *lastif = NULL;

	*ifap = NULL;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		return -1;
	}

	ifc.ifc_len = sizeof(buff);
	ifc.ifc_buf = buff;

	if (ioctl(fd, SIOCGIFCONF, &ifc) != 0) {
		close(fd);
		return -1;
	}

#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifdef __FreeBSD__
#define ifreq_size(i) max(sizeof(struct ifreq),\
     sizeof((i).ifr_name)+(i).ifr_addr.sa_len)
#else
#define ifreq_size(i) sizeof(struct ifreq)
#endif

	n = ifc.ifc_len;

	for (i = 0; i < n; i+= (int)ifreq_size(*ifrp) ) {
		int match = 0;
		ifrp = (struct ifreq *)((char *) ifc.ifc_buf+i);
		for(ifa = *ifap; ifa != NULL; ifa = ifa->ifa_next) {
			if(strcmp(ifrp->ifr_name, ifa->ifa_name) == 0) {
				match = 1;
				break;
			}
		}
		if(match == 1) {
			continue;
		}
		curif = calloc(1, sizeof(struct ifaddrs));
		if(curif == NULL) {
			freeifaddrs(*ifap);
			close(fd);
			return -1;
		}

		curif->ifa_name = malloc(sizeof(IFNAMSIZ)+1);
		if(curif->ifa_name == NULL) {
			free(curif);
			freeifaddrs(*ifap);
			close(fd);
			return -1;
		}
		strncpy(curif->ifa_name, ifrp->ifr_name, IFNAMSIZ);
		strncpy(ifr.ifr_name, ifrp->ifr_name, IFNAMSIZ);

		curif->ifa_flags = (unsigned int)ifr.ifr_flags;
		curif->ifa_dstaddr = NULL;
		curif->ifa_data = NULL;
		curif->ifa_next = NULL;

		curif->ifa_addr = NULL;
		if (ioctl(fd, SIOCGIFADDR, &ifr) != -1) {
			curif->ifa_addr = sockaddr_dup(&ifr.ifr_addr);
			if (curif->ifa_addr == NULL) {
				free(curif->ifa_name);
				free(curif);
				freeifaddrs(*ifap);
				close(fd);
				return -1;
			}
		}


		curif->ifa_netmask = NULL;
		if (ioctl(fd, SIOCGIFNETMASK, &ifr) != -1) {
			curif->ifa_netmask = sockaddr_dup(&ifr.ifr_addr);
			if (curif->ifa_netmask == NULL) {
				if (curif->ifa_addr != NULL) {
					free(curif->ifa_addr);
				}
				free(curif->ifa_name);
				free(curif);
				freeifaddrs(*ifap);
				close(fd);
				return -1;
			}
		}

		if (lastif == NULL) {
			*ifap = curif;
		} else {
			lastif->ifa_next = curif;
		}
		lastif = curif;
	}

	close(fd);

	return 0;
}

void rep_freeifaddrs(struct ifaddrs *ifaddr) {
	struct ifaddrs *ifa;
	while(ifaddr) {
		ifa = ifaddr;
		sfree((void *)&ifa->ifa_name);
		sfree((void *)&ifa->ifa_addr);
		sfree((void *)&ifa->ifa_netmask);
		ifaddr = ifaddr->ifa_next;
		sfree((void *)&ifa);
	}
	sfree((void *)&ifaddr);
}
#endif

int whitelist_check(char *ip) {
	char *whitelist = NULL;
	unsigned int client[4] = {0};
	int x = 0, i = 0, error = 1;
	char *pch = NULL;
	char wip[16] = {'\0'};

	/* Check if there are any whitelisted ip address */
	if(settings_find_string("whitelist", &whitelist) != 0) {
		return 0;
	}

	if(strlen(whitelist) == 0) {
		return 0;
	}

	/* Explode ip address to a 4 elements int array */
	pch = strtok(ip, ".");
	x = 0;
	while(pch) {
		client[x] = (unsigned int)atoi(pch);
		x++;
		pch = strtok(NULL, ".");
	}
	sfree((void *)&pch);

	if(!whitelist_cache) {
		char *tmp = whitelist;
		x = 0;
		/* Loop through all whitelised ip addresses */
		while(*tmp != '\0') {
			/* Remove any comma's and spaces */
			while(*tmp == ',' || *tmp == ' ') {
				tmp++;
			}
			/* Save ip address in temporary char array */
			wip[x] = *tmp;
			x++;
			tmp++;

			/* Each ip address is either terminated by a comma or EOL delimiter */
			if(*tmp == '\0' || *tmp == ',') {
				x = 0;
				if((whitelist_cache = realloc(whitelist_cache, (sizeof(unsigned int ***)*(whitelist_number+1)))) == NULL) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				if((whitelist_cache[whitelist_number] = malloc(sizeof(unsigned int **)*2)) == NULL) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				/* Lower boundary */
				if((whitelist_cache[whitelist_number][0] = malloc(sizeof(unsigned int *)*4)) == NULL) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				/* Upper boundary */
				if((whitelist_cache[whitelist_number][1] = malloc(sizeof(unsigned int *)*4)) == NULL) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}

				/* Turn the whitelist ip address into a upper and lower boundary.
				   If the ip address doesn't contain a wildcard, then the upper
				   and lower boundary are the same. If the ip address does contain
				   a wildcard, then this lower boundary number will be 0 and the
				   upper boundary number 255. */
				i = 0;
				pch = strtok(wip, ".");
				while(pch) {
					if(strcmp(pch, "*") == 0) {
						whitelist_cache[whitelist_number][0][i] = 0;
						whitelist_cache[whitelist_number][1][i] = 255;
					} else {
						whitelist_cache[whitelist_number][0][i] = (unsigned int)atoi(pch);
						whitelist_cache[whitelist_number][1][i] = (unsigned int)atoi(pch);
					}
					pch = strtok(NULL, ".");
					i++;
				}
				sfree((void *)&pch);
				memset(wip, '\0', 16);
				whitelist_number++;
			}
		}
	}

	for(x=0;x<whitelist_number;x++) {
		/* Turn the different ip addresses into one single number and compare those
		   against each other to see if the ip address is inside the lower and upper
		   whitelisted boundary */
		unsigned int wlower = whitelist_cache[x][0][0] << 24 | whitelist_cache[x][0][1] << 16 | whitelist_cache[x][0][2] << 8 | whitelist_cache[x][0][3];
		unsigned int wupper = whitelist_cache[x][1][0] << 24 | whitelist_cache[x][1][1] << 16 | whitelist_cache[x][1][2] << 8 | whitelist_cache[x][1][3];
		unsigned int nip = client[0] << 24 | client[1] << 16 | client[2] << 8 | client[3];

		/* Always allow 127.0.0.1 connections */
		if((nip >= wlower && nip <= wupper) || (nip == 2130706433)) {
			error = 0;
		}
	}

	return error;
}

void whitelist_free(void) {
	int i = 0;
	if(whitelist_cache) {
		for(i=0;i<whitelist_number;i++) {
			sfree((void *)&whitelist_cache[i][0]);
			sfree((void *)&whitelist_cache[i][1]);
			sfree((void *)&whitelist_cache[i]);
		}
		sfree((void *)&whitelist_cache);
	}
}

/* Check if a given path exists */
int path_exists(char *fil) {
	struct stat s;
	char tmp[strlen(fil)+1];
	strcpy(tmp, fil);
	char *filename = basename(tmp);
	char path[(strlen(tmp)-strlen(filename))+1];
	size_t i = (strlen(tmp)-strlen(filename));

	memset(path, '\0', sizeof(path));
	memcpy(path, tmp, i);
	snprintf(path, i, "%s", tmp);

	if(strcmp(filename, tmp) != 0) {
		int err = stat(path, &s);
		if(err == -1) {
			if(ENOENT == errno) {
				return EXIT_FAILURE;
			} else {
				return EXIT_FAILURE;
			}
		} else {
			if(S_ISDIR(s.st_mode)) {
				return EXIT_SUCCESS;
			} else {
				return EXIT_FAILURE;
			}
		}
	}
	return EXIT_SUCCESS;
}

/* Copyright (C) 1995 Ian Jackson <iwj10@cus.cam.ac.uk> */
/* Copyright (C) 1995 Ian Jackson <iwj10@cus.cam.ac.uk> */
//  1: val > ref
// -1: val < ref
//  0: val == ref
int vercmp(char *val, char *ref) {
	int vc, rc;
	long vl, rl;
	char *vp, *rp;
	char *vsep, *rsep;

	if(!val) {
		strcpy(val, "");
	}
	if(!ref) {
		strcpy(ref, "");
	}
	while(1) {
		vp = val;
		while(*vp && !isdigit(*vp)) {
			vp++;
		}
		rp = ref;
		while(*rp && !isdigit(*rp)) {
			rp++;
		}
		while(1) {
			vc =(val == vp) ? 0 : *val++;
			rc =(ref == rp) ? 0 : *ref++;
			if(!rc && !vc) {
				break;
			}
			if(vc && !isalpha(vc)) {
				vc += 256;
			}
			if(rc && !isalpha(rc)) {
				rc += 256;
			}
			if(vc != rc) {
				return vc - rc;
			}
		}
		val = vp;
		ref = rp;
		vl = 0;
		if(isdigit(*vp)) {
			vl = strtol(val, (char**)&val, 10);
		}
		rl = 0;
		if(isdigit(*rp)) {
			rl = strtol(ref, (char**)&ref, 10);
		}
		if(vl != rl) {
			return (int)(vl - rl);
		}

		vc = *val;
		rc = *ref;
		vsep = strchr(".-", vc);
		rsep = strchr(".-", rc);

		if((vsep && !rsep) || !*val) {
			return 0;
		}

		if((!vsep && rsep) || !*ref) {
			return +1;
		}

		if(!*val && !*ref) {
			return 0;
		}
	}
}

int str_replace(char *search, char *replace, char **str) {
	char *target = *str;
	unsigned short match = 0;
	int len = (int)strlen(target);
	int nlen = 0;
	int slen = (int)strlen(search);
	int rlen = (int)strlen(replace);
	int x = 0;

	while(x < len) {
		if(strncmp(&target[x], search, (size_t)slen) == 0) {
			match = 1;
			int rpos = (x + (slen - rlen));
			if(rpos < 0) {
				slen -= rpos;
				rpos = 0;
			}
			nlen = len - (slen - rlen);
			if(len < nlen) {
				if(!(target = realloc(target, (size_t)nlen+1))) {
					printf("out of memory\n");
				}
				memset(&target[len], '\0', (size_t)(nlen-len));
			}
			len = nlen;

			memmove(&target[x], &target[rpos], (size_t)(len-x));
			strncpy(&target[x], replace, (size_t)rlen);
			target[len] = '\0';
			x += rlen-1;
		}
		x++;
	}
	if(match) {
		return (int)len;
	} else {
		return -1;
	}
}

int stricmp(char const *a, char const *b) {
    for(;; a++, b++) {
        int d = tolower(*a) - tolower(*b);
        if (d != 0 || !*a)
            return d;
    }
}
