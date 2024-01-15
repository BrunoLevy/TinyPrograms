#include "ansi_graphics.h"

int main() {
    GL_init();
    for(int i=0; i<25; ++i) {
        for(int j=0; j<80; ++j) {
            int r = (24-i)*4;
            int g = i*4;
            int b = j*2;
            GL_setpixel(j,i,r,g,b);
        }
    }
    GL_terminate();
}
