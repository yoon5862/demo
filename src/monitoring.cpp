#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/wait.h>
#include <regex.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <ctime>
#include <cerrno>

#include "monitoring.hpp"
#include "util.hpp"

using namespace std;

// aarch64
#if (__aarch64__ || __arm__)
	constexpr uint64_t L2_MISS_EVENT_CODE = 0x17;
	constexpr uint64_t L2_ACCESS_EVENT_CODE = 0x16;
#endif

unsigned long long memory_tlmkb;
unsigned int kb_shift;
float util_cpu_, util_memory_, llc_miss_rate_;

/*
 * Read memory statistics from /proc/meminfo.
 * RETURN:
 * @memory_tlmkb	Total memory capcity using KB.
*/
int read_meminfo()
{
    FILE *fp;
    char line[128];

    if ((fp = fopen(MEMORY_INFO, "r")) == NULL)
        return 0;
    
    while (fgets(line, sizeof(line), fp) != NULL)
    {
		if (!strncmp(line, "MemTotal:", 9))
        {
			/* Read the total amount of memory in kB */
			sscanf(line + 9, "%llu", &memory_tlmkb);
		}
    }
    fclose(fp);
    return memory_tlmkb;
}

/*
 ***************************************************************************
 * Read stats from /proc/#[/task/##]/stat.
 *
 * IN:
 * @pid		Process whose stats are to be read.
 *
 * RETURNS:
 * @pid_stat	various stats of process when successed.
 * NULL when failed.
 ***************************************************************************
 */
pid_stats read_proc_memory_stat(pid_t pid)
{
	int fd, sz, rc, commsz, thread_nr;
    char filename[128];
    static char buffer[1024 + 1];
	char *start, *end;
    char comm[128];
	struct pid_stats pst;

    sprintf(filename, PID_STAT, pid);

    if ((fd = open(filename, 00)) == NULL)
        return pid_stats{NULL};

	sz = read(fd, buffer, 1024);
	close(fd);
	if (sz <= 0)
		return pid_stats{NULL};
	buffer[sz] = '\0';

	if ((start = strchr(buffer, '(')) == NULL)
		return pid_stats{NULL};
	start += 1;
	if ((end = strrchr(start, ')')) == NULL)
		return pid_stats{NULL};
	commsz = end - start;
	if (commsz >= MAX_COMM_LEN)
		return pid_stats{NULL};
	memcpy(comm, start, commsz);
	comm[commsz] = '\0';
	start = end + 2;

	rc = sscanf(start,
		    "%*s %*d %*d %*d %*d %*d %*u %llu %llu"
		    " %llu %llu %llu %llu %lld %lld %*d %*d %u %*u %*d %llu %llu"
		    " %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u"
		    " %*u %u %u %u %llu %llu %lld\n",
		    &pst.minflt, &pst.cminflt, &pst.majflt, &pst.cmajflt,
		    &pst.utime,  &pst.stime, &pst.cutime, &pst.cstime,
		    &thread_nr, &pst.vsz, &pst.rss, &pst.processor,
		    &pst.priority, &pst.policy,
		    &pst.blkio_swapin_delays, &pst.gtime, &pst.cgtime);

	if (rc < 15)
		return pid_stats{NULL};

	if (rc < 17) {
		/* gtime and cgtime fields are unavailable in file */
		pst.gtime = pst.cgtime = 0;
	}

	/* Convert to kB */
	pst.vsz >>= 10;
	pst.rss = pst.rss << kb_shift;

	return pst;
}


/*
 ***************************************************************************
 * Write memory stat.
 *
 * RETURNS:
 * @memory_util	Memory utilization at now.
 ***************************************************************************
*/
float write_memory_stat()
{
	DIR *dir;
	struct dirent *drp;
	pid_t pid;
	struct st_pid *plist;
	struct pid_stats pid_list;
	float memory_tlmkb, memory_usedkb = 0, memory_util;

	if ((dir = opendir(PROC)) == NULL)
	{
		perror("opendir");
		exit(4);
	}

	kb_shift = get_kb_shift();
	memory_tlmkb = read_meminfo();
	while ((drp = readdir(dir)) != NULL)
	{
		if (!isdigit(drp->d_name[0]))
			continue;

		pid = atoi(drp->d_name);
		pid_list = read_proc_memory_stat(pid);
		if (pid_list.rss == 0)
			continue;

		memory_usedkb += pid_list.rss;
	}

	memory_util = float(memory_usedkb / memory_tlmkb) * 100;
	closedir(dir);

	return memory_util;
}


/*
 ***************************************************************************
 * Write process' memory stat.
 *
 * RETURNS:
 * @memory_util	Memory utilization at now.
 ***************************************************************************
*/
void write_process_memory_stat()
{
	DIR *dir;
	struct dirent *drp;
	pid_t pid;
	struct st_pid *plist;
	struct pid_stats pid_list;
	float memory_tlmkb, memory_usedkb = 0, memory_util;

	if ((dir = opendir(PROC)) == NULL)
	{
		perror("opendir");
		exit(4);
	}

	kb_shift = get_kb_shift();
	memory_tlmkb = read_meminfo();
	while ((drp = readdir(dir)) != NULL)
	{
		if (!isdigit(drp->d_name[0]))
			continue;

		pid = atoi(drp->d_name);
		pid_list = read_proc_memory_stat(pid);
		if (pid_list.rss == 0)
			continue;

    	char filename[128], pid_comm[128], line[128];
		FILE * fp;
		sprintf(filename, PID_COMM, pid);
		fp = fopen(filename, "r");
		fgets(line, sizeof(line), fp);
		sscanf(line, "%s", pid_comm);

		cout << pid_comm << ": " << float(pid_list.rss / memory_tlmkb) * 100 << "; ";
		fclose(fp);
	}

	closedir(dir);
}

/*
 ***************************************************************************
 * Open performance count event
 * 
 * IN:
 * @type		type of performance counter (e.g., cache, memory)
 * @config		configuration of performance counter (e.g., hit, miss, access)
 * 
 * RETURN:
 * @fd			system call of performance event
 ***************************************************************************
*/
int open_perf_event(perf_type_id type, uint64_t config) {
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = type;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = config;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    int fd = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
    if (fd == -1) {
        std::cerr << "Error opening perf event: " << strerror(errno) << std::endl;
    }
    return fd;
}


/*
 ***************************************************************************
 * Read LLC access/miss count
 * 
 * IN:
 * @llc_miss_fd			syscall of performance event for llc miss count
 * @llc_access_fd		syscall of performance event for llc access count
 ***************************************************************************
*/
void start_llc_stat(int* llc_miss_fd, int* llc_access_fd)
{
    // Initialize performance counters and start read them
    ioctl(*llc_miss_fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(*llc_access_fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(*llc_miss_fd, PERF_EVENT_IOC_ENABLE, 0);
    ioctl(*llc_access_fd, PERF_EVENT_IOC_ENABLE, 0);
}

/*
 ***************************************************************************
 * Stop reading LLC access/miss count and Calculate miss ratio
 * 
 * IN:
 * @llc_miss_fd			syscall of performance event for llc miss count
 * @llc_access_fd		syscall of performance event for llc access count
 * 
 * RETURN
 * @llc_miss_rate		LLC miss rate
 ***************************************************************************
*/
float stop_llc_stat(int* llc_miss_fd, int* llc_access_fd)
{
	ioctl(*llc_miss_fd, PERF_EVENT_IOC_DISABLE, 0);
	ioctl(*llc_access_fd, PERF_EVENT_IOC_DISABLE, 0);

    long long llc_misses, llc_accesses;
    read(*llc_miss_fd, &llc_misses, sizeof(long long));
    read(*llc_access_fd, &llc_accesses, sizeof(long long));

    // LLC 미스 비율 계산
    double llc_miss_rate = llc_accesses > 0 ? (static_cast<double>(llc_misses) / llc_accesses) * 100.0 : 0.0;

	return llc_miss_rate;
}

/*
 ***************************************************************************
 * Compute global CPU statistics as the sum of individual CPU ones,
 * RETURNS:
 * @cpu_util	Various statictis of CPU.
 ***************************************************************************
 */
cpu_stats read_cpu_stats()
{
	FILE *fp;
	struct cpu_stats cpu_util, cpu_util_;
	char cpu_idx[10];
    char line[128];
	char* search = "cpu";
	std::vector<cpu_stats> node_util;

    if ((fp = fopen(CPU_STAT, "r")) == NULL)
        return cpu_stats{NULL};

	while (fgets(line, sizeof(line), fp) != NULL)
	{
		sscanf(line, "%s %llu %llu %llu %llu",
		&cpu_idx, &cpu_util_.cpu_user, &cpu_util_.cpu_nice, &cpu_util_.cpu_sys, &cpu_util_.cpu_idle);
		// cout << cpu_idx << "; " << cpu_util_.cpu_user << " ;" << cpu_util_.cpu_nice << " ;" << cpu_util_.cpu_sys << " ;" << cpu_util_.cpu_idle << endl;
		if (cpu_idx[3] != NULL && strstr(cpu_idx, search) != NULL)
			node_util.push_back(cpu_util_);
		else if (strstr(cpu_idx, search) != NULL)
		{
			cpu_util = cpu_util_;
			break;
		}
	}
	
	fclose(fp);

	return cpu_util;
}


/*
 ***************************************************************************
 * Compute individual CPU nodes' statistics, not global CPU statistics.,
 * RETURNS:
 * @node_util	Vector of 'cpu_stats' including various statictis of CPU ones.
 ***************************************************************************
 */
std::vector<cpu_stats> read_node_stats()
{
	FILE *fp;
	struct cpu_stats cpu_util_;
	char cpu_idx[10];
    char line[128];
	char* search = "cpu";
	std::vector<cpu_stats> node_util;

    if ((fp = fopen(CPU_STAT, "r")) == NULL)
        return vector<cpu_stats>{NULL};

	while (fgets(line, sizeof(line), fp) != NULL)
	{
		sscanf(line, "%s %llu %llu %llu %llu",
		&cpu_idx, &cpu_util_.cpu_user, &cpu_util_.cpu_nice, &cpu_util_.cpu_sys, &cpu_util_.cpu_idle);
		// cout << cpu_idx << "; " << cpu_util_.cpu_user << " ;" << cpu_util_.cpu_nice << " ;" << cpu_util_.cpu_sys << " ;" << cpu_util_.cpu_idle << endl;
		if (cpu_idx[3] != NULL && strstr(cpu_idx, search) != NULL)
			node_util.push_back(cpu_util_);
		else if (strstr(cpu_idx, search) == NULL)
			break;
	}

	fclose(fp);

	return node_util;
}

/*
 ***************************************************************************
 * Write global CPU utilization.
 *
 * IN:
 * @curr		current CPU statistics
 * @prev		previous CPU statistics
 * RETURNS:
 * @cpu_util	cpu utilization at now.
 ***************************************************************************
*/
float write_cpu_stat(cpu_stats curr, cpu_stats prev)
{
	cpu_stats diff;
	unsigned long long tot_diff;
	float cpu_util;

	diff.cpu_user = curr.cpu_user - prev.cpu_user;
	diff.cpu_nice = curr.cpu_nice - prev.cpu_nice;
	diff.cpu_sys = curr.cpu_sys - prev.cpu_sys;
	diff.cpu_idle = curr.cpu_idle - prev.cpu_idle;
	tot_diff = diff.cpu_user + diff.cpu_nice + diff.cpu_sys + diff.cpu_idle;

	if (tot_diff == 0)
		return 0;
	cpu_util = 100 * (1.0 - double(diff.cpu_idle) / double(tot_diff));
	return cpu_util;
}

/*
 ***************************************************************************
 * Write CPU nodes' utilization.
 *
 * IN:
 * @curr		current CPU nodes statistics
 * @prev		previous CPU nodes statistics
 * RETURNS:
 * @node_util	vector; node utilization at now.
 ***************************************************************************
*/
vector<float> write_node_stat(vector<cpu_stats> curr, vector<cpu_stats> prev)
{
	cpu_stats diff;
	unsigned long long tot_diff;
	vector<float> node_util;

	for (int idx = 0; idx < curr.size(); idx++)
	{
		diff.cpu_user = curr[idx].cpu_user - prev[idx].cpu_user;
		diff.cpu_nice = curr[idx].cpu_nice - prev[idx].cpu_nice;
		diff.cpu_sys = curr[idx].cpu_sys - prev[idx].cpu_sys;
		diff.cpu_idle = curr[idx].cpu_idle - prev[idx].cpu_idle;
		tot_diff = diff.cpu_user + diff.cpu_nice + diff.cpu_sys + diff.cpu_idle;

		if (tot_diff == 0)
		{
			node_util.push_back(0);
			continue;
		}

		float tmp_util = 100 * (1.0 - double(diff.cpu_idle) / double(tot_diff));
		node_util.push_back(tmp_util);
	}

	return node_util;
}

void monitoring_loop(bool CPU_MODE, bool MEMORY_MODE, int delay)
{
	// cpu_stats cpu_curr, cpu_prev;
	vector<cpu_stats> node_curr, node_prev;
	float cpu_util;
	vector<float> node_util;
	float memory_util;
	float llc_miss_rate;
	int llc_miss_fd, llc_access_fd;

	node_curr = read_node_stats();
	while(true)
	{
		// CPU utilization monitoring
		// cpu_prev = cpu_curr;
		// cpu_curr = read_cpu_stats();
		// cpu_util = write_cpu_stat(cpu_curr, cpu_prev);
		
		node_prev = node_curr;
		node_curr = read_node_stats();
		node_util = write_node_stat(node_curr, node_prev);
		float tmp_avg = 0;
		for (int tmp = 0; tmp < node_util.size(); tmp++)
			tmp_avg += node_util[tmp];
		cpu_util = tmp_avg / node_util.size();

		// Memory utilization monitoring
		if (MEMORY_MODE)
		{
			memory_util = write_memory_stat();
		}

		// LLC miss ratio monitoring
		// Set LLC miss event
		#if (__aarch64__ || __arm__)
			llc_miss_fd = open_perf_event(PERF_TYPE_RAW, L2_MISS_EVENT_CODE);
		#else
			llc_miss_fd = open_perf_event(PERF_TYPE_HW_CACHE, 
			PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));
		#endif
		if (llc_miss_fd == -1){
			close(llc_miss_fd);
			exit(1);
		}

		// Set LLC access ratio
		#if (__aarch64__ || __arm__)
			llc_access_fd = open_perf_event(PERF_TYPE_RAW, L2_ACCESS_EVENT_CODE);
		#else
			llc_access_fd = open_perf_event(PERF_TYPE_HW_CACHE, 
			PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16));
		#endif
		if (llc_access_fd == -1) {
			close(llc_access_fd);
			exit(1);
		}
		start_llc_stat(&llc_miss_fd, &llc_access_fd);

		sleep(delay);
		llc_miss_rate = stop_llc_stat(&llc_miss_fd, &llc_access_fd);
		close(llc_miss_fd);
		close(llc_access_fd);

		// Print out
		auto time = chrono::system_clock::now();
		std::time_t end_time = std::chrono::system_clock::to_time_t(time);
		cout << "System time: " << ctime(&end_time);

		cout << fixed;
		cout.precision(2);

		// if (CPU_MODE && MEMORY_MODE)
		// 	cout << "CPU util (%): " << cpu_util << "; Memory util (%):" << memory_util;
		// else if (!CPU_MODE && MEMORY_MODE)
		// {
		// 	cout << "CPU util (%): ";
		// 	for (int tmp = 0; tmp < node_util.size(); tmp++)
		// 		cout << node_util[tmp] << "; ";
		// 	cout << "Memory util (%):" << memory_util;
		// }
    	// cout << "; LLC Miss Rate (%): " << llc_miss_rate << "%" << endl;

		if (CPU_MODE)
			cout << "CPU util (%): " << cpu_util;
		else {
			cout << "CPU util (%): ";
			for (int tmp = 0; tmp < node_util.size(); tmp++)
				cout << node_util[tmp] << "; ";
		}
		if (MEMORY_MODE)
			cout << "; Memory util (%): " << memory_util;
		else {
			cout << "; Memory util per process (%):";
			write_process_memory_stat();
		}
    	cout << "; LLC Miss Rate (%): " << llc_miss_rate << "%" << endl;
	}
}

void *background_monitoring(void *delay)
{
	cpu_stats cpu_curr, cpu_prev;
	float delay_ = *((float*)delay);
	int llc_miss_fd, llc_access_fd;

	// node_curr = read_node_stats();
	cpu_curr = read_cpu_stats();
	while(true)
	{
		// CPU utilization monitoring
		cpu_prev = cpu_curr;
		cpu_curr = read_cpu_stats();
		util_cpu_ = write_cpu_stat(cpu_curr, cpu_prev);

		// memory utilization monitoring
		util_memory_ = write_memory_stat();

		//// LLC miss ratio monitoring
		// Set LLC miss event
		#if (__aarch64__ || __arm__)
			llc_miss_fd = open_perf_event(PERF_TYPE_RAW, L2_MISS_EVENT_CODE);
		#else
			llc_miss_fd = open_perf_event(PERF_TYPE_HW_CACHE, 
			PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));
		#endif
		if (llc_miss_fd == -1) {
			close(llc_miss_fd);
			exit(1) ;
		}

		// Set LLC access ratio
		#if (__aarch64__ || __arm__)
			llc_access_fd = open_perf_event(PERF_TYPE_RAW, L2_ACCESS_EVENT_CODE);
		#else
			llc_access_fd = open_perf_event(PERF_TYPE_HW_CACHE, 
			PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16));
		#endif

		if (llc_access_fd == -1) {
			close(llc_access_fd);
			exit(1);
		}
		start_llc_stat(&llc_miss_fd, &llc_access_fd);

		// delay
		sleep(delay_);
		llc_miss_rate_ = stop_llc_stat(&llc_miss_fd, &llc_access_fd);
	}
}
