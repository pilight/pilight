#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#include <windows.h>
#include <winioctl.h>
#include <psapi.h>
#include <winternl.h>

#include "psutil.h"

#define PSUTIL_FIRST_PROCESS(Processes) ((PSYSTEM_PROCESS_INFORMATION)(Processes))
#define PSUTIL_NEXT_PROCESS(Process) ( \
   ((PSYSTEM_PROCESS_INFORMATION)(Process))->NextEntryOffset ? \
   (PSYSTEM_PROCESS_INFORMATION)((PCHAR)(Process) + \
        ((PSYSTEM_PROCESS_INFORMATION)(Process))->NextEntryOffset) : NULL)

unsigned long psutil_max_pid(void) {
	return 99999;
}

/*
 * A wrapper around OpenProcess setting NSP exception if process
 * no longer exists.
 * "pid" is the process pid, "dwDesiredAccess" is the first argument
 * exptected by OpenProcess.
 * Return a process handle or NULL.
 */
static HANDLE psutil_handle_from_pid_waccess(DWORD pid, DWORD dwDesiredAccess) {
	HANDLE hProcess;
	DWORD processExitCode = 0;

	if(pid == 0) {
		// otherwise we'd get NoSuchProcess
		// return AccessDenied();
		return NULL;
	}

	hProcess = OpenProcess(dwDesiredAccess, FALSE, pid);
	if(hProcess == NULL) {
		if(GetLastError() == ERROR_INVALID_PARAMETER) {
			// NoSuchProcess();
			return NULL;
		} else {
			// PyErr_SetFromWindowsErr(0);
			return NULL;
		}
		return NULL;
	}

	/* make sure the process is running */
	GetExitCodeProcess(hProcess, &processExitCode);
	if(processExitCode == 0) {
		// NoSuchProcess();
		CloseHandle(hProcess);
		return NULL;
	}

	return hProcess;
}

int psutil_proc_exe(const pid_t pid, char **name) {
	if((pid % 4) != 0) {
		return -1;
	}

	HANDLE hProcess;
	char exe[MAX_PATH];
	LPSTR p = exe;
	DWORD nSize = MAX_PATH;

	hProcess = psutil_handle_from_pid_waccess(pid, PROCESS_QUERY_INFORMATION);
	if(hProcess == NULL) {
		return -1;
	}

	if(GetProcessImageFileName(hProcess, p, nSize) == 0) {
		CloseHandle(hProcess);
		if(GetLastError() == ERROR_INVALID_PARAMETER) {
			// Access denied
			return -1;
		} else {
			// PyErr_SetFromWindowsErr(0);
			return -1;
		}
		return -1;
	}
	strcpy(*name, p);

	CloseHandle(hProcess);
	return 0;
}

int psutil_proc_name(const pid_t pid, char **name, size_t bufsize) {
	if((pid % 4) != 0) {
		return -1;
	}

	DWORD aiPID[1000], iCb = 1000;
	DWORD iCbneeded = 0;
	int iNumProc = 0, i = 0;
	char szName[MAX_PATH];
	int iLenP = 0;
	HANDLE hProc;
	HMODULE hMod;

	hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	if(hProc) {
		if(EnumProcessModules(hProc, &hMod, sizeof(hMod), &iCbneeded)) {
			GetModuleBaseName(hProc, hMod, szName, MAX_PATH);
			strcpy(*name, szName);
			CloseHandle(hProc);
			return 0;
		}
		CloseHandle(hProc);
	}

	return -1;
}

typedef NTSTATUS (NTAPI *_NtQueryInformationProcess)(
	HANDLE ProcessHandle,
	DWORD ProcessInformationClass,
	PVOID ProcessInformation,
	DWORD ProcessInformationLength,
	PDWORD ReturnLength
);

PVOID GetPebAddress(HANDLE ProcessHandle) {
	_NtQueryInformationProcess NtQueryInformationProcess =
		(_NtQueryInformationProcess)GetProcAddress(
			GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess"
		);

	PROCESS_BASIC_INFORMATION pbi;

	NtQueryInformationProcess(ProcessHandle, 0, &pbi, sizeof(pbi), NULL);

	return pbi.PebBaseAddress;
}

size_t psutil_get_cmd_args(pid_t pid, char **args, size_t size) {
	if((pid % 4) != 0) {
		return 0;
	}

	HANDLE processHandle;
	PVOID pebAddress;
	PVOID rtlUserProcParamsAddress;
	UNICODE_STRING commandLine;
	WCHAR *commandLineContents;

	memset(*args, 0, size);

	if((processHandle = OpenProcess(
		PROCESS_QUERY_INFORMATION | /* required for NtQueryInformationProcess */
		PROCESS_VM_READ, /* required for ReadProcessMemory */
		FALSE, pid)) == 0) {
		// printf("Could not open process!\n");
		return 0;
	}

	pebAddress = GetPebAddress(processHandle);

	/* get the address of ProcessParameters */
	if(!ReadProcessMemory(processHandle,
				&(((struct _PEB *)pebAddress)->ProcessParameters),
				&rtlUserProcParamsAddress,
				sizeof(PVOID), NULL)) {
		// printf("Could not read the address of ProcessParameters!\n");
		return 0;
	}

	/* read the CommandLine UNICODE_STRING structure */
	if(!ReadProcessMemory(processHandle,
			&(((struct _RTL_USER_PROCESS_PARAMETERS *)rtlUserProcParamsAddress)->CommandLine),
			&commandLine, sizeof(commandLine), NULL)) {
		// printf("Could not read CommandLine!\n");
		return 0;
	}

	/* allocate memory to hold the command line */
	commandLineContents = (WCHAR *)malloc(commandLine.Length);

	/* read the command line */
	if(!ReadProcessMemory(processHandle, commandLine.Buffer, commandLineContents, commandLine.Length, NULL)) {
		// printf("Could not read the command line string!\n");
		return 0;
	}

	/* the length specifier is in characters, but commandLine.Length is in bytes */
	/* a WCHAR is 2 bytes */
	int i = 0;
	for(i=0;i<commandLine.Length/2;i++) {
		if(i > size) {
			break;
		}
		(*args)[i] = commandLineContents[i];
	}

	CloseHandle(processHandle);
	free(commandLineContents);

	return i;
}

int psutil_cpu_count_phys(void) {
#ifdef _WIN32
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
#endif

	return -1;
}