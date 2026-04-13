#include <stdint.h>
#include "config.h"
#include "lib/lib8tion/lib8tion.h"
#include "lib/lib8tion/math8.h"
#include "lib/lib8tion/random8.h"
#include "oled_driver.h"
#include "quantum.h"
#include "timer.h"
#include "util.h"
#ifdef WPM_ENABLE
#    include "wpm.h"
#endif

#include "custom_keys.h"

// Ceil division
#define CDIV(x, y) (uint8_t)(1 + (x-1) / y)

oled_rotation_t oled_init_kb(oled_rotation_t rotation) {
    return OLED_ROTATION_270;
}

#define XMIN 0
#define XMAX 31
#define YMIN 0
#define YMAX 127
// #define YMAX (111 - 4)

#define BORDER_LEFT   (XMIN-1)
#define BORDER_RIGHT  (XMAX+1)
#define BORDER_TOP    (YMIN-1)
#define BORDER_BOTTOM (YMAX+1)


#if (XMAX > 31) || (XMIN < 0) || (YMAX > 127) || (YMIN < 0)
    #error "Screen bounds exceeded"
#endif

#define NUM_PARTICLES 10
// #define NUM_PARTICLES 14
#define PARTICLE_SIZE 5

typedef int16_t q88_t;
#define Q88_FRAC_BITS 8
#define Q88_ONE (1 << Q88_FRAC_BITS)
#define Q88_HALF (Q88_ONE >> 1)

#define FLOAT_TO_Q88(f) ((q88_t)((f) * Q88_ONE))
#define Q88_TO_FLOAT(q) ((float)(q) / Q88_ONE)
#define INT_TO_Q88(i) ((q88_t)((i) << Q88_FRAC_BITS))
#define Q88_INT(q) ((int8_t)((q) >> Q88_FRAC_BITS))
#define Q88_FRAC(q) ((uint8_t)((q) & 0xFF))
#define Q88_MUL(a, b) (((int32_t)(a) * (int32_t)(b)) >> Q88_FRAC_BITS)
#define Q88_DIV(a, b) (((int32_t)(a) << Q88_FRAC_BITS) / (b))
#define Q88_SQ(a) Q88_MUL(a, a)
#define Q88_SQRT(a) (sqrt16(a) << 4)

typedef struct {
    q88_t x;
    q88_t y;
    q88_t dx;
    q88_t dy;
} particle_t;

static particle_t particles[NUM_PARTICLES];

static bool initialized = false;
static uint32_t idle_time = 0;
static bool prev_oled_state = true;

#define MIN_DISTANCE (INT_TO_Q88(PARTICLE_SIZE))
#define MIN_DISTANCE_SQ (INT_TO_Q88(PARTICLE_SIZE * PARTICLE_SIZE))
#define MIN_DISTANCE_THRESHOLD (FLOAT_TO_Q88(0.1f))
#define MIN_DISTANCE_SQ_THRESHOLD (Q88_SQ(FLOAT_TO_Q88(0.1f)))
// #define MIN_DISTANCE_SQ_THRESHOLD (Q88_SQ(MIN_DISTANCE_THRESHOLD))
#define MAX_DELTA (INT_TO_Q88(10))
// #define MAX_NORMALIZED (Q88_ONE + (Q88_ONE >> 2))  // 1.25 in Q8.8

#define WALL_XMIN (INT_TO_Q88(XMIN))
#define WALL_XMAX (INT_TO_Q88(XMAX - PARTICLE_SIZE + 1))
#define WALL_YMIN (INT_TO_Q88(YMIN))
#define WALL_YMAX (INT_TO_Q88(YMAX - PARTICLE_SIZE + 1))

void draw_outline(void) {
    for (uint8_t x = BORDER_LEFT+1; x <= BORDER_RIGHT-1; x++) { // Top and bottom borders
#if BORDER_TOP >= 0
        uint8_t sft_top = BORDER_TOP % 8;
        uint16_t idx_top = (BORDER_TOP / 8)*32 + x;
        oled_write_raw_byte(1 << sft_top, idx_top);
#endif
#if BORDER_BOTTOM <= 127
        uint8_t sft_bot = BORDER_BOTTOM % 8;
        uint16_t idx_bot = (BORDER_BOTTOM / 8)*32 + x;
        oled_write_raw_byte(1 << sft_bot, idx_bot);
#endif
    }

    for (uint8_t y = BORDER_TOP/8 + 1; y <= BORDER_BOTTOM/8 - 1; y++) { // Left and right borders
#if BORDER_LEFT >= 0
        uint16_t idx_lft = y*32 + BORDER_LEFT;
        oled_write_raw_byte(255, idx_lft);
#endif
#if BORDER_RIGHT <= 31
        uint16_t idx_rgt = y*32 + BORDER_RIGHT;
        oled_write_raw_byte(255, idx_rgt);
#endif
    }
    // Draw final segments vertical segments
#if BORDER_LEFT >= 0
    oled_write_raw_byte(~((1<<MAX(BORDER_TOP, 0)%8)-1), MAX(BORDER_TOP, 0)/8*32 + BORDER_LEFT); // Top
    oled_write_raw_byte(0xFF>>(7-MIN(BORDER_BOTTOM, 127)%8), (MIN(BORDER_BOTTOM, 127)/8)*32 + BORDER_LEFT); // Bottom
#endif
#if BORDER_RIGHT <= 31
    oled_write_raw_byte(~((1<<MAX(BORDER_TOP, 0)%8)-1), MAX(BORDER_TOP, 0)/8*32 + BORDER_RIGHT); // Top
    oled_write_raw_byte(0xFF>>(7-MIN(BORDER_BOTTOM, 127)%8), (MIN(BORDER_BOTTOM, 127)/8)*32 + BORDER_RIGHT); // Bottom
#endif
}

void init_particles(void) {
    for (uint8_t i = 0; i < NUM_PARTICLES; i++) {
        q88_t x = INT_TO_Q88((random8() % (XMAX - XMIN)) + XMIN);
        q88_t y = INT_TO_Q88((random8() % (YMAX - YMIN)) + YMIN);
        particles[i].x = x;
        particles[i].y = y;

        particles[i].dx = (q88_t)(((int16_t)random8() - 127) * 3);
        particles[i].dy = (q88_t)(((int16_t)random8() - 127) * 3);
    }
}

void draw_particle(q88_t qx, q88_t qy, bool on) {
    uint8_t x = Q88_INT(qx);
    uint8_t y = Q88_INT(qy);
    uint8_t x_end = x + PARTICLE_SIZE;
    if (x_end > XMAX + 1) x_end = XMAX + 1;
    uint8_t y_end = y + PARTICLE_SIZE;
    if (y_end > YMAX + 1) y_end = YMAX + 1;

    for (uint8_t i = x; i < x_end; i++) {
        for (uint8_t j = y; j < y_end; j++) {
            uint8_t sft = j % 8;
            uint16_t idx = (j / 8) * 32 + i;
            uint8_t prev = oled_read_raw_byte(idx);
            if (on) {
                if ((i-x == 1 || i-x == 3) && (j-y == 1 || j-y == 2))
                    oled_write_raw_byte(prev & ~(1 << sft), idx);
                else
                    oled_write_raw_byte(prev | (1 << sft), idx);
            } else {
                if (i % 2 == 0 && j % 2 == 0)
                    oled_write_raw_byte(prev | (1 << sft), idx);
                else
                    oled_write_raw_byte(prev & ~(1 << sft), idx);
            }
        }
    }
}


static const uint16_t face[] = { // Ghost
    0b111110,
    0b010011,
    0b111111,
    0b010011,
    0b111110,
};

// static const uint16_t face[] = { // Circle
//     0b01110,
//     0b11111,
//     0b11111,
//     0b11111,
//     0b01110,
// };

// const uint16_t *face = ghost;

void draw_particle_filled(q88_t qx, q88_t qy) {
    uint8_t x0 = Q88_INT(qx);
    uint8_t y = Q88_INT(qy);
    for (uint8_t x = x0; x < x0 + PARTICLE_SIZE; x++) {

        uint8_t sft = y % 8;
        uint8_t yblk = y / 8;
        // uint8_t yblk_end = (y+PARTICLE_SIZE-1) / 8;

        uint16_t idx = yblk*32 + x;

        uint8_t prev = oled_read_raw_byte(idx);
        uint16_t mask = face[x-x0] << sft;
        // uint16_t mask = ((1<<PARTICLE_SIZE) - 1) << sft;
        oled_write_raw_byte(prev | mask, idx);


        uint16_t idx_end = idx + 32;
        if (idx_end < OLED_MATRIX_SIZE) {
            prev = oled_read_raw_byte(idx_end);
            oled_write_raw_byte(prev | (mask >> 8), idx_end);
        }
    }
}

void check_wall_collisions(particle_t *p, q88_t damping) {
    if (p->x <= WALL_XMIN) {
        p->x = WALL_XMIN;
        p->dx = Q88_MUL(-p->dx, damping);
    } else if (p->x >= WALL_XMAX) {
        p->x = WALL_XMAX;
        p->dx = Q88_MUL(-p->dx, damping);
    }

    if (p->y <= INT_TO_Q88(YMIN)) {
        p->y = INT_TO_Q88(YMIN);
        p->dy = Q88_MUL(-p->dy, damping);
    } else if (p->y >= WALL_YMAX) {
        p->y = WALL_YMAX;
        p->dy = Q88_MUL(-p->dy, damping);
    }
}

void check_particle_collisions(q88_t damping) {
    for (uint8_t i = 0; i < NUM_PARTICLES; i++) {
        particle_t *p1 = &particles[i];

        for (uint8_t j = i + 1; j < NUM_PARTICLES; j++) {

            particle_t *p2 = &particles[j];

            q88_t dx = p1->x - p2->x;
            q88_t dy = p1->y - p2->y;

            // Early exit: check bounds before expensive calculations
            if (dx > MAX_DELTA || dx < -MAX_DELTA || dy > MAX_DELTA || dy < -MAX_DELTA) {
                continue;
            }

            q88_t distance_sq = Q88_SQ(dx) + Q88_SQ(dy);

            // if (distance_sq < MIN_DISTANCE_SQ_THRESHOLD || distance_sq >= MIN_DISTANCE_SQ) {
            //     continue;
            // }

            // Only compute sqrt if we pass the squared distance check
            q88_t distance = Q88_SQRT(distance_sq);
            if (distance == 0 || distance > MIN_DISTANCE) {
                continue;
            }

            // Normalize collision vector
            q88_t nx = Q88_DIV(dx, distance);
            q88_t ny = Q88_DIV(dy, distance);

            // Safety check: normalized vectors should be in reasonable range
            // if (
            //     nx > MAX_NORMALIZED || nx < -MAX_NORMALIZED
            //     || ny > MAX_NORMALIZED || ny < -MAX_NORMALIZED
            // ) {
            //     continue;
            // }

            // Relative velocity
            q88_t dvx = p1->dx - p2->dx;
            q88_t dvy = p1->dy - p2->dy;

            // Relative velocity along collision normal
            q88_t dot_product = Q88_MUL(dvx, nx) + Q88_MUL(dvy, ny);

            // Elastic collision response
            q88_t impulse = Q88_MUL(dot_product, damping);

            // Update velocities
            q88_t impulse_nx = Q88_MUL(impulse, nx);
            q88_t impulse_ny = Q88_MUL(impulse, ny);
            p1->dx = p1->dx - impulse_nx;
            p1->dy = p1->dy - impulse_ny;
            p2->dx = p2->dx + impulse_nx;
            p2->dy = p2->dy + impulse_ny;

            // Separate particles to prevent overlap
            q88_t overlap = MIN_DISTANCE - distance;
            q88_t separation_half = Q88_MUL(overlap, Q88_HALF);
            q88_t separation_x = Q88_MUL(nx, separation_half);
            q88_t separation_y = Q88_MUL(ny, separation_half);

            p1->x = p1->x + separation_x;
            p1->y = p1->y + separation_y;
            p2->x = p2->x - separation_x;
            p2->y = p2->y - separation_y;
        }
    }
}


void render(void) {
#ifdef WPM_ENABLE
    const uint8_t wpm = get_current_wpm();
#else
    const uint8_t wpm = 0;
#endif
    const bool oled_on = is_oled_on();
    const uint32_t time = timer_read32();

    if (wpm > 0 || (oled_on && !prev_oled_state)) {
        idle_time = time;
    }

    prev_oled_state = oled_on;

    if (!oled_on || (time - idle_time > OLED_TIMEOUT_AUX)) {
        return;
    }


    if (!initialized) {
        // draw_outline();
        init_particles();
        initialized = true;
    }

    oled_clear();
    // draw_outline();

    // Erase all particles at old positions
    // for (uint8_t i = 0; i < NUM_PARTICLES; i++) {
    //     draw_particle(particles[i].x, particles[i].y, false);
    // }

    // Calculate average velocity (optimized: cache NUM_PARTICLES conversion)
    q88_t avg_velocity = 0;
    q88_t num_particles_q = INT_TO_Q88(NUM_PARTICLES);
    for (uint8_t i = 0; i < NUM_PARTICLES; i++) {
        q88_t vel_sq = Q88_SQ(particles[i].dx) + Q88_SQ(particles[i].dy);
        avg_velocity = avg_velocity + Q88_SQRT(vel_sq);
    }
    avg_velocity = Q88_DIV(avg_velocity, num_particles_q);

    // desired_velocity = (wpm/50) + 0.6
    q88_t desired_velocity = Q88_DIV(INT_TO_Q88(wpm), INT_TO_Q88(50)) + FLOAT_TO_Q88(0.6f);
    if (wpm > 0) {
        desired_velocity = desired_velocity + FLOAT_TO_Q88(0.9f);
    }
    // damping = MAX((desired_velocity - avg_velocity) * 0.2 + 1, 0.95)
    q88_t damping = Q88_MUL(desired_velocity - avg_velocity, FLOAT_TO_Q88(0.2f)) + Q88_ONE;
    q88_t min_damping = FLOAT_TO_Q88(0.95f);
    if (damping < min_damping) {
        damping = min_damping;
    }

// #ifdef WPM_ENABLE
//     oled_set_cursor(1, 14);
//     // oled_write_P(PSTR("WPM"), false);
//     oled_write(get_u8_str(Q88_INT(desired_velocity), '0'), false);
//     oled_set_cursor(1, 15);
//     // oled_write(get_u8_str(wpm, '0'), false);
//     oled_write(get_u8_str(Q88_INT(avg_velocity), '0'), false);
// #endif

    // Update particle positions (optimized: direct addition)
    for (uint8_t i = 0; i < NUM_PARTICLES; i++) {
        particles[i].x = particles[i].x + particles[i].dx;
        particles[i].y = particles[i].y + particles[i].dy;
    }

    check_particle_collisions(damping);

    for (uint8_t i = 0; i < NUM_PARTICLES; i++) {
        // check_wall_collisions(&particles[i], MIN(damping, 1.01f));
        check_wall_collisions(&particles[i], damping);
    }

    for (uint8_t i = 0; i < NUM_PARTICLES; i++) {
        draw_particle_filled(particles[i].x, particles[i].y);
    }

}

bool oled_task_kb(void) {
    render();
    return false;
}
