#define GL_width  80
#define GL_height 50
#define GL_FPS 24
#include "GL_tty.h"
#include <math.h>
#include <time.h>

struct timespec start, cur;

void mainImage(int fragCoord_x, int fragCoord_y, float* fragColor_r, float* fragColor_g, float *fragColor_b) { // kinda shadertoy naming :)
    float u = (2.*fragCoord_x - GL_width )/GL_height;
    float v = (2.*fragCoord_y - GL_height)/GL_height;
    clock_gettime(CLOCK_MONOTONIC_RAW, &cur);
    float iTime =  (float)(cur.tv_sec - start.tv_sec) + (cur.tv_nsec - start.tv_nsec)*1e-9;
    float d1 = 1./sqrt(pow(sin(iTime*.5) -u, 2) + pow(sin(iTime*.5)-v, 2)); // linear motion
    float d2 = 1./sqrt(pow(sin(iTime*.5) -u, 2) + pow(cos(iTime*.5)-v, 2)); // circular motion
    float d3 = 1./sqrt(pow(sin(iTime*.25)-u, 2) + pow(sin(iTime)   -v, 2)); // wave
    float sdf = d1 + d2 + d3 - 2;       // metaballs signed distance function
    *fragColor_r = sdf;                 // orange halo (red and green channels)
    *fragColor_g = sdf*.5;
    *fragColor_b = sdf*.6-1<0 ? 0 : 1;  // cold-white metaballs (step function)
}

int main() {
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    GL_init();
    for (;;) {
        GL_home();
        GL_scan_RGBf(GL_width, GL_height, mainImage);
        GL_swapbuffers();
    }
    GL_terminate();
    return 0;
}
