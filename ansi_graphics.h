/**
 * ansi_graphics.h
 * A couple of function to display graphics in the terminal, 
 * using ansi sequences.
 * Bruno Levy, Jan 2024
 */

#include <stdio.h>
#include <math.h>
#include <unistd.h>

// You can define GL_width and GL_height before
// #including ansi_graphics.h in case the plain
// old 80x25 pixels does not suffice.

#ifndef GL_width
#define GL_width  80
#endif

#ifndef GL_height
#define GL_height 25
#endif

/**
 * \brief Sets the current graphics position
 * \param[in] x typically in 0,79
 * \param[in] y typically in 0,24
 */
static inline void GL_gotoxy(int x, int y) {
    printf("\033[%d;%dH",y,x);
}

/**
 * \brief Sets the current graphics position
 * \param[in] R , G , B the RGB color of the pixel, in [0..255]
 * \details Typically used by programs that draw all pixels sequentially,
 *  like a raytracer. After each line, one can either printf("\n") or
 *  call GL_gotoxy(). If you want to draw individual pixels in an
 *  arbitrary order, use GL_setpixel(x,y,R,G,B)
 */
static inline void GL_setpixelhere(int R, int G, int B) {
    printf("\033[48;2;%d;%d;%dm ",R,G,B); // set background color, print space 
}

/**
 * \brief Sets the color of a pixel
 * \param[in] x typically in 0,79
 * \param[in] y typically in 0,24
 * \param[in] R , G , B the RGB color of the pixel, in [0..255]
 */
static inline void GL_setpixel(int x, int y, int R, int G, int B) {
    GL_gotoxy(x,y);
    GL_setpixelhere(R,G,B);
}

/**
 * \brief restore default foreground and background colors
 */
static inline void GL_restore_default_colors() {
    printf(
        "\033[48;5;16m"   // set background color black
        "\033[38;5;15m"   // set foreground color white
    );
}

/**
 * \brief Call this function each time graphics should be cleared
 */
static inline void GL_clear() {
    GL_restore_default_colors();
    printf("\033[2J"); // clear screen
}

/**
 * \brief Call this function before starting drawing graphics 
 *  or each time graphics should be cleared
 */
static inline void GL_init() {
    printf(
	   "\033[H"     // home
	   "\033[?25l"  // hide cursor
    ); 
    GL_clear();
}


/**
 * \brief Call this function at the end of the program
 */
static inline void GL_terminate() {
    GL_restore_default_colors();
    GL_gotoxy(0,GL_height);
    printf("\033[?25h"); // show cursor
}

/**
 * \brief Flushes pending graphic operations
 * \details Flushes the output buffer of stdout
 */
static inline void GL_flush() {
   fflush(stdout);
}


/***************************************************************/

#define INSIDE 0
#define LEFT   1
#define RIGHT  2
#define BOTTOM 4
#define TOP    8

#define XMIN 0
#define XMAX (GL_width-1)
#define YMIN 0
#define YMAX (GL_height-1)

#define code(x,y) ((x) < XMIN) | (((x) > XMAX)<<1) | (((y) < YMIN)<<2) | (((y) > YMAX)<<3) 

/***************************************************************/

static inline void GL_line(
    int x1, int y1, int x2, int y2, int R, int G, int B
) {
    int x,y,dx,dy,sx,sy,tmp;

    /* Cohen-Sutherland line clipping. */
    int code1 = code(x1,y1);
    int code2 = code(x2,y2);
    int codeout;

    for(;;) {
	/* Both points inside. */
	if(code1 == 0 && code2 == 0) {
	    break;
	}

	/* No point inside. */
	if(code1 & code2) {
	    return;
	}

	/* One of the points is outside. */
	codeout = code1 ? code1 : code2;

	/* Compute intersection. */
	if (codeout & TOP) { 
	    x = x1 + (x2 - x1) * (YMAX - y1) / (y2 - y1); 
	    y = YMAX; 
	} else if (codeout & BOTTOM) { 
	    x = x1 + (x2 - x1) * (YMIN - y1) / (y2 - y1); 
	    y = YMIN; 
	}  else if (codeout & RIGHT) { 
	    y = y1 + (y2 - y1) * (XMAX - x1) / (x2 - x1); 
	    x = XMAX; 
	} else if (codeout & LEFT) { 
	    y = y1 + (y2 - y1) * (XMIN - x1) / (x2 - x1); 
	    x = XMIN; 
	} 
	
	/* Replace outside point with intersection. */
	if (codeout == code1) { 
	    x1 = x; 
	    y1 = y;
	    code1 = code(x1,y1);
	} else { 
	    x2 = x; 
	    y2 = y;
	    code2 = code(x2,y2);
	}
    }
    
    // Swap both extremities to ensure x increases
    if(x2 < x1) {
       tmp = x2;
       x2 = x1;
       x1 = tmp;
       tmp = y2;
       y2 = y1;
       y1 = tmp;
    }
   
    // Bresenham line drawing.
    dy = y2 - y1;
    sy = 1;
    if(dy < 0) {
	sy = -1;
	dy = -dy;
    }

    dx = x2 - x1;
   
    x = x1;
    y = y1;
    
    if(dy > dx) {
	int ex = (dx << 1) - dy;
	for(int u=0; u<dy; u++) {
	    GL_setpixel(x,y,R,G,B);
	    y += sy;
	    if(ex >= 0)  {
		x++;
		ex -= dy << 1;
		GL_setpixel(x,y,R,G,B);
	    }
	    while(ex >= 0)  {
		x++;
		ex -= dy << 1;
	        putchar(' ');
	    }
	    ex += dx << 1;
	}
    } else {
	int ey = (dy << 1) - dx;
	for(int u=0; u<dx; u++) {
	    GL_setpixel(x,y,R,G,B);
	    x++;
	    while(ey >= 0) {
		y += sy;
		ey -= dx << 1;
		GL_setpixel(x,y,R,G,B);
	    }
	    ey += dy << 1;
	}
    }
}


/***************************************************************/

typedef struct {
    int x;        // in [0..79]
    int y;        // in [0..24]
    int angle;    // in degrees
    int R,G,B;    // pen color
    int pendown;  // draw if non-zero
} Turtle;
    
static inline void Turtle_init(Turtle* T) {
    T->x = GL_width/2;
    T->y = GL_height/2;
    T->angle = -90;
    T->pendown = 1;
    T->R = 255;
    T->G = 255;
    T->B = 255;
}

static inline void Turtle_pen_up(Turtle* T) {
    T->pendown = 0;
}

static inline void Turtle_pen_down(Turtle* T) {
    T->pendown = 1;
}

static inline void Turtle_pen_color(Turtle* T, int R, int G, int B) {
    T->R = R;
    T->G = G;
    T->B = B;
}

static inline void Turtle_forward(Turtle* T, int distance) {
    int last_x = T->x;
    int last_y = T->y;
    float alpha = (float)(T->angle) * M_PI / 180.0;
    T->x += (int)(cos(alpha) * (float)(distance));
    T->y += (int)(sin(alpha) * (float)(distance));
    if(T->pendown) {
        GL_line(last_x, last_y, T->x, T->y, T->R, T->G, T->B);
    }
}

static inline void Turtle_backward(Turtle* T, int distance) {
    Turtle_forward(T,-distance);
}

static inline void Turtle_turn_right(Turtle* T, int delta_angle) {
    T->angle += delta_angle;
}

static inline void Turtle_turn_left(Turtle* T, int delta_angle) {
    Turtle_turn_right(T, -delta_angle);
}
