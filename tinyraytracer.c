// clang-format off
/* A port of Dmitry Sokolov's tiny raytracer to C and to FemtoRV32
 * Displays on the small OLED display and/or HDMI
 * Bruno Levy, 2020
 * Original tinyraytracer: https://github.com/ssloy/tinyraytracer
 *
 * Fixed-point (Q16.16) implementation, unified build switch (USE_FIXED),
 * integer sqrt, and pure-integer RGB8 renderer:
 *   Author: Hirosh Dabui <hirosh@dabui.de>
 *
 * Build:
 *   float/original:  gcc tinyraytracer.c -O2 -lm -o tinyraytracer && ./tinyraytracer
 *   fixed Q16.16:    gcc tinyraytracer.c -O2 -DUSE_FIXED -lm -o tinyraytracer && ./tinyraytracer
 */
// clang-format on

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

// It is 80x50 (rather than 80x25) because GL_scan_RGB() and GL_scan_RGBf()
// use "double resolution" "pixels".
//
// Default 80x50 may feel to small, you can use larger value (and enlarge
// your terminal window).

#define GL_width 80
#define GL_height 50
#include "GL_tty.h"

/*=========================== Scalar + ops ==================================*/
#ifdef USE_FIXED
/* -------- Q16.16 fixed-point -------- */
typedef int32_t scal; /* Q16.16 */
#define SHIFT 16
#define ONE ((scal)1 << SHIFT)
#define HALF ((scal)1 << (SHIFT - 1))

static inline scal S(int32_t x) { return (scal)(x << SHIFT); }
static inline scal SF(double f) {
  return (scal)(f * 65536.0 + (f >= 0 ? 0.5 : -0.5));
}
static inline scal ADD(scal a, scal b) { return a + b; }
static inline scal SUB(scal a, scal b) { return a - b; }
static inline scal NEG(scal a) { return -a; }
static inline scal MUL(scal a, scal b) {
  return (scal)(((int64_t)a * (int64_t)b) >> SHIFT);
}
static inline scal DIV(scal a, scal b) {
  return (scal)(((int64_t)a << SHIFT) / (int64_t)b);
}
static inline scal MAX(scal a, scal b) { return a > b ? a : b; }
static inline scal MIN(scal a, scal b) { return a < b ? a : b; }
static inline scal ABS(scal a) { return a < 0 ? -a : a; }

static inline uint32_t isqrt_u64(uint64_t x) {
  uint64_t op = x, res = 0, one = 1ULL << 62;
  while (one > op)
    one >>= 2;
  while (one) {
    if (op >= res + one) {
      op -= res + one;
      res = (res >> 1) + one;
    } else
      res >>= 1;
    one >>= 2;
  }
  return (uint32_t)res;
}
static inline scal SQRT(scal x) {
  if (x <= 0)
    return 0;
  return (scal)isqrt_u64(((uint64_t)(uint32_t)x) << SHIFT);
}

static inline scal POWI(scal a, int e) {
  scal r = ONE;
  while (e) {
    if (e & 1)
      r = MUL(r, a);
    a = MUL(a, a);
    e >>= 1;
  }
  return r;
}

static inline int TO_INT(scal a) { return (int)(a >> SHIFT); }

#define SURF_BIAS S(1) /* robust surface bias in fixed */
#define EPS_Y SF(0.001)
#define INV_2TAN_FOV SF(0.86602540378443864676) /* 1/(2*tan(pi/6)) */
#define BIG_SCAL ((scal)0x7FFFFFFF)             /* safe "infinity" for Q16.16 */

/* fixed build renders as RGB8 (pure integer) */
#define USE_RGB8 1
#else
/* -------------------------- native float -------------------------- */
typedef float scal;
#define ONE 1.0f

#define S(x) ((float)(x))
#define SF(f) ((float)(f))
#define ADD(a, b) ((a) + (b))
#define SUB(a, b) ((a) - (b))
#define NEG(a) (-(a))
#define MUL(a, b) ((a) * (b))
#define DIV(a, b) ((a) / (b))
#define MAX(a, b) fmaxf((a), (b))
#define MIN(a, b) fminf((a), (b))
#define ABS(a) fabsf((a))
#define SQRT(x) sqrtf(x)
static inline scal POWI(scal a, int e) { return powf(a, (float)e); }

static inline int TO_INT(scal a) { return (int)(a); }

#define SURF_BIAS SF(1e-3)
#define EPS_Y SF(1e-3)
#define BIG_SCAL SF(1e30)

#define USE_RGB8 0
#endif

/*============================= Vectors =====================================*/
typedef struct {
  scal x, y, z;
} v3;
typedef struct {
  scal x, y, z, w;
} v4;

static inline v3 V3(scal x, scal y, scal z) {
  v3 v = {x, y, z};
  return v;
}
static inline v4 V4(scal x, scal y, scal z, scal w) {
  v4 v = {x, y, z, w};
  return v;
}
static inline v3 v3_add(v3 a, v3 b) {
  return V3(ADD(a.x, b.x), ADD(a.y, b.y), ADD(a.z, b.z));
}
static inline v3 v3_sub(v3 a, v3 b) {
  return V3(SUB(a.x, b.x), SUB(a.y, b.y), SUB(a.z, b.z));
}
static inline v3 v3_neg(v3 a) { return V3(NEG(a.x), NEG(a.y), NEG(a.z)); }
static inline v3 v3_scale(scal s, v3 a) {
  return V3(MUL(s, a.x), MUL(s, a.y), MUL(s, a.z));
}
static inline scal v3_dot(v3 a, v3 b) {
  return ADD(ADD(MUL(a.x, b.x), MUL(a.y, b.y)), MUL(a.z, b.z));
}
static inline scal v3_len(v3 a) { return SQRT(v3_dot(a, a)); }
static inline v3 v3_norm(v3 a) {
  scal L = v3_len(a);
  return (L == 0) ? a : v3_scale(DIV(ONE, L), a);
}

/*============================= Scene =======================================*/
typedef struct {
  v3 position;
  scal intensity;
} Light;
typedef struct {
  scal refr;
  v4 albedo;
  v3 diffuse;
  int spec;
} Material;
typedef struct {
  v3 center;
  scal radius;
  Material m;
} Sphere;

static inline v3 reflect(v3 I, v3 N) {
  return v3_sub(I, v3_scale(MUL(S(2), v3_dot(I, N)), N));
}

static v3 refract_(v3 I, v3 N, scal eta_t, scal eta_i) {
  scal dotIN = v3_dot(I, N);
  if (dotIN < NEG(ONE))
    dotIN = NEG(ONE);
  if (dotIN > ONE)
    dotIN = ONE;
  scal cosi = NEG(dotIN);
  if (cosi < 0)
    return refract_(I, v3_neg(N), eta_i, eta_t);
  scal eta = DIV(eta_i, eta_t);
  scal k = SUB(ONE, MUL(MUL(eta, eta), SUB(ONE, MUL(cosi, cosi))));
  if (k < 0)
    return V3(ONE, 0, 0);
  scal term = SUB(MUL(eta, cosi), SQRT(k));
  return v3_add(v3_scale(eta, I), v3_scale(term, N));
}

/*=========================== Intersections =================================*/
static inline int sphere_hit(const Sphere *sp, v3 orig, v3 dir, scal *t0) {
  v3 L = v3_sub(sp->center, orig);
  scal tca = v3_dot(L, dir);
  scal d2 = SUB(v3_dot(L, L), MUL(tca, tca));
  scal r2 = MUL(sp->radius, sp->radius);
  if (d2 > r2)
    return 0;
  scal thc = SQRT(SUB(r2, d2));
  scal t1 = ADD(tca, thc);
  *t0 = SUB(tca, thc);
  if (*t0 < SURF_BIAS)
    *t0 = t1;
  if (*t0 < SURF_BIAS)
    return 0;
  return 1;
}

static int scene_hit(v3 orig, v3 dir, const Sphere *sp, int ns, v3 *hit, v3 *N,
                     Material *mat) {
  scal spheres_dist = BIG_SCAL;
  for (int i = 0; i < ns; i++) {
    scal di;
    if (sphere_hit(&sp[i], orig, dir, &di) && di < spheres_dist) {
      spheres_dist = di;
      *hit = v3_add(orig, v3_scale(di, dir));
      *N = v3_norm(v3_sub(*hit, sp[i].center));
      *mat = sp[i].m;
    }
  }

  scal checker_dist = BIG_SCAL;
  if (ABS(dir.y) > EPS_Y) {
    scal d = DIV(NEG(ADD(orig.y, S(4))), dir.y); /* plane y=-4 */
    v3 pt = v3_add(orig, v3_scale(d, dir));
    if (d > SURF_BIAS && ABS(pt.x) < S(10) && pt.z < S(-10) && pt.z > S(-30) &&
        d < spheres_dist) {
      checker_dist = d;
      *hit = pt;
      *N = V3(0, ONE, 0);
      int cx = TO_INT(ADD(MUL(SF(0.5), hit->x), S(1000)));
      int cz = TO_INT(MUL(SF(0.5), hit->z));
      mat->diffuse = ((cx + cz) & 1) ? V3(SF(0.3), SF(0.3), SF(0.3))
                                     : V3(SF(0.3), SF(0.2), SF(0.1));
    }
  }

  scal best = MIN(spheres_dist, checker_dist);
  return best < S(1000);
}

/*============================= Tracing =====================================*/
static v3 cast_ray(v3 orig, v3 dir, const Sphere *sp, int ns, const Light *li,
                   int nl, int depth) {
  const int MAXD = 2; /* original depth */

  v3 P, N;
  Material M = (Material){ONE, V4(ONE, 0, 0, 0), V3(0, 0, 0), 0};
  if (depth > MAXD || !scene_hit(orig, dir, sp, ns, &P, &N, &M)) {
    scal s = MUL(SF(0.5), ADD(dir.y, ONE)); /* original sky */
    return v3_add(v3_scale(s, V3(SF(0.2), SF(0.7), SF(0.8))),
                  v3_scale(s, V3(0, 0, SF(0.5))));
  }

  v3 Rdir = v3_norm(reflect(dir, N));
  v3 Tdir = v3_norm(refract_(dir, N, M.refr, ONE));

  v3 Rorg = (v3_dot(Rdir, N) < 0) ? v3_sub(P, v3_scale(SURF_BIAS, N))
                                  : v3_add(P, v3_scale(SURF_BIAS, N));
  v3 Torg = (v3_dot(Tdir, N) < 0) ? v3_sub(P, v3_scale(SURF_BIAS, N))
                                  : v3_add(P, v3_scale(SURF_BIAS, N));

  v3 Rcol = cast_ray(Rorg, Rdir, sp, ns, li, nl, depth + 1);
  v3 Tcol = cast_ray(Torg, Tdir, sp, ns, li, nl, depth + 1);

  scal diff = 0, spec = 0;
  for (int i = 0; i < nl; i++) {
    v3 Ldir = v3_norm(v3_sub(li[i].position, P));
    scal Ldist = v3_len(v3_sub(li[i].position, P));

    v3 Sorg = (v3_dot(Ldir, N) < 0) ? v3_sub(P, v3_scale(SURF_BIAS, N))
                                    : v3_add(P, v3_scale(SURF_BIAS, N));
    v3 spt, sN;
    Material tmp = M;
    if (scene_hit(Sorg, Ldir, sp, ns, &spt, &sN, &tmp) &&
        v3_len(v3_sub(spt, Sorg)) < Ldist)
      continue;

    diff = ADD(diff, MUL(li[i].intensity, MAX(0, v3_dot(Ldir, N))));
    v3 r = reflect(v3_neg(Ldir), N);
    scal base = MAX(0, v3_dot(v3_neg(r), dir));
    if (base > 0 && M.spec > 0) {
      spec = ADD(spec, MUL(li[i].intensity, POWI(base, M.spec)));
    }
  }

  v3 C = v3_scale(MUL(diff, M.albedo.x), M.diffuse);
  C = v3_add(C, v3_scale(MUL(spec, M.albedo.y), V3(ONE, ONE, ONE)));
  C = v3_add(C, v3_scale(M.albedo.z, Rcol));
  C = v3_add(C, v3_scale(M.albedo.w, Tcol));
  return C;
}

/*============================= Scene data ==================================*/
static int ns = 4;
static Sphere spheres[4];
static int nl = 3;
static Light lights[3];

#ifdef USE_FIXED
static scal DIRZ_SCALE; /* -GL_height/(2*tan(fov/2)) precomputed */
#endif

static void init_scene(void) {
  Material ivory = (Material){SF(1.0), V4(SF(0.6), SF(0.3), SF(0.1), SF(0.0)),
                              V3(SF(0.4), SF(0.4), SF(0.3)), 50};
  Material glass = (Material){SF(1.5), V4(SF(0.0), SF(0.5), SF(0.1), SF(0.8)),
                              V3(SF(0.6), SF(0.7), SF(0.8)), 125};
  Material redrb = (Material){SF(1.0), V4(SF(0.9), SF(0.1), SF(0.0), SF(0.0)),
                              V3(SF(0.3), SF(0.1), SF(0.1)), 10};
  Material mirror = (Material){SF(1.0), V4(SF(0.0), SF(10.0), SF(0.8), SF(0.0)),
                               V3(ONE, ONE, ONE), 142};

  spheres[0] = (Sphere){V3(S(-3), 0, S(-16)), S(2), ivory};
  spheres[1] = (Sphere){V3(SF(-1.0), SF(-1.5), S(-12)), S(2), glass};
  spheres[2] = (Sphere){V3(SF(1.5), SF(-0.5), S(-18)), S(3), redrb};
  spheres[3] = (Sphere){V3(S(7), S(5), S(-18)), S(4), mirror};

  lights[0] = (Light){V3(S(-20), S(20), S(20)), SF(1.5)};
  lights[1] = (Light){V3(S(30), S(50), S(-25)), SF(1.8)};
  lights[2] = (Light){V3(S(30), S(20), S(30)), SF(1.7)};

#ifdef USE_FIXED
  DIRZ_SCALE = NEG(MUL(S(GL_height), INV_2TAN_FOV));
#endif
}

/*============================= Rendering ===================================*/
#if USE_RGB8
static void render(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
  /* fixed-mode camera (no tanf), pure integer */
  scal dir_x = SUB(ADD(S(x), SF(0.5)), S(GL_width / 2));
  scal dir_y = SUB(S(GL_height / 2), ADD(S(y), SF(0.5))); /* flip */
  scal dir_z = DIRZ_SCALE;

  v3 C = cast_ray(V3(0, 0, 0), v3_norm(V3(dir_x, dir_y, dir_z)), spheres, ns,
                  lights, nl, 0);

  /* clamp to [0,ONE] and quantize to 0..255 with rounding */
  scal cx = C.x < 0 ? 0 : (C.x > ONE ? ONE : C.x);
  scal cy = C.y < 0 ? 0 : (C.y > ONE ? ONE : C.y);
  scal cz = C.z < 0 ? 0 : (C.z > ONE ? ONE : C.z);

  uint32_t R = (uint32_t)((((int64_t)cx) * 255 + HALF) >> SHIFT);
  uint32_t G = (uint32_t)((((int64_t)cy) * 255 + HALF) >> SHIFT);
  uint32_t B = (uint32_t)((((int64_t)cz) * 255 + HALF) >> SHIFT);

  *r = (R > 255) ? 255 : (uint8_t)R;
  *g = (G > 255) ? 255 : (uint8_t)G;
  *b = (B > 255) ? 255 : (uint8_t)B;
}
#else
static void render(int x, int y, float *r, float *g, float *b) {
  const float fov = (float)M_PI / 3.f;
  float dir_x = (x + 0.5f) - GL_width / 2.f;
  float dir_y = -(y + 0.5f) + GL_height / 2.f; /* flip */
  float dir_z = -GL_height / (2.f * tanf(fov / 2.f));
  v3 C = cast_ray(V3(0, 0, 0), v3_norm(V3(dir_x, dir_y, dir_z)), spheres, ns,
                  lights, nl, 0);
  *r = C.x;
  *g = C.y;
  *b = C.z;
}
#endif

/*================================= Main ====================================*/
int main(void) {
  init_scene();
  GL_init();
#if USE_RGB8
  GL_scan_RGB(GL_width, GL_height, render);
#else
  GL_scan_RGBf(GL_width, GL_height, render);
#endif
  GL_terminate();
  return 0;
}
