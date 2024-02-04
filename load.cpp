#include "load.h"

int main () {
    LoadAverage load(10);

    while (true) {
        load.update(5.0);
        printf("Load Average: %f    Avaliable Load: %d\n", load.average, load.available);
#ifdef _WIN32
        Sleep(1000);
#elif
        sleep(1);
#endif
    }
}