#include "load.h"
#include <unistd.h>

int main () {
    LoadAverage load(10);

    while (true) {
        load.update(5.0);
        printf("Load Average: %f    Avaliable Load: %d\n", load.average, load.available);
        sleep(1);
    }
}