/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef __FreeBSD__
	#define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#ifdef _WIN32
	#if _WIN32_WINNT < 0x0501
		#undef _WIN32_WINNT
		#define _WIN32_WINNT 0x0501
	#endif
	#include <winsock2.h>
	#include <windows.h>
	#include <psapi.h>
	#include <tlhelp32.h>
	#include <ws2tcpip.h>
	#include <iphlpapi.h>
#else
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <netdb.h>
	#include <arpa/inet.h>
	#include <sys/wait.h>
	#include <net/if.h>
	#include <ifaddrs.h>
	#include <pwd.h>
	#include <sys/mount.h>
	#include <sys/ioctl.h>
	#include <pthread.h>
	#include <unistd.h>
#endif
#ifdef __sun__
#include <procfs.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#ifndef __USE_XOPEN
	#define __USE_XOPEN
#endif
#include <time.h>

#if defined(WIN32) || defined(WIN64)
	// Copied from linux libc sys/stat.h:
	#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
	#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#include "mem.h"
#include "common.h"
#include "network.h"
#include "log.h"
#include "../psutil/psutil.h"

char *progname = NULL;

static const char base64table[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3',
	'4', '5', '6', '7', '8', '9', '+', '/'
};

static uv_mutex_t atomic_lock;
static int atomic_init = 0;

void atomicinit(void) {
	if(atomic_init == 0) {
		atomic_init = 1;
		uv_mutex_init(&atomic_lock);
	}
}

void atomiclock(void) {
	atomicinit();
	uv_mutex_lock(&atomic_lock);
}

void atomicunlock(void) {
	uv_mutex_unlock(&atomic_lock);
}

void array_free(char ***array, int len) {
	int i = 0;
	if(len > 0) {
		for(i=0;i<len;i++) {
			FREE((*array)[i]);
		}
		FREE((*array));
	}
}

unsigned int explode(char *str, const char *delimiter, char ***output) {
	if(str == NULL || output == NULL) {
		return 0;
	}
	int i = 0, n = 0, y = 0;
	size_t l = 0, p = 0;
	if(delimiter != NULL) {
		l = strlen(str);
		p = strlen(delimiter);
	}
	while(i < l) {
		if(strncmp(&str[i], delimiter, p) == 0) {
			if((i-y) > 0) {
				if((*output = REALLOC(*output, sizeof(char *)*(n+1))) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				if(((*output)[n] = MALLOC((i-y)+1)) == NULL) {
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				strncpy((*output)[n], &str[y], i-y);
				(*output)[n][(i-y)] = '\0';
				n++;
			}
			y=i+p;
		}
		i++;
	}
	if(strlen(&str[y]) > 0) {
		if((*output = REALLOC(*output, sizeof(char *)*(n+1))) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		if(((*output)[n] = MALLOC((i-y)+1)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		strncpy((*output)[n], &str[y], i-y);
		(*output)[n][(i-y)] = '\0';
		n++;
	}
	return n;
}

#ifdef _WIN32
void usleep(unsigned long value) {
	struct timeval tv;
	tv.tv_sec = value / 1000000;
	tv.tv_usec = value % 1000000;
	select(0, NULL, NULL, NULL, &tv);
}

int gettimeofday(struct timeval *tp, struct timezone *tzp) {
	// Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
	static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

	SYSTEMTIME  system_time;
	FILETIME    file_time;
	uint64_t    time;

	GetSystemTime( &system_time );
	SystemTimeToFileTime( &system_time, &file_time );
	time =  ((uint64_t)file_time.dwLowDateTime )      ;
	time += ((uint64_t)file_time.dwHighDateTime) << 32;

	tp->tv_sec  = (long) ((time - EPOCH) / 10000000L);
	tp->tv_usec = (long) (system_time.wMilliseconds * 1000);
	return 0;
}

int check_instances(const wchar_t *prog) {
	HANDLE m_hStartEvent = CreateEventW(NULL, FALSE, FALSE, prog);
	if(m_hStartEvent == NULL) {
		CloseHandle(m_hStartEvent);
		return 0;
	}

	if(GetLastError() == ERROR_ALREADY_EXISTS) {
		CloseHandle(m_hStartEvent);
		m_hStartEvent = NULL;
		return 0;
	}
	return -1;
}

int setenv(const char *name, const char *value, int overwrite) {
	if(name == NULL) {
		errno = EINVAL;
		return -1;
	}
	if(overwrite == 0 && getenv(name) != NULL) {
		return 0; // name already defined and not allowed to overwrite. Treat as OK.
	}
	if(value == NULL) {
		return unsetenv(name);
	}
	char *c = MALLOC(strlen(name)+1+strlen(value)+1); // one for "=" + one for term zero
	if(c == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcat(c, name);
	strcat(c, "=");
	strcat(c, value);
	int r = putenv(c);
	FREE(c);
	return r;
}

int unsetenv(const char *name) {
	if(name == NULL) {
		errno = EINVAL;
		return -1;
	}
	char *c = MALLOC(strlen(name)+1+1); // one for "=" + one for term zero
	if(c == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcat(c, name);
	strcat(c, "=");
	int r = putenv(c);
	FREE(c);
	return r;
}
#endif

int isrunning(const char *program) {
	if(program == NULL) {
		return -1;
	}
	int i = 0;
	char name[1024], *p = name;
	memset(&name, '\0', sizeof(name));

	for(i=0;i<psutil_max_pid();i++) {
		if(psutil_proc_name(i, &p, sizeof(name)) == 0) {
			if(strcmp(name, program) == 0) {
				return i;
			}
		}
	}
	return -1;
}

int isNumeric(char *s) {
	if(s == NULL || *s == '\0' || *s == ' ')
		return -1;

	char *p = NULL;
	strtod(s, &p);
	return (*p == '\0') ? 0 : -1;
}

int nrDecimals(char *s) {
	unsigned int b = 0, c = strlen(s), i = 0;
	int a = 0;
	for(i=0;i<c;i++) {
		if(b == 1) {
			a++;
		}
		if(s[i] == '.') {
			b = 1;
		}
	}
	return a;
}

int name2uid(char const *name) {
#ifndef _WIN32
	if(name != NULL) {
		struct passwd *pwd = getpwnam(name); /* don't free, see getpwnam() for details */
		if(pwd) {
			return (int)pwd->pw_uid;
		}
	}
#endif
	return -1;
}

int ishex(int x) {
	return(x >= '0' && x <= '9') || (x >= 'a' && x <= 'f') || (x >= 'A' && x <= 'F');
}

const char *rstrstr(const char* haystack, const char* needle) {
	char *loc = NULL;
	char *found = NULL;
	size_t pos = 0;

	if(haystack == NULL || needle == NULL) {
		return NULL;
	}

	while((found = strstr(haystack + pos, needle)) != 0) {
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
	char *o = NULL;
	const char *end = s + strlen(s);
	int c = 0;

	for(o = dec; s <= end; o++) {
		c = *s++;
		if(c == '+') {
			c = ' ';
		} else if(c == '%' && (!ishex(*s++) || !ishex(*s++) || !sscanf(s - 2, "%2x", &c))) {
			return -1;
		}
		if(dec) {
			sprintf(o, "%c", c);
		}
	}

	return (int)(o - dec);
}

static char to_hex(char code) {
  static char hex[] = "0123456789abcdef";
  return hex[code & 15];
}

char *urlencode(char *str) {
	if(str == NULL) {
		return NULL;
	}
	char *pstr = str, *buf = MALLOC(strlen(str) * 3 + 1), *pbuf = buf;
	while(*pstr) {
		if(isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~')
			*pbuf++ = *pstr;
		else if(*pstr == ' ')
			*pbuf++ = '%', *pbuf++ = '2', *pbuf++ = '0';
		else
			*pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), *pbuf++ = to_hex(*pstr & 15);
		pstr++;
	}
	*pbuf = '\0';
	return buf;
}

char *base64decode(char *src, size_t len, size_t *decsize) {
  unsigned int i = 0;
  unsigned int j = 0;
  unsigned int l = 0;
  size_t size = 0;
  char *dec = NULL;
  char buf[3];
  char tmp[4];

  dec = MALLOC(0);
  if(dec == NULL) {
		OUT_OF_MEMORY /* LCOV_EXCL_LINE */
	}

  while(len--) {
    if('=' == src[j]) {
			break;
		}
    if(!(isalnum(src[j]) || src[j] == '+' || src[j] == '/')) {
			break;
		}

    tmp[i++] = src[j++];

    if(i == 4) {
      for(i = 0; i < 4; ++i) {
        for(l = 0; l < 64; ++l) {
          if(tmp[i] == base64table[l]) {
            tmp[i] = (char)l;
            break;
          }
        }
      }

      buf[0] = (char)((tmp[0] << 2) + ((tmp[1] & 0x30) >> 4));
      buf[1] = (char)(((tmp[1] & 0xf) << 4) + ((tmp[2] & 0x3c) >> 2));
      buf[2] = (char)(((tmp[2] & 0x3) << 6) + tmp[3]);

      dec = REALLOC(dec, size + 3);
      for(i = 0; i < 3; ++i) {
        dec[size++] = buf[i];
      }

      i = 0;
    }
  }

  if(i > 0) {
    for(j = i; j < 4; ++j) {
      tmp[j] = '\0';
    }

    for(j = 0; j < 4; ++j) {
			for(l = 0; l < 64; ++l) {
				if(tmp[j] == base64table[l]) {
					tmp[j] = (char)l;
					break;
				}
			}
    }

    buf[0] = (char)((tmp[0] << 2) + ((tmp[1] & 0x30) >> 4));
    buf[1] = (char)(((tmp[1] & 0xf) << 4) + ((tmp[2] & 0x3c) >> 2));
    buf[2] = (char)(((tmp[2] & 0x3) << 6) + tmp[3]);

    dec = REALLOC(dec, (size_t)(size + (size_t)(i - 1)));
    for(j = 0; (j < i - 1); ++j) {
      dec[size++] = buf[j];
    }
  }

  dec = REALLOC(dec, size + 1);
  dec[size] = '\0';

  if(decsize != NULL) {
		*decsize = size;
	}

  return dec;
}

char *base64encode(char *src, size_t len) {
  unsigned int i = 0;
  unsigned int j = 0;
  char *enc = NULL;
  size_t size = 0;
  char buf[4];
  char tmp[3];

  enc = MALLOC(0);
  if(enc == NULL) {
		OUT_OF_MEMORY /* LCOV_EXCL_LINE */
	}

  while(len--) {
    tmp[i++] = *(src++);

    if(i == 3) {
      buf[0] = (char)((tmp[0] & 0xfc) >> 2);
      buf[1] = (char)(((tmp[0] & 0x03) << 4) + ((tmp[1] & 0xf0) >> 4));
      buf[2] = (char)(((tmp[1] & 0x0f) << 2) + ((tmp[2] & 0xc0) >> 6));
      buf[3] = (char)(tmp[2] & 0x3f);

      enc = REALLOC(enc, size + 4);
      for(i = 0; i < 4; ++i) {
        enc[size++] = base64table[(int)buf[i]];
      }

      i = 0;
    }
  }

  if(i > 0) {
    for(j = i; j < 3; ++j) {
      tmp[j] = '\0';
    }

		buf[0] = (char)((tmp[0] & 0xfc) >> 2);
		buf[1] = (char)(((tmp[0] & 0x03) << 4) + ((tmp[1] & 0xf0) >> 4));
		buf[2] = (char)(((tmp[1] & 0x0f) << 2) + ((tmp[2] & 0xc0) >> 6));
		buf[3] = (char)(tmp[2] & 0x3f);

    for(j = 0; (j < i + 1); ++j) {
      enc = REALLOC(enc, size+1);
      enc[size++] = base64table[(int)buf[j]];
    }

    while((i++ < 3)) {
      enc = REALLOC(enc, size+1);
      enc[size++] = '=';
    }
  }

  enc = REALLOC(enc, size+1);
  enc[size] = '\0';

  return enc;
}

char *hostname(void) {
	char name[255] = {'\0'};
	char *host = NULL, **array = NULL;
	unsigned int n = 0, i = 0;

#ifdef _WIN32
	WSADATA wsa;

	if(WSAStartup(0x202, &wsa) != 0) {
		logprintf(LOG_ERR, "WSAStartup");
		exit(EXIT_FAILURE);
	}
#endif	
	
	if(gethostname(name, 254) != 0) {
		/* LCOV_EXCL_START */
		logprintf(LOG_ERR, "gethostbyname: %s", strerror(errno));
		return NULL;
		/* LCOV_EXCL_STOP */
	}
	if(strlen(name) > 0) {
		n = explode(name, ".", &array);
		if(n > 0) {
			if((host = MALLOC(strlen(array[0])+1)) == NULL) {
				OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
			}
			strcpy(host, array[0]);
		}
	}
	for(i=0;i<n;i++) {
		FREE(array[i]);
	}
	if(n > 0) {
		FREE(array);
	}
	return host;
}

char *distroname(void) {
	char dist[32];
	memset(dist, '\0', 32);
	char *distro = NULL;

#ifdef _WIN32
	strcpy(dist, "Windows");
#elif defined(__FreeBSD__)
	strcpy(dist, "FreeBSD/0.0");
#else
	int rc = 1;
	struct stat sb;
	/* LCOV_EXCL_START */
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
	/* LCOV_EXCL_STOP */
#endif
	if(strlen(dist) > 0) {
		if((distro = MALLOC(strlen(dist)+1)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		strcpy(distro, dist);
		return distro;
	} else {
		return NULL;
	}
}

/* The UUID is either generated from the
   processor serial number or from the
   onboard LAN controller mac address */
char *genuuid(char *ifname) {
	char mac[ETH_ALEN], *upnp_id = NULL, *p = mac;
	char serial[UUID_LENGTH+1];

	memset(serial, '\0', UUID_LENGTH+1);
#ifndef _WIN32
	char a[1024];
	FILE *fp = fopen("/proc/cpuinfo", "r");
	if(fp != NULL) {
		while(!feof(fp)) {
			if(fgets(a, 1024, fp) == 0) {
				break;
			}
			/* LCOV_EXCL_START */
			if(strstr(a, "Serial") != NULL) {
				sscanf(a, "Serial          : %16s%*[ \n\r]", (char *)&serial);
				if(strlen(serial) > 0 &&
					 ((isNumeric(serial) == EXIT_SUCCESS && atoi(serial) > 0) ||
					  (isNumeric(serial) == EXIT_FAILURE))) {
					memmove(&serial[5], &serial[4], 16);
					serial[4] = '-';
					memmove(&serial[8], &serial[7], 13);
					serial[7] = '-';
					memmove(&serial[11], &serial[10], 10);
					serial[10] = '-';
					memmove(&serial[14], &serial[13], 7);
					serial[13] = '-';
					if((upnp_id = MALLOC(UUID_LENGTH+1)) == NULL) {
						OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
					}
					strcpy(upnp_id, serial);
					fclose(fp);
					return upnp_id;
				}
			}
			/* LCOV_EXCL_STOP */
		}
		fclose(fp);
	}

#endif
	if(dev2mac(ifname, &p) == 0) {
		if((upnp_id = MALLOC(UUID_LENGTH+1)) == NULL) {
			OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
		}
		memset(upnp_id, '\0', UUID_LENGTH+1);
		snprintf(upnp_id, UUID_LENGTH,
				"0000-%02x-%02x-%02x-%02x%02x%02x",
				(unsigned char)p[0], (unsigned char)p[1], (unsigned char)p[2],
				(unsigned char)p[3], (unsigned char)p[4], (unsigned char)p[5]);
		return upnp_id;
	}

	return NULL; /* LCOV_EXCL_LINE */
}

/* Check if a given file exists */
int file_exists(char *filename) {
	if(filename == NULL) {
		return -1;
	}
	struct stat sb;
	return stat(filename, &sb);
}

/* Check if a given path exists */
int path_exists(char *fil) {
	if(fil == NULL) {
		return -1;
	}
	struct stat s;
	int len = strlen(fil);
	char *tmp = MALLOC(len+1);
	if(tmp == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}
	strcpy(tmp, fil);

/*
 * dir stat doens't work on windows if path has a trailing slash
 */
#ifdef _WIN32
	if(tmp[len-1] == '\\' || tmp[len-1] == '/') {
		tmp[len-1] = '\0';
	}
#endif

	int err = stat(tmp, &s);
	if(err == -1) {
		FREE(tmp);
		return -1;
	} else {
		if(S_ISDIR(s.st_mode)) {
			FREE(tmp);
			return 0;
		} else {
			FREE(tmp);
			return -1;
		}
	}
	FREE(tmp);
	return 0;
}

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

char *uniq_space(char *str){
	if(str == NULL) {
		return NULL;
	}

	char *from = NULL, *to = NULL;
	int spc=0;
	to = from = str;
	while(1){
		if(spc == 1 && *from == ' ' && to[-1] == ' ') {
			++from;
		} else {
			spc = (*from == ' ') ? 1 : 0;
			*to++ = *from++;
			if(!to[-1]) {
				break;
			}
		}
	}
	return str;
}

int str_replace(char *search, char *replace, char **str) {
	unsigned short match = 0;
	int len = (int)strlen(*str);
	int nlen = 0;
	int slen = (int)strlen(search);
	int rlen = (int)strlen(replace);
	int x = 0;
	while(x < len) {
		if(strncmp(&(*str)[x], search, (size_t)slen) == 0) {
			match = 1;
			int rpos = (x + (slen - rlen));
			if(rpos < 0) {
				slen -= rpos;
				rpos = 0;
			}
			nlen = len - (slen - rlen);
			if(len < nlen) {
				if(((*str) = REALLOC((*str), (size_t)nlen+1)) == NULL) { /*LCOV_EXCL_LINE*/
					OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
				}
				memset(&(*str)[len], '\0', (size_t)(nlen-len));
			}
			len = nlen;
			memmove(&(*str)[x], &(*str)[rpos], (size_t)(len-x));
			strncpy(&(*str)[x], replace, (size_t)rlen);
			(*str)[len] = '\0';
			x += rlen-1;
		}
		x++;
	}
	if(match == 1) {
		return (int)len;
	} else {
		return -1;
	}
}

int strnicmp(char const *a, char const *b, size_t len) {
	int i = 0;

	if(a == NULL || b == NULL) {
		return -1;
	}
	if(len == 0) {
		return 0;
	}

	for(;i++<len; a++, b++) {
		int d = tolower(*a) - tolower(*b);
		if(d != 0 || !*a || i == len) {
			return d;
		}
	}
	return -1;
}

int stricmp(char const *a, char const *b) {
	if(a == NULL || b == NULL) {
		return -1;
	}

	for(;; a++, b++) {
		int d = tolower(*a) - tolower(*b);
		if(d != 0 || !*a) {
			return d;
		}
	}
	return -1;
}

int file_get_contents(char *file, char **content) {
	FILE *fp = NULL;
	size_t bytes = 0;
	struct stat st;

	if(file == NULL || content == NULL) {
		return -1;
	}

	if(file_exists(file) == -1) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "file does not exists: %s", file);
		return -1;
		/*LCOV_EXCL_STOP*/
	}
	
	if((fp = fopen(file, "rb")) == NULL) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "cannot open file: %s", file);
		return -1;
		/*LCOV_EXCL_STOP*/
	}

	fstat(fileno(fp), &st);
	bytes = (size_t)st.st_size;

	if((*content = CALLOC(bytes+1, sizeof(char))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	if(fread(*content, sizeof(char), bytes, fp) == -1) {
		/*LCOV_EXCL_START*/
		logprintf(LOG_ERR, "cannot read file: %s", file);
		return -1;
		/*LCOV_EXCL_STOP*/
	}
	fclose(fp);
	return 0;
}

/*
 * 1: milliseconds
 * 2: seconds
 * 3: minutes
 * 4: hours
 * 5: days
 */
void calc_time_interval(int type, int seconds, int diff, struct timeval *tv) {
	int divide = 0;

	switch(type) {
		case 1:
			divide = (1000*1000);
			seconds *= 1;
		break;
		case 2:
			divide = (1000*1000);
			seconds *= 1000;
		break;
		case 3:
			seconds *= 60;
		break;
		case 4:
			seconds *= (60*60);
		break;
		case 5:
			seconds *= (60*60*24);
		break;
	}
	if(type > 2) {
		divide = 1000;
		seconds *= divide;
	}

	tv->tv_sec = (int)((float)seconds / (float)diff) / divide;
	tv->tv_usec = (int)((float)seconds / (float)diff) % divide;

	if(tv->tv_usec <= 0 && tv->tv_sec == 0) {
		tv->tv_usec = 1;
	}
	if(tv->tv_usec >= 1000) {
		tv->tv_sec += tv->tv_usec / 1000;
		tv->tv_usec = tv->tv_usec % 1000;
	}
}

/* Copyright (c) 2015-2016 Nuxi, https://nuxi.nl/ */
char *_dirname(char *path) {
	const char *in, *prev, *begin, *end;
	char *out;
	size_t prevlen;
	bool skipslash;

	/*
	 * If path is a null pointer or points to an empty string,
	 * dirname() shall return a pointer to the string ".".
	 */
	if(path == NULL || *path == '\0')
		return ((char *)".");

	/* Retain at least one leading slash character. */
	in = out = *path == '/' ? path + 1 : path;

	skipslash = true;
	prev = ".";
	prevlen = 1;
	for(;;) {
		/* Extract the next pathname component. */
		while (*in == '/')
			++in;
		begin = in;
		while(*in != '/' && *in != '\0')
			++in;
		end = in;
		if(begin == end)
			break;

		/*
		 * Copy over the previous pathname component, except if
		 * it's dot. There is no point in retaining those.
		 */
		if(prevlen != 1 || *prev != '.') {
			if(!skipslash)
				*out++ = '/';
			skipslash = false;
			memmove(out, prev, prevlen);
			out += prevlen;
		}

		/* Preserve the pathname component for the next iteration. */
		prev = begin;
		prevlen = end - begin;
	}

	/*
	 * If path does not contain a '/', then dirname() shall return a
	 * pointer to the string ".".
	 */
	if(out == path)
		*out++ = '.';
	*out = '\0';
	return (path);
}
