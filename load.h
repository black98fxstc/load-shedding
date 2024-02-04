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

class LoadAverage {
    double time_constant;
    double current;
    double squared;
    double current_cpu;
    double current_time;
    double last_cpu;
    double last_time;
    double next_time;
    int user_hz;

    void get_stats () {
#ifdef _WIN32
        SYSTEMTIME sys_time;
        FILETIME file_time;
        GetSystemTime(&sys_time);
        SystemTimeToFileTime(&sys_time, &file_time);
        current_time = (file_time.dwHighDateTime * pow(2.0, 32) + file_time.dwLowDateTime) * 1e-7;
        GetSystemTimes(NULL, NULL, &file_time);
        current_cpu = (file_time.dwHighDateTime * pow(2.0, 32) + file_time.dwLowDateTime) * 1e-7;
#elif defined(__APPLE__)
#else
        timespec t;
        clock_gettime(CLOCK_REALTIME, &t);
        current_time = (double) t.tv_sec + (double) t.tv_nsec * 1e-9;

        FILE *f;
        char buffer[100];
        f = fopen("/proc/stat","r");
        unsigned long jiffies;
        fscanf(f, "cpu  %lu", &jiffies);
        fclose(f);
        current_cpu = (double) jiffies / (double) user_hz;
#endif
    }

public:
    double average;
    int available;

    LoadAverage (double time_constant = 1.0) : time_constant(time_constant) {
        current = 0;
        squared = 1;
        average = 0;
        available = 0;
#ifndef _WIN32
        user_hz = sysconf(_SC_CLK_TCK);
#endif
        get_stats();
        last_cpu = current_cpu;
        last_time = current_time;
    }

    void update(double load_threshold)
    {
        get_stats();
        if (!(current_time > last_time) || !(current_cpu > last_cpu))
            return;

        // exponential smoothing
        current = (current_cpu - last_cpu) / (current_time - last_time);
        double x = exp(- (current_time - last_time) / time_constant);
        average = x * average + (1 - x) * current;
        squared = x * squared + (1 - x) * current * current;
        double error = sqrt(squared - average * average);

        // don't make more than one adjustpemt per time_constant
        if (current_time > next_time) {
            if (average > load_threshold + error && available > 0) {
                --available;
                next_time = current_time + time_constant;
            }
            else if (average < load_threshold - 2 * error && available < load_threshold) {
                ++available;
                next_time = current_time + time_constant;
            }
        }

        last_cpu = current_cpu;
        last_time = current_time;
    }
};