#ifndef _MONITORING_H
#define _MONITORING_H

#include <sys/param.h>
#include <vector>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <asm/unistd.h>

#define CPU_STAT        "/proc/stat"
#define MEMORY_INFO     "/proc/meminfo"
#define UPTIME          "/proc/uptime"
#define TASK_STAT       "/proc/%u/task/%u/stat"
#define PID_STAT        "/proc/%u/stat"
#define PID_STATUS      "/proc/%u/status"
#define PID_COMM		"/proc/%u/comm"
#define PROC			"/proc"

#define PAGE_SIZE		sysconf(_SC_PAGESIZE)
#define MAX_COMM_LEN    128

/*
* Structure for memory and swap sapce utilization statistics.
*/
struct stats_memory {
	unsigned long long frmkb;
	unsigned long long bufkb;
	unsigned long long camkb;
	unsigned long long tlmkb;
	unsigned long long frskb;
	unsigned long long tlskb;
	unsigned long long caskb;
	unsigned long long comkb;
	unsigned long long activekb;
	unsigned long long inactkb;
	unsigned long long dirtykb;
	unsigned long long anonpgkb;
	unsigned long long slabkb;
	unsigned long long kstackkb;
	unsigned long long pgtblkb;
	unsigned long long vmusedkb;
	unsigned long long availablekb;
};

struct pid_stats {
	unsigned long long read_bytes			__attribute__ ((aligned (8)));
	unsigned long long write_bytes			__attribute__ ((packed));
	unsigned long long cancelled_write_bytes	__attribute__ ((packed));
	unsigned long long blkio_swapin_delays		__attribute__ ((packed));
	unsigned long long minflt			__attribute__ ((packed));
	unsigned long long cminflt			__attribute__ ((packed));
	unsigned long long majflt			__attribute__ ((packed));
	unsigned long long cmajflt			__attribute__ ((packed));
	unsigned long long utime			__attribute__ ((packed));
	long long          cutime			__attribute__ ((packed));
	unsigned long long stime			__attribute__ ((packed));
	long long          cstime			__attribute__ ((packed));
	unsigned long long gtime			__attribute__ ((packed));
	long long          cgtime			__attribute__ ((packed));
	unsigned long long wtime			__attribute__ ((packed));
	unsigned long long vsz				__attribute__ ((packed));
	unsigned long long rss				__attribute__ ((packed));
	unsigned long      nvcsw			__attribute__ ((packed));
	unsigned long      nivcsw			__attribute__ ((packed));
	unsigned long      stack_size			__attribute__ ((packed));
	unsigned long      stack_ref			__attribute__ ((packed));
	unsigned int       processor			__attribute__ ((packed));
	unsigned int       priority			__attribute__ ((packed));
	unsigned int       policy			__attribute__ ((packed));
	unsigned int       threads			__attribute__ ((packed));
	unsigned int       fd_nr			__attribute__ ((packed));
};

struct st_pid {
	unsigned long long total_vsz;
	unsigned long long total_rss;
	unsigned long long total_stack_size;
	unsigned long long total_stack_ref;
	unsigned long long total_threads;
	unsigned long long total_fd_nr;
	pid_t		   pid;
	uid_t		   uid;
	int		   exist;	/* TRUE if PID exists */
	unsigned int	   flags;
	unsigned int	   rt_asum_count;
	unsigned int	   rc_asum_count;
	unsigned int	   uc_asum_count;
	unsigned int	   tf_asum_count;
	unsigned int	   sk_asum_count;
	unsigned int	   delay_asum_count;
	struct pid_stats  *pstats[3];
	struct st_pid	  *tgid;	/* If current task is a TID, pointer to its TGID. NULL otherwise. */
	struct st_pid	  *next;
};


/*
 * Structure for CPU statistics.
 * In activity buffer: First structure is for global CPU utilization ("all").
 * Following structures are for each individual CPU (0, 1, etc.)
 *
 */
struct cpu_stats {
	unsigned long long cpu_user;
	unsigned long long cpu_nice;
	unsigned long long cpu_sys;
	unsigned long long cpu_idle;
	unsigned long long cpu_iowait;
	unsigned long long cpu_steal;
	unsigned long long cpu_hardirq;
	unsigned long long cpu_softirq;
	unsigned long long cpu_guest;
	unsigned long long cpu_guest_nice;
};

// memory stat
int read_meminfo();
pid_stats read_proc_memory_stat(pid_t pid);
float write_memory_stat();

// cpu stat
cpu_stats read_cpu_stats();
std::vector<cpu_stats> read_node_stats();
float write_cpu_stat(cpu_stats curr, cpu_stats prev);
std::vector<float> write_node_stat(std::vector<cpu_stats> curr, std::vector<cpu_stats> prev);

// montiroing tools
void monitoring_loop(bool CPU_MODE, bool MEMORY_MODE, int delay);
void *background_monitoring(void *delay);

#endif