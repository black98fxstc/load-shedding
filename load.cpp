#include "load.h"

int main () {
    LoadAverage load;

    while (true) {
        load.update(5.0);
        printf("Load Average: %f    Avaliable Load: %d\n", load.average, load.available);
#ifdef _WIN32
        Sleep(1000);
#else
        sleep(1);
#endif
    }
}