#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#define WIDTH 80
#define HEIGHT 25
#define FPS 30

const char* palette[256] = {
#define ANSIRGB(R,G,B) "\033[48;2;" #R ";"  #G ";" #B "m "
    ANSIRGB(  0,  0,   0), ANSIRGB(  0,   4,  4), ANSIRGB(  0,  16, 20), ANSIRGB(  0,  28,  36),
    ANSIRGB(  0,  32, 44), ANSIRGB(  0,  36, 48), ANSIRGB( 60,  24, 32), ANSIRGB(100,  16,  16),
    ANSIRGB(132,  12, 12), ANSIRGB(160,   8,  8), ANSIRGB(192,   8,  8), ANSIRGB(220,   4,   4),
    ANSIRGB(252,   0,  0), ANSIRGB(252,   0,  0), ANSIRGB(252,  12,  0), ANSIRGB(252,  28,   0),
    ANSIRGB(252,  40,  0), ANSIRGB(252,  52,  0), ANSIRGB(252,  64,  0), ANSIRGB(252,  80,   0),
    ANSIRGB(252,  92,  0), ANSIRGB(252, 104,  0), ANSIRGB(252, 116,  0), ANSIRGB(252, 132,   0),
    ANSIRGB(252, 144,  0), ANSIRGB(252, 156,  0), ANSIRGB(252, 156,  0), ANSIRGB(252, 160,   0),
    ANSIRGB(252, 160,  0), ANSIRGB(252, 164,  0), ANSIRGB(252, 168,  0), ANSIRGB(252, 168,   0),
    ANSIRGB(252, 172,  0), ANSIRGB(252, 176,  0), ANSIRGB(252, 176,  0), ANSIRGB(252, 180,   0),
    ANSIRGB(252, 180,  0), ANSIRGB(252, 184,  0), ANSIRGB(252, 188,  0), ANSIRGB(252, 188,   0),
    ANSIRGB(252, 192,  0), ANSIRGB(252, 196,  0), ANSIRGB(252, 196,  0), ANSIRGB(252, 200,   0),
    ANSIRGB(252, 204,  0), ANSIRGB(252, 204,  0), ANSIRGB(252, 208,  0), ANSIRGB(252, 212,   0),
    ANSIRGB(252, 212,  0), ANSIRGB(252, 216,  0), ANSIRGB(252, 220,  0), ANSIRGB(252, 220,   0),
    ANSIRGB(252, 224,  0), ANSIRGB(252, 228,  0), ANSIRGB(252, 228,  0), ANSIRGB(252, 232,   0),
    ANSIRGB(252, 232,  0), ANSIRGB(252, 236,  0), ANSIRGB(252, 240,  0), ANSIRGB(252, 240,   0),
    ANSIRGB(252, 244,  0), ANSIRGB(252, 248,  0), ANSIRGB(252, 248,  0), ANSIRGB(252, 252,   0),
#define W ANSIRGB(252,252,252)
    W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W,
    W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W,
    W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W,
    W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W,
    W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W,
    W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W,
    W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W,
    W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W
#undef W
#undef ANSIRGB
};

static uint8_t fire[WIDTH * HEIGHT];

void line_blur(int offset, int step, int nsteps) {
    uint8_t circ[3] = {0, fire[offset], fire[offset+step]};
    uint8_t beg = 1;
    for (int i=0; i<nsteps; i++) {
        fire[offset] = (circ[0]+circ[1]+circ[2])/3;
        circ[(beg+++2)%3] = i+2<nsteps ? fire[offset+2*step] : 0;
        offset += step;
    }
}

int main() {
    printf("\033[2J"); // clear screen
    for (;;) {
        printf("\033[H"); // home

        // box blur: first horizontal motion blur then vertical motion blur
        for (int j = 0; j<HEIGHT; j++)
            line_blur(j*WIDTH, 1, WIDTH);
        for (int i = 0; i<WIDTH; i++)
            line_blur(i, WIDTH, HEIGHT);

        for (int i = 0; i< WIDTH*HEIGHT; i++) // cool
            if (rand()%2 && fire[i]>0)
                fire[i]--;

        for (int i = 0; i<WIDTH; i++) {       // add heat to the bed
            int idx = i+(HEIGHT-1)*WIDTH;
            if (!(rand()%32))
                fire[idx] = 128+rand()%128;   // sparks
            else
                fire[idx] = fire[idx]<16 ? 16 : fire[idx]; // ember bed
        }

        for (int j = 0; j<HEIGHT; j++) {      // show the buffer
            for (int i = 0; i<WIDTH; i++)
                printf(palette[fire[i+j*WIDTH]]);
            printf("\033[49m\n");
        }

        for (int j = 1; j<HEIGHT; j++)        // scroll up
            for (int i = 0; i<WIDTH; i++)
                fire[i+(j-1)*WIDTH] = fire[i+j*WIDTH] ;
        usleep(1000000/FPS);
    }
    return 0;
}

