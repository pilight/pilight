#ifdef _WIN32
#define pid_t int
#endif

unsigned long psutil_max_pid(void);
int psutil_proc_exe(const pid_t pid, char **name);
int psutil_proc_name(const pid_t pid, char **name, size_t bufsize);
int psutil_cpu_count_phys(void);
size_t psutil_get_cmd_args(pid_t pid, char **args, size_t size);
