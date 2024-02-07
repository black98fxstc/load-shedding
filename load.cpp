#include "load.h"

int main () {
    LOAD_AVERAGE load;

    while (true) {
        load.update(5.0);
        printf("Load Average: %f   Avaliable Load: %d   Temperature: %f\n", load.average, load.available, load.temperature);
#ifdef _WIN32
        Sleep(1000);
#else
        sleep(1);
#endif
    }
}