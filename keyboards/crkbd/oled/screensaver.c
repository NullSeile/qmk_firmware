#include "config.h"
#include "lib/lib8tion/lib8tion.h"
#include "lib/lib8tion/random8.h"
#include "oled_driver.h"
#include "quantum.h"
#include "timer.h"
#include "util.h"
#include "wpm.h"
#include <math.h>

#include "custom_keys.h"


oled_rotation_t oled_init_kb(oled_rotation_t rotation) {
    return OLED_ROTATION_270;
    // return OLED_ROTATION_270;
}

#define XMIN 0
#define XMAX 31
#define YMIN 0
// #define YMAX (103 - 2)
#define YMAX (111 - 2)

#if (XMAX > 32) || (XMIN < 0) || (YMAX > 127) || (YMIN < 0)
    #error "Screen bounds exceeded"
#endif

#define MAX_PARTICLES 8
#define PARTICLE_SIZE 5

typedef struct {
    float x;
    float y;
    float dx;
    float dy;
    // float mass;
} particle_t;

static particle_t particles[MAX_PARTICLES];
static bool initialized = false;
static uint8_t active_particles = MAX_PARTICLES;
// static uint8_t prev_wpm = 0;
// static uint32_t timer = 0;
static uint32_t idle_time = 0;

void draw_outline(void) {
    for (uint8_t x = XMIN+1; x <= XMAX-1; x++) { // Top and bottom borders
        uint8_t sft_top = YMIN % 8;
        uint16_t idx_top = (YMIN / 8)*32 + x;
        oled_write_raw_byte(1 << sft_top, idx_top);

        uint8_t sft_bot = YMAX % 8;
        uint16_t idx_bot = (YMAX / 8)*32 + x;
        oled_write_raw_byte(1 << sft_bot, idx_bot);
    }

    for (uint8_t y = YMIN/8 + 1; y <= YMAX/8 - 1; y++) { // Left and right borders
        uint16_t idx_lft = y*32 + XMIN;
        oled_write_raw_byte(255, idx_lft);

        uint16_t idx_rgt = y*32 + XMAX;
        oled_write_raw_byte(255, idx_rgt);
    }
    // Draw final segments vertical segments
    oled_write_raw_byte(~((1<<YMIN%8)-1), YMIN/8*32 + XMIN);
    oled_write_raw_byte(~((1<<YMIN%8)-1), YMIN/8*32 + XMAX);

    oled_write_raw_byte(0xFF>>(7-YMAX%8), (YMAX/8)*32 + XMIN);
    oled_write_raw_byte(0xFF>>(7-YMAX%8), (YMAX/8)*32 + XMAX);
}

void init_particles(void) {
    for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
        float x = random8()/255.f * (XMAX - XMIN - PARTICLE_SIZE - 2) + XMIN + 1;
        float y = random8()/255.f * (YMAX - YMIN - PARTICLE_SIZE - 2) + YMIN + 1;
        particles[i].x = x;
        particles[i].y = y;

        particles[i].dx = 0.8 + (i * 0.3);
        particles[i].dy = 1.2 - (i * 0.2);
        // particles[i].mass = 2.f;
    }
}

void draw_particle(uint8_t x, uint8_t y, bool on) {
    for (uint8_t i = x; i < x + PARTICLE_SIZE && i <= XMAX; i++) {
        for (uint8_t j = y; j < y + PARTICLE_SIZE && j <= YMAX; j++) {
            if (i < XMIN || j < YMIN) continue;
            uint8_t sft = j % 8;
            uint16_t idx = (j / 8)*32 + i;
            uint8_t prev = oled_read_raw_byte(idx);
            if (on) {
                oled_write_raw_byte(prev | 1 << sft, idx);
            } else {
                oled_write_raw_byte(prev & ~(1 << sft), idx);
            }
        }
    }
}

void check_wall_collisions(particle_t *p, float damping) {
    // Left/Right walls
    if (p->x <= XMIN + 1) {
        p->x = XMIN + 1;
        p->dx = -p->dx * damping;
    } else if (p->x + PARTICLE_SIZE >= XMAX) {
        p->x = XMAX - PARTICLE_SIZE;
        p->dx = -p->dx * damping;
    }

    // Top/Bottom walls
    if (p->y <= YMIN + 1) {
        p->y = YMIN + 1;
        p->dy = -p->dy * damping;
    } else if (p->y + PARTICLE_SIZE >= YMAX) {
        p->y = YMAX - PARTICLE_SIZE;
        p->dy = -p->dy * damping;
    }
}

void check_particle_collisions(float damping) {
    for (uint8_t i = 0; i < active_particles; i++) {
        for (uint8_t j = i + 1; j < active_particles; j++) {
            particle_t *p1 = &particles[i];
            particle_t *p2 = &particles[j];

            // Calculate distance between particle centers
            float dx = (p1->x + PARTICLE_SIZE/2.0) - (p2->x + PARTICLE_SIZE/2.0);
            float dy = (p1->y + PARTICLE_SIZE/2.0) - (p2->y + PARTICLE_SIZE/2.0);
            float distance = sqrtf(dx*dx + dy*dy);
            float min_distance = PARTICLE_SIZE;

            // Check if particles are colliding
            if (distance < min_distance && distance > 0.1) {
                // Normalize collision vector
                float nx = dx / distance;
                float ny = dy / distance;

                // Relative velocity
                float dvx = p1->dx - p2->dx;
                float dvy = p1->dy - p2->dy;

                // Relative velocity along collision normal
                float dot_product = dvx * nx + dvy * ny;

                // Don't process if particles are moving away from each other
                if (dot_product > 0) continue;

                const float p1_mass = 1.0; // Assuming equal mass for simplicity
                const float p2_mass = 1.0; // Assuming equal mass for simplicity

                // Elastic collision response
                float impulse = 2.0 * dot_product / (p1_mass + p2_mass);
                impulse *= damping;

                // Update velocities
                p1->dx -= impulse * p2_mass * nx;
                p1->dy -= impulse * p2_mass * ny;
                p2->dx += impulse * p1_mass * nx;
                p2->dy += impulse * p1_mass * ny;

                // Separate particles to prevent overlap
                float overlap = min_distance - distance;
                float separation_x = nx * overlap * 0.5;
                float separation_y = ny * overlap * 0.5;

                p1->x += separation_x;
                p1->y += separation_y;
                p2->x -= separation_x;
                p2->y -= separation_y;
            }
        }
    }
}


void render(void) {
    const uint8_t wpm = get_current_wpm();

    const uint32_t time = timer_read32();
    if (wpm > 0) {
        idle_time = time;
    }


    if (time - idle_time > OLED_TIMEOUT_AUX) {
        return;
    }
    // timer++;

    draw_outline();

    if (!initialized) {
        init_particles();
        initialized = true;
    }

    // Erase all particles at old positions
    for (uint8_t i = 0; i < active_particles; i++) {
        draw_particle((uint8_t)particles[i].x, (uint8_t)particles[i].y, false);
    }

    // if (timer > 10) {
    //     if (TIMER_DIFF_32(time, idle_time) > 120000 && active_particles > 0) {
    //         active_particles--;
    //         timer = 0;
    //     }
    //     else if (wpm > 0 && active_particles < MAX_PARTICLES) {
    //         active_particles++;
    //         timer = 0;
    //     }
    // }

    // float damping = 0.9f + (wpm / 300.0f); // Damping factor based on WPM
    float avg_velocity = 0.f;
    for (uint8_t i = 0; i < active_particles; i++) {
        avg_velocity += sqrtf(particles[i].dx * particles[i].dx + particles[i].dy * particles[i].dy);
    };


    avg_velocity /= active_particles;

    const float desired_velocity = (wpm/50.f) + 1.f;
    float damping = (desired_velocity - avg_velocity) * 0.3f + 1.f;

    // Update particle positions
    for (uint8_t i = 0; i < active_particles; i++) {
        particles[i].x += particles[i].dx;
        particles[i].y += particles[i].dy;
    }

    // Check and resolve particle-to-particle collisions
    check_particle_collisions(damping);

    // Check and resolve wall collisions
    for (uint8_t i = 0; i < active_particles; i++) {
        check_wall_collisions(&particles[i], damping);
    }

    // Draw all particles at new positions
    for (uint8_t i = 0; i < active_particles; i++) {
        draw_particle((uint8_t)particles[i].x, (uint8_t)particles[i].y, true);
    }

    oled_set_cursor(1, 14);
    // oled_write_P(PSTR("WPM"), false);
    oled_write(get_u8_str(desired_velocity*100, '0'), false);
    oled_set_cursor(1, 15);
    oled_write(get_u8_str(avg_velocity*100, '0'), false);
}

bool oled_task_kb(void) {
    render();
    return false;
}
