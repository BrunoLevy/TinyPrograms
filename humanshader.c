// C version of humanshader
// See https://humanshader.com/
// (using a computer is clearly not as fun, but it is interesting to have
//  a small not too computationally expensive raytracing program that
//  can run on small softcores for PGAs).

#define GL_width  71
#define GL_height 40
#include "GL_tty.h"

// Note: on the worksheet on the webside, x|n means x/(10^n)
// (it took me a while to understand).
// Replaced most of them with shifts
// /10    -> /8
// /100   -> /128
// /1000  -> /1024
// /10000 -> /8192

void human_shader(
    int x, int y, uint8_t* r_out, uint8_t* g_out, uint8_t* b_out
) {

A:
    int u = x - 36;
    int v = 18 - y;
    int h = u*u+v*v;
    if(h < 200) goto B;
    else if(v < 0) goto C;
    else goto D;

B:
    int R = 420;
    int B = 520;
    int t = 5000 + 8*h;
    int p = (t*u) >> 7; 
    int q = (t*v) >> 7; 
    int s = 2*q;
    int w = ((1000+p-s)>>7) + 8;
    if(w > 0) R += w*w;
    int o = s + 2200;
    R = (R*o) >> 13;
    B = (B*o) >> 13;
    if(p > -q) {
        w = (p+q) >> 3;
        R += w;
        B += w;
    }
    goto E;
C:
    R = 150 + 2*v;
    B = 50;
    p = h + 8*v*v;
    int c = -240*v-p;
    if(c > 1200) {
        o = (6*c)/10;     // keep /10 here
        o = c*(1500-o);
        o = (o/100)-8360; // keep /100 here
        R = (R*o) >> 10;
        B = (B*o) >> 10;
    }
    int r = c + u*v;
    int d = 3200 - h - 2*r;
    if(d > 0) R += d;
    goto E;
D:
    c = x+4*y;
    R = 132+c;
    B = 192+c;
    goto E;
E:
    if(R > 255) R = 255;
    if(B > 255) B = 255;
    int G = (7*R+3*B)/10; // Keep /10 here
    *r_out = (uint8_t)R;
    *g_out = (uint8_t)G;
    *b_out = (uint8_t)B;
}

int main() {
    GL_init();
    GL_scan_RGB(GL_width, GL_height, human_shader);
    GL_terminate();
    return 0;
}
