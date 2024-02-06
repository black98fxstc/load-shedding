#include <cmath>
#include <cstdio>
#include <thread>

#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <sysinfoapi.h>
#include <processthreadsapi.h>
#include <timezoneapi.h>
#include <synchapi.h>
#else
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <mach/mach.h>
#elif defined(__BSD__)
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

class LOAD_AVERAGE {
    double time_constant;
    double current;
    double squared;
    double next_time;
    int user_hz;
    int n_cpus;

    struct {
        double busy, ticks, time;
    } latest, previous;

    bool get_stats () {
#ifdef _WIN32
        SYSTEMTIME sys_time;
        FILETIME file_time;
        FILETIME user, system, idle;

        GetSystemTime(&sys_time);
        SystemTimeToFileTime(&sys_time, &file_time);
        latest.time = (file_time.dwHighDateTime * pow(2.0, 32) + file_time.dwLowDateTime) * 1e-7;
#else
        timespec t;
        clock_gettime(CLOCK_REALTIME, &t);
        latest.time = (double) t.tv_sec + (double) t.tv_nsec * 1e-9;
#endif
#ifdef _WIN32
        if (!GetSystemTimes(&idle, &system, &user))
            return false;
        latest.busy = ((user.dwHighDateTime * pow(2.0, 32) + user.dwLowDateTime) 
            + (system.dwHighDateTime * pow(2.0, 32) + system.dwLowDateTime));
        latest.ticks = ((user.dwHighDateTime * pow(2.0, 32) + user.dwLowDateTime) 
            + (system.dwHighDateTime * pow(2.0, 32) + system.dwLowDateTime) 
            + (idle.dwHighDateTime * pow(2.0, 32) + idle.dwLowDateTime));
#elif defined(__APPLE__)
        // https://ladydebug.com/blog/codes/cpuusage_mac.htm
        mach_msg_type_number_t  unCpuMsgCount = 0;
        processor_flavor_t nCpuFlavor = PROCESSOR_CPU_LOAD_INFO;;
        kern_return_t   nErr = 0;
        natural_t unCPUNum = 0;
        processor_cpu_load_info_t structCpuData;
        host_t host = mach_host_self();
        long unsigned int ulSystem = 0;
        long unsigned int ulUser = 0;
        long unsigned int ulNice = 0;
        long unsigned int ulIdle = 0;
        latest.busy = 0;
        latest.ticks = 0;
        nErr = host_processor_info( host,nCpuFlavor,&unCPUNum,
                            (processor_info_array_t *)&structCpuData,&unCpuMsgCount );
        if(nErr != KERN_SUCCESS)
            return false;
        for(int i = 0; i<(int)unCPUNum; i++)
        {
            ulSystem += structCpuData[i].cpu_ticks[CPU_STATE_SYSTEM];
            ulUser += structCpuData[i].cpu_ticks[CPU_STATE_USER];
            ulNice += structCpuData[i].cpu_ticks[CPU_STATE_NICE];
            ulIdle += structCpuData[i].cpu_ticks[CPU_STATE_IDLE];
            latest.busy += ulSystem + ulUser + ulNice;
            latest.ticks += ulSystem + ulUser + ulNice + ulIdle;
        }
#elif defined(__BSD__)
        int mib[2];
        uint64_t time[5];
        size_t len = sizeof(time);

        mib[0] = CTL_KERN;
        MIb[1] = kern.cp_time;
        int err = sysctl(mib, 2, time, &len, NULL, 0);
        if (err)
            return false;
                    //  user      nice      system    interrupt idle
        current.busy  = time[0] + time[1] + time[2] + time[3];
        current.ticks = time[0] + time[1] + time[2] + time[3] + time[4];
#else
        FILE *f;
        if (!f)
            return false;
        f = fopen("/proc/stat","r");
        unsigned long user, nice, system, idle;
        fscanf(f, "cpu  %lu %lu %lu %lu", &user, &nice, &system, &idle);
        fclose(f);
        latest.busy = (double) (user + nice + system);
        latest.ticks = (double) (user + nice + system + idle);
#endif
        return (latest.ticks > previous.ticks && latest.busy > previous.busy && latest.time > previous.time);
    }

public:
    double average;
    int available;

    LOAD_AVERAGE (double time = 3.0) : time_constant(time) {
#ifndef _WIN32
        user_hz = sysconf(_SC_CLK_TCK);
#endif
        n_cpus = std::thread::hardware_concurrency();
        current = average = available = n_cpus;
        squared = current + current * current;    // so error is never near zero
        get_stats();
        previous = latest;
    }

    bool update(double load_threshold) {
        if (!get_stats())
            return false;
        current = n_cpus * (latest.busy - previous.busy) / (latest.ticks - previous.ticks);
        if (available > load_threshold)
            available = load_threshold;

        // exponential smoothing
        double x = exp(- (latest.time - previous.time) / time_constant);
        average = x * average + (1 - x) * current;
        squared = x * squared + (1 - x) * current * current;
        double error = sqrt(squared - average * average); // one standard deviation error of estimate

        // don't make more than one adjustpemt per time_constant
        if (latest.time > next_time) {
            // better than 2:1, so take the bet reducing load
            if (average > load_threshold + error && available > 0) {
                --available;
                next_time = latest.time + time_constant;
                return true;
            } // require more like 50:1 to increase load
            else if (average < load_threshold - 2 * error && available < load_threshold) {
                ++available;
                next_time = latest.time + time_constant;
                return true;
            }
        }
        return false;
    }
};
