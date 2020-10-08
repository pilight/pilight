/*
    Memu Library

    Memu is a C/C++ single header file library that provides a cross-platform way
    of determining memory usage of your program.

    Memu is an adaptation of a .c file written by David Robert Nadeau, which you
    can find here:

        http://nadeausoftware.com/articles/2012/07/c_c_tip_how_get_process_resident_set_size_physical_memory_use

    The modifier of this file is in no way affiliated with the original author.

    You MUST define MEMU_IMPLEMENTATION before using this file like this:

        #define MEMU_IMPLEMENTATION
        #include "memu.h"

    You can also define MEMU_STATIC to define all functions as static, isolating
    the implementation.

    If you are on Windows, then you must link with psapi.lib, with other OS's there
    is no need to link with anything.

    This library is supported on the following platforms:

        Microsoft Windows
        Mac OS X
        GNU/Linux
        BSD
        AIX
        Solaris

    There are only two functions in this library:

        size_t memu_get_peak_rss()  Gets the peak physical memory usage in bytes
        size_t memu_get_curr_rss()  Gets the current physical memory usage in bytes

    Both functions will return 0 if the memory usage could not be determined.

    Info:

        Original Author: David Robert Nadeau
        Original Site:   http://NadeauSoftware.com/

        Modified by: Kabsly

        License: Creative Commons Attribution 3.0 Unported License
        http://creativecommons.org/licenses/by/3.0/deed.en_US
*/

#ifndef MEMU_HEADER
#define MEMU_HEADER

#ifdef MEMU_IMPLEMENTATION
#   if defined(_WIN32)
#       pragma comment(lib, "psapi.lib")
#       include <windows.h>
#       include <psapi.h>
#   elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
#       include <unistd.h>
#       include <sys/resource.h>
#   if defined(__APPLE__) && defined(__MACH__)
#       include <mach/mach.h>
#   elif (defined(_AIX) || defined(__TOS__AIX__)) || (defined(__sun__) || defined(__sun) || defined(sun) && (defined(__SVR4) || defined(__svr4__)))
#       include <fcntl.h>
#       include <procfs.h>
#   elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
#       include <stdio.h>
#   endif
#   else
#       error "memu error: Unsupported operating system"
#   endif
#endif /* MEMU_IMPLEMENTATION */

#ifdef MEMU_STATIC
#   define MEMU_PREFIX static
#else
#   define MEMU_PREFIX
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Prototypes */
MEMU_PREFIX size_t memu_get_peak_rss();
MEMU_PREFIX size_t memu_get_curr_rss();

#ifdef MEMU_IMPLEMENTATION

MEMU_PREFIX size_t memu_get_peak_rss()
{
#if defined(_WIN32)
	PROCESS_MEMORY_COUNTERS info;
	GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
	return (size_t)info.PeakWorkingSetSize;

#elif (defined(_AIX) || defined(__TOS__AIX__)) || (defined(__sun__) || defined(__sun) || defined(sun) && (defined(__SVR4) || defined(__svr4__)))
	struct psinfo psinfo;
	int fd = -1;
	if ((fd = open("/proc/self/psinfo", O_RDONLY)) == -1)
		return (size_t)0L;
	if (read(fd, &psinfo, sizeof(psinfo)) != sizeof(psinfo))
	{
		close(fd);
		return (size_t)0L;
	}
	close(fd);
	return (size_t)(psinfo.pr_rssize * 1024L);

#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
	struct rusage rusage;
	getrusage( RUSAGE_SELF, &rusage );
#if defined(__APPLE__) && defined(__MACH__)
	return (size_t)rusage.ru_maxrss;
#else
	return (size_t)(rusage.ru_maxrss * 1024L);
#endif

#else
	return (size_t)0L;
#endif
}

MEMU_PREFIX size_t memu_get_curr_rss()
{
#if defined(_WIN32)
	PROCESS_MEMORY_COUNTERS info;
	GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
	return (size_t)info.WorkingSetSize;

#elif defined(__APPLE__) && defined(__MACH__)
	struct mach_task_basic_info info;
	mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
		(task_info_t)&info, &infoCount) != KERN_SUCCESS)
		return (size_t)0L;
	return (size_t)info.resident_size;

#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
	long rss = 0L;
	FILE* fp = NULL;

	if ((fp = fopen("/proc/self/statm", "r")) == NULL)
		return (size_t)0L;
	if (fscanf(fp, "%*s%ld", &rss) != 1) {
		fclose(fp);
		return (size_t)0L;
	}

	fclose(fp);
	return (size_t)rss * (size_t)sysconf(_SC_PAGESIZE);

#else
	return (size_t)0L;
#endif
}

#endif /* MEMU_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif /* MEMU_HEADER */

