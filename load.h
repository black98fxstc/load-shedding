#include <cmath>
#include <cstdio>

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
#endif

class LoadAverage {
    double time_constant;
    double current;
    double squared;
    double next_time;
    int user_hz;

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

        if (!GetSystemTimes(&idle, &system, &user))
            return false;
        latest.busy = ((user.dwHighDateTime * pow(2.0, 32) + user.dwLowDateTime) 
            + (system.dwHighDateTime * pow(2.0, 32) + system.dwLowDateTime)) * 1e-7;
        latest.ticks = ((user.dwHighDateTime * pow(2.0, 32) + user.dwLowDateTime) 
            + (system.dwHighDateTime * pow(2.0, 32) + system.dwLowDateTime) 
            + (idle.dwHighDateTime * pow(2.0, 32) + idle.dwLowDateTime)) * 1e-7;
#elif defined(__APPLE__)
        timespec t;
        clock_gettime(CLOCK_REALTIME, &t);
        latest.time = (double) t.tv_sec + (double) t.tv_nsec * 1e-9;

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
        for(int i = 0; i<(int)unCPUNum; i++)
        {
                ulSystem += structCpuData[i].cpu_ticks[CPU_STATE_SYSTEM];
                ulUser += structCpuData[i].cpu_ticks[CPU_STATE_USER];
                ulNice += structCpuData[i].cpu_ticks[CPU_STATE_NICE];
                ulIdle += structCpuData[i].cpu_ticks[CPU_STATE_IDLE];
                latest.busy += ulSystem + ulUser + ulNice;
                latest.ticks += ulSystem + ulUser + ulNice + ulIdle;
        }
#else
        timespec t;
        clock_gettime(CLOCK_REALTIME, &t);
        latest.time = (double) t.tv_sec + (double) t.tv_nsec * 1e-9;

        FILE *f;
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

    LoadAverage (double time_constant = 3.0) : time_constant(time_constant) {
        current = 0;
        squared = 1;
        average = 0;
        available = 0;
#ifndef _WIN32
        user_hz = sysconf(_SC_CLK_TCK);
#endif
        get_stats();
        previous = latest;
    }

    void update(double load_threshold) {
        if (!get_stats())
            return;
        current = (latest.busy - previous.busy) / (latest.ticks - previous.ticks);

        // exponential smoothing
        double x = exp(- (latest.time - previous.time) / time_constant);
        average = x * average + (1 - x) * current;
        squared = x * squared + (1 - x) * current * current;
        double error = sqrt(squared - average * average);

        // don't make more than one adjustpemt per time.constant
        if (latest.time > next_time) {
            if (average > load_threshold + error && available > 0) {
                --available;
                next_time = latest.time + time_constant;
            }
            else if (average < load_threshold - 2 * error && available < load_threshold) {
                ++available;
                next_time = latest.time + time_constant;
            }
        }

        previous = latest;
    }
};