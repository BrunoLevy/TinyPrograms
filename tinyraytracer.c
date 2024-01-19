/* A port of Dmitry Sokolov's tiny raytracer to C and to FemtoRV32 */
/* Displays on the small OLED display and/or HDMI                  */
/* Bruno Levy, 2020                                                */
/* Original tinyraytracer: https://github.com/ssloy/tinyraytracer  */

#include <math.h>


// It is 80x50 (rather than 80x25) because GL_scan_RGB() and GL_scan_RGBf()
// use "double resolution" "pixels".
//
// Default 80x50 may feel to small, you can use larger value (and enlarge
// your terminal window).

#define GL_width  80
#define GL_height 50
#include "GL_tty.h"

/*******************************************************************/

typedef int BOOL;

static inline float max(float x, float y) { return x>y?x:y; }
static inline float min(float x, float y) { return x<y?x:y; }

/*******************************************************************/

typedef struct { float x,y,z; }   vec3;
typedef struct { float x,y,z,w; } vec4;

static inline vec3 make_vec3(float x, float y, float z) {
  vec3 V;
  V.x = x; V.y = y; V.z = z;
  return V;
}

static inline vec4 make_vec4(float x, float y, float z, float w) {
  vec4 V;
  V.x = x; V.y = y; V.z = z; V.w = w;
  return V;
}

static inline vec3 vec3_neg(vec3 V) {
  return make_vec3(-V.x, -V.y, -V.z);
}

static inline vec3 vec3_add(vec3 U, vec3 V) {
  return make_vec3(U.x+V.x, U.y+V.y, U.z+V.z);
}

static inline vec3 vec3_sub(vec3 U, vec3 V) {
  return make_vec3(U.x-V.x, U.y-V.y, U.z-V.z);
}

static inline float vec3_dot(vec3 U, vec3 V) {
  return U.x*V.x+U.y*V.y+U.z*V.z;
}

static inline vec3 vec3_scale(float s, vec3 U) {
  return make_vec3(s*U.x, s*U.y, s*U.z);
}

static inline float vec3_length(vec3 U) {
  return sqrtf(U.x*U.x+U.y*U.y+U.z*U.z);
}

static inline vec3 vec3_normalize(vec3 U) {
  return vec3_scale(1.0f/vec3_length(U),U);
}

/*************************************************************************/

typedef struct Light {
    vec3 position;
    float intensity;
} Light;

Light make_Light(vec3 position, float intensity) {
  Light L;
  L.position = position;
  L.intensity = intensity;
  return L;
}

/*************************************************************************/

typedef struct {
    float refractive_index;
    vec4  albedo;
    vec3  diffuse_color;
    float specular_exponent;
} Material;

Material make_Material(float r, vec4 a, vec3 color, float spec) {
  Material M;
  M.refractive_index = r;
  M.albedo = a;
  M.diffuse_color = color;
  M.specular_exponent = spec;
  return M;
}

Material make_Material_default() {
  Material M;
  M.refractive_index = 1;
  M.albedo = make_vec4(1,0,0,0);
  M.diffuse_color = make_vec3(0,0,0);
  M.specular_exponent = 0;
  return M;
}

/*************************************************************************/

typedef struct {
  vec3 center;
  float radius;
  Material material;
} Sphere;

Sphere make_Sphere(vec3 c, float r, Material M) {
  Sphere S;
  S.center = c;
  S.radius = r;
  S.material = M;
  return S;
}

BOOL Sphere_ray_intersect(Sphere* S, vec3 orig, vec3 dir, float* t0) {
  vec3 L = vec3_sub(S->center, orig);
  float tca = vec3_dot(L,dir);
  float d2 = vec3_dot(L,L) - tca*tca;
  float r2 = S->radius*S->radius;
  if (d2 > r2) return 0;
  float thc = sqrtf(r2 - d2);
  *t0       = tca - thc;
  float t1 = tca + thc;
  if (*t0 < 0) *t0 = t1;
  if (*t0 < 0) return 0;
  return 1;
}

vec3 reflect(vec3 I, vec3 N) {
  return vec3_sub(I, vec3_scale(2.f*vec3_dot(I,N),N));
}

vec3 refract(vec3 I, vec3 N, float eta_t, float eta_i /* =1.f */) {
  // Snell's law
  float cosi = -max(-1.f, min(1.f, vec3_dot(I,N)));
  // if the ray comes from the inside the object, swap the air and the media  
  if (cosi<0) return refract(I, vec3_neg(N), eta_i, eta_t); 
    float eta = eta_i / eta_t;
    float k = 1 - eta*eta*(1 - cosi*cosi);
    // k<0 = total reflection, no ray to refract.
    // I refract it anyways, this has no physical meaning
    return k<0 ? make_vec3(1,0,0)
              : vec3_add(vec3_scale(eta,I),vec3_scale((eta*cosi - sqrtf(k)),N));
}

BOOL scene_intersect(
   vec3 orig, vec3 dir, Sphere* spheres, int nb_spheres,
   vec3* hit, vec3* N, Material* material
) {
  float spheres_dist = 1e30;
  for(int i=0; i<nb_spheres; ++i) {
    float dist_i;
    if(
       Sphere_ray_intersect(&spheres[i], orig, dir, &dist_i) &&
       (dist_i < spheres_dist)
    ) {
      spheres_dist = dist_i;
      *hit = vec3_add(orig,vec3_scale(dist_i,dir));
      *N = vec3_normalize(vec3_sub(*hit, spheres[i].center));
      *material = spheres[i].material;
    }
  }
  float checkerboard_dist = 1e30;
  if (fabs(dir.y)>1e-3)  {
    float d = -(orig.y+4)/dir.y; // the checkerboard plane has equation y = -4
    vec3 pt = vec3_add(orig, vec3_scale(d,dir));
    if (d>0 && fabs(pt.x)<10 && pt.z<-10 && pt.z>-30 && d<spheres_dist) {
      checkerboard_dist = d;
      *hit = pt;
      *N = make_vec3(0,1,0);
      material->diffuse_color =
	(((int)(.5*hit->x+1000) + (int)(.5*hit->z)) & 1)
	             ? make_vec3(.3, .3, .3)
	             : make_vec3(.3, .2, .1);
    }
  }
  return min(spheres_dist, checkerboard_dist)<1000;
}

vec3 cast_ray(
   vec3 orig, vec3 dir, Sphere* spheres, int nb_spheres,
   Light* lights, int nb_lights, int depth /* =0 */
) {
  vec3 point,N;
  Material material = make_Material_default();
  if (
    depth>2 ||
    !scene_intersect(orig, dir, spheres, nb_spheres, &point, &N, &material)
  ) {
    float s = 0.5*(dir.y + 1.0);
    return vec3_add(
	vec3_scale(s,make_vec3(0.2, 0.7, 0.8)),
        vec3_scale(s,make_vec3(0.0, 0.0, 0.5))
    );
  }

  vec3 reflect_dir=vec3_normalize(reflect(dir, N));
  vec3 refract_dir=vec3_normalize(refract(dir,N,material.refractive_index,1));
  
  // offset the original point to avoid occlusion by the object itself 
  vec3 reflect_orig =
    vec3_dot(reflect_dir,N) < 0
               ? vec3_sub(point,vec3_scale(1e-3,N))
               : vec3_add(point,vec3_scale(1e-3,N)); 
  vec3 refract_orig =
    vec3_dot(refract_dir,N) < 0
               ? vec3_sub(point,vec3_scale(1e-3,N))
               : vec3_add(point,vec3_scale(1e-3,N));
  vec3 reflect_color = cast_ray(
       reflect_orig, reflect_dir, spheres, nb_spheres,
       lights, nb_lights, depth + 1
  );
  vec3 refract_color = cast_ray(
       refract_orig, refract_dir, spheres, nb_spheres,
       lights, nb_lights, depth + 1
  );
  
  float diffuse_light_intensity = 0, specular_light_intensity = 0;
  for (int i=0; i<nb_lights; i++) {
    vec3  light_dir = vec3_normalize(vec3_sub(lights[i].position,point));
    float light_distance = vec3_length(vec3_sub(lights[i].position,point));

    vec3 shadow_orig =
      vec3_dot(light_dir,N) < 0
                ? vec3_sub(point,vec3_scale(1e-3,N))
                : vec3_add(point,vec3_scale(1e-3,N)) ;
    // checking if the point lies in the shadow of the lights[i]
    vec3 shadow_pt, shadow_N;
    Material tmpmaterial;
    if (
       scene_intersect(
	 shadow_orig, light_dir, spheres, nb_spheres,
	 &shadow_pt, &shadow_N, &tmpmaterial
       ) && (
  	 vec3_length(vec3_sub(shadow_pt,shadow_orig)) < light_distance
	     )
    ) continue ;
    
    diffuse_light_intensity  +=
                  lights[i].intensity * max(0.f, vec3_dot(light_dir,N));
     
    float abc = max(
	           0.f, vec3_dot(vec3_neg(reflect(vec3_neg(light_dir), N)),dir)
	        );
    float def = material.specular_exponent;
    if(abc > 0.0f && def > 0.0f) {
      specular_light_intensity += powf(abc,def)*lights[i].intensity;
    }
  }
  vec3 result = vec3_scale(
      diffuse_light_intensity * material.albedo.x, material.diffuse_color
  );
  result = vec3_add(
       result, vec3_scale(specular_light_intensity * material.albedo.y,
       make_vec3(1,1,1))
  );
  result = vec3_add(result, vec3_scale(material.albedo.z, reflect_color));
  result = vec3_add(result, vec3_scale(material.albedo.w, refract_color));
  return result;
}


int nb_spheres = 4;
Sphere spheres[4];

int nb_lights = 3;
Light lights[3];

void init_scene() {
    Material ivory = make_Material(
       1.0, make_vec4(0.6,  0.3, 0.1, 0.0), make_vec3(0.4, 0.4, 0.3),   50.
    );
    Material glass = make_Material(
       1.5, make_vec4(0.0,  0.5, 0.1, 0.8), make_vec3(0.6, 0.7, 0.8),  125.
    );
    Material red_rubber = make_Material(
       1.0, make_vec4(0.9,  0.1, 0.0, 0.0), make_vec3(0.3, 0.1, 0.1),   10.
    );
    Material mirror = make_Material(
       1.0, make_vec4(0.0, 10.0, 0.8, 0.0), make_vec3(1.0, 1.0, 1.0),  142.
    );

    spheres[0] = make_Sphere(make_vec3(-3,    0,   -16), 2,      ivory);
    spheres[1] = make_Sphere(make_vec3(-1.0, -1.5, -12), 2,      glass);
    spheres[2] = make_Sphere(make_vec3( 1.5, -0.5, -18), 3, red_rubber);
    spheres[3] = make_Sphere(make_vec3( 7,    5,   -18), 4,     mirror);

    lights[0] = make_Light(make_vec3(-20, 20,  20), 1.5);
    lights[1] = make_Light(make_vec3( 30, 50, -25), 1.8);
    lights[2] = make_Light(make_vec3( 30, 20,  30), 1.7);
}


void render(int x, int y, float* r, float* g, float* b) {
   const float fov  = M_PI/3.;
   float dir_x =  (x + 0.5) - GL_width/2.;
   float dir_y = -(y + 0.5) + GL_height/2.; // this flips the image.
   float dir_z = -GL_height/(2.*tan(fov/2.));
   vec3 C = cast_ray(
       make_vec3(0,0,0), vec3_normalize(make_vec3(dir_x, dir_y, dir_z)),
       spheres, nb_spheres, lights, nb_lights, 0
   );
   *r=C.x;
   *g=C.y;
   *b=C.z;
}


int main() {
    init_scene();
    GL_init();
    GL_scan_RGBf(GL_width, GL_height, render);
    GL_terminate();
    return 0;
}
