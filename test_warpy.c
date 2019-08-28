#include <stdio.h>
#include <stdint.h>

#include "warpy.h"

int main(int argc, char* argv[]) {
        struct warpy* warpy = start_warpy();
        update_sample_path(warpy, "kickroll.wav");
        update_speed(warpy, 0.5);
        update_gain(warpy, 0.5);
        update_center(warpy, 60);
        stop_warpy(warpy);

        return 0;
}
