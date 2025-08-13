/***| raym_earth.c |

┌─────────────────────────────────────────────────────────────────────────────┐
│  Copyright (c) 2025 Carlos Venegas <carlos@magnitude.es>                    │     
│  X: @cavearr | github: @cavearr | FPGAwars: charliva@gmail.com              │
│                                                                             │
│  This work is dedicated to the public domain under the Creative Commons     │
│  Zero (CC0) 1.0 Universal. To the extent possible under law, the author(s)  │
│  have waived all copyright and related or neighboring rights to this work   │
│  worldwide. No rights are reserved.                                         │
│  Full text: https://creativecommons.org/publicdomain/zero/1.0/              │
│                                                                             │
│  -- Citing authorship is a personal ethical decision --                     │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│  
│  Simple heightmap renderer oriented to RISC-V/FPGA using ray marching.
│  Calculates each pixel directly without intermediate buffers
│  
│  ALGORITHM OVERVIEW - Ray Marching Terrain Renderer
│  ====================================================
│  
│  For each pixel on screen:
│    1. Cast a ray from camera through the pixel
│    2. March along the ray in steps
│    3. Check terrain height at each step
│    4. If ray hits terrain, color the pixel
│    5. Otherwise, draw sky
│  
│  CAMERA & WORLD COORDINATE SYSTEM:
│  
│      Y (forward)
│      ^
│      |
│      |
│      +-----> X (right)
│     /
│    /
│   Z (up)
│  
│  SCREEN SPACE TO WORLD SPACE TRANSFORMATION:
│  
│   Screen (80x50)          Ray Direction
│   +------------+          Camera
│   |············|            *
│   |·····P······|           /|\
│   |············|          / | \
│   +------------+         /  |  \
│                         /   |   \
│   P = pixel       Ray--+----+----+-- Terrain check points
│                       d1   d2   d3   (marching steps)
│  
│  RAY MARCHING VISUALIZATION:
│  
│   Side view:
│   
│   Camera
│     *     Ray trajectory
│      \    ·····
│       \  ····· ·····
│        \····· ········→ (march until hit)
│         *·············
│        __|_____/\_____|__ Terrain surface
│       /  |    /  \    |
│          ↑ Hit point
│  
│  TERRAIN HEIGHT FUNCTION:
│    h(x,z) = Σ sin(x*freq) * cos(z*freq) 
│    Using multiple frequencies for detail
│  
│  COLOR MAPPING BY HEIGHT:
│    h < -2: Deep valley (dark green)
│    h < 0:  Valley (green)  
│    h < 3:  Plains (bright green)
│    h < 6:  Hills (medium green)
│    h > 6:  Mountains (brown)
│  
│  CAMERA MOVEMENT PATTERN:
│  
│    6 sec straight → 3 sec rotation → 6 sec straight → repeat
│    
│    Top view:
│    --------→ (6s)
│            \
│             \ (3s rotate)
│              \
│               --------→ (6s)
│                       \
│                        \ (3s)
│                         ...
│  
│  
└─────────────────────────────────────────────────────────────────────────────┘
***/

#include <math.h>

#define GL_width  80
#define GL_height 50
#include "GL_tty.h"

// Global camera position and orientation
float cam_x = 0.0f;      // Camera X position in world
float cam_y = 0.0f;      // Camera Y position (forward axis)
float cam_z = 10.0f;     // Camera height above terrain
float cam_angle = 0.0f;  // Camera viewing angle (radians)

// Terrain height function using layered sine waves
// Returns height at world position (x,z)
float get_height(float x, float z) {
    // Layer 1: Large scale mountains
    float h = 5.0f * sinf(x * 0.1f) * cosf(z * 0.1f);
    // Layer 2: Medium scale hills  
    h += 3.0f * sinf(x * 0.2f + z * 0.15f);
    return h;
}

// Per-pixel rendering function called by GL_scan_RGBf
// Implements ray marching algorithm for each screen pixel
void render_pixel(int x, int y, float* r, float* g, float* b) {
    // Convert screen coordinates to normalized ray direction
    // screen_x: -1.0 (left) to +1.0 (right)
    // screen_y: -1.0 (top) to +1.0 (bottom)
    float screen_x = (float)(x - GL_width/2) / (float)GL_width * 2.0f;
    float screen_y = (float)(y - GL_height/2) / (float)GL_height * 2.0f;
    
    // Ray marching parameters
    float max_dist = 80.0f;   // Maximum marching distance
    float step = 0.5f;        // Initial step size (smaller = more precise)
    int hit = 0;
    float hit_height = 0.0f;
    float fog = 1.0f;
    
    // Ray marching loop - step along ray until terrain hit or max distance
    for (float dist = 1.0f; dist < max_dist; dist += step) {
        // Calculate world position for this ray step
        // Apply camera rotation to ray direction
        float ray_angle = cam_angle + screen_x * 0.5f;  // 0.5f = FOV factor
        float world_x = cam_x + sinf(ray_angle) * dist;
        float world_z = cam_y + cosf(ray_angle) * dist;
        
        // Sample terrain height at this position
        float terrain_h = get_height(world_x, world_z);
        
        // Calculate ray height at this distance
        // Includes perspective projection and downward tilt
        float ray_height = cam_z - screen_y * dist * 0.4f - dist * 0.2f;
        
        // Check for ray-terrain intersection
        if (ray_height <= terrain_h) {
            hit = 1;
            hit_height = terrain_h;
            fog = 1.0f - dist / max_dist;  // Calculate fog based on distance
            break;
        }
        
        // Adaptive step size for performance
        if (dist > 20.0f) step = 1.0f;   // Medium distance: larger steps
        if (dist > 40.0f) step = 2.0f;   // Far distance: even larger steps
    }
    
    if (hit) {
        // Terrain coloring based on height - greens and browns only
        if (hit_height < -2.0f) {
            // Deep valley - very dark green
            *r = 0.05f;
            *g = 0.2f;
            *b = 0.05f;
        } else if (hit_height < 0.0f) {
            // Valley - dark green
            *r = 0.1f;
            *g = 0.3f;
            *b = 0.08f;
        } else if (hit_height < 3.0f) {
            // Plains - bright green
            *r = 0.15f;
            *g = 0.7f;
            *b = 0.1f;
        } else if (hit_height < 6.0f) {
            // Hills - medium green
            *r = 0.2f;
            *g = 0.5f;
            *b = 0.15f;
        } else {
            // Mountains - brown
            *r = 0.6f;
            *g = 0.4f;
            *b = 0.2f;
        }
        
        // Apply fog only to very distant terrain
        if (fog < 0.3f) {  // Only if very far
            float fog_factor = fog / 0.3f;
            *r = *r * fog_factor + 0.7f * (1.0f - fog_factor);
            *g = *g * fog_factor + 0.75f * (1.0f - fog_factor);
            *b = *b * fog_factor + 0.8f * (1.0f - fog_factor);
        }
        // Otherwise keep vivid colors without fog
    } else {
        // Sky - gradient from dark blue (top) to light blue (horizon)
        float sky_gradient = (float)y / GL_height;  // 0 at top, 1 at bottom
        *r = 0.2f + sky_gradient * 0.5f;  // Dark to light
        *g = 0.3f + sky_gradient * 0.5f;
        *b = 0.5f + sky_gradient * 0.45f;
    }
}

int main() {
    GL_init();
    
    int frame_count = 0;
    int state = 0;  // 0 = moving straight, 1 = rotating
    int state_timer = 180;  // 6 seconds for first straight segment
    float rotation_speed = 0.0f;
    float total_rotation = 0.0f;
    
    // Main rendering loop
    while(1) {
        // GL_scan_RGBf calls render_pixel for each screen pixel
        // No intermediate buffers - everything calculated on the fly
        GL_scan_RGBf(GL_width, GL_height, render_pixel);
        GL_swapbuffers();
        
        // Always move forward in current direction
        cam_x += sinf(cam_angle) * 0.1f;
        cam_y += cosf(cam_angle) * 0.1f;
        
        // Navigation state machine
        state_timer--;
        
        if (state == 0) {  // Moving straight
            // No rotation, just advance
            if (state_timer <= 0) {
                // Switch to rotation state
                state = 1;
                state_timer = 90;  // 3 seconds of rotation
                
                // Decide rotation direction and speed randomly
                int random_val = (frame_count * 7) % 100;
                float turn_degrees = (random_val - 50) * 0.8f;  // Between -40 and +40 degrees
                rotation_speed = (turn_degrees * 0.017453f) / 90.0f;  // Convert to radians/frame
                total_rotation = 0.0f;
            }
        } else {  // Rotating
            // Apply continuous smooth rotation
            cam_angle += rotation_speed;
            total_rotation += rotation_speed;
            
            if (state_timer <= 0) {
                // Return to straight movement
                state = 0;
                state_timer = 180;  // 6 seconds moving straight
                rotation_speed = 0.0f;
            }
        }
        
        frame_count++;
    }
    
    GL_terminate();
    return 0;
}
