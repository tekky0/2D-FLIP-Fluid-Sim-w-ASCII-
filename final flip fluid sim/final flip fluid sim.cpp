﻿#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <corecrt_math.h>
#include <windows.h>
// My first ever fluid sim!

// Definitions
// Grid parameters
#define grid_width 100
#define grid_height 100
// Number of particles in grid
#define particle_num 1500

// Gotta define particle blueprint
typedef struct {
    float x, y;
    float vx, vy;
} Particle;

// Now I'll define the grid's structure blueprints such as its x and y velocity components
typedef struct {
    float u[grid_width][grid_height];
    float v[grid_width][grid_height];
    float pressure[grid_width][grid_height];
} Grid;

Particle particle[particle_num];

// Now we spawn in the particles
void spawn_particles() {
    srand(time(NULL)); // Seed for randomness
    for (int i = 0; i < particle_num; i++) {
        particle[i].x = rand() % grid_width;
        particle[i].y = rand() % grid_height;
        particle[i].vx = ((rand() % 200) - 100) / 100.0f; // Random velocity
        particle[i].vy = ((rand() % 200) - 100) / 100.0f;
    }
}


Grid grid;
void gridset() {
    for (int iw = 0; iw < grid_width; iw++) {
        for (int ih = 0; ih < grid_height; ih++) {
            grid.u[iw][ih] = 0.0f;
            grid.v[iw][ih] = 0.0f;
            grid.pressure[iw][ih] = 0.0f;
        }
    }
}

void particle2grid() {
    float weight[grid_width][grid_height] = { 0 };
    // Reset grid
    for (int i = 0; i < grid_width; i++) {
        for (int j = 0; j < grid_height; j++) {
            grid.u[i][j] = 0.0f;
            grid.v[i][j] = 0.0f;
            weight[i][j] = 0.0f;
        }
    }

    for (int p = 0; p < particle_num; p++) {
        Particle* pt = &particle[p];
        // Find the grids the particles are in
        int i = (int)pt->x;
        int j = (int)pt->y;
        // Find the weight ratio to prep particle for bilinear interpolation
        float fx = pt->x - i;
        float fy = pt->y - j;

        for (int di = 0; di < 2; di++) {
            for (int dj = 0; dj < 2; dj++) {
                int cellX = di + i;
                int cellY = dj + j;
                float w = (di ? fx : (1 - fx)) * (dj ? fy : (1 - fy));

                if (cellX >= 0 && cellX < grid_width && cellY >= 0 && cellY < grid_height) {
                    grid.u[cellX][cellY] += pt->vx * w;
                    grid.v[cellX][cellY] += pt->vy * w;
                    weight[cellX][cellY] += w;
                }
            }
        }
    }
    for (int i = 0; i < grid_width; i++) {
        for (int j = 0; j < grid_height; j++) {
            if (weight[i][j] > 0) {
                grid.u[i][j] /= weight[i][j];
                grid.v[i][j] /= weight[i][j];
            }
        }
    }
}

// You also have to define timedelta, I'm going to choose 60fps so 1/60
float dt = 1.0f / 60.0f;

void uravity() {
    for (int i = 0; i < grid_width; i++) {
        for (int h = 0; h < grid_height; h++) {
            grid.v[i][h] += (-9.81f * dt);
        }
    }
}






void pressuresolve(int iterations) {
    // First we solve for divergence on the reference grid
    for (int iter = 0; iter < iterations; iter++) {
        for (int i = 1; i < grid_width - 1; i++) {
            for (int j = 1; j < grid_height - 1; j++) {
                float divergence = (((grid.u[i + 1][j] - grid.u[i - 1][j]) + (grid.v[i][j + 1] - grid.v[i][j - 1])) / 2) * dt;
                grid.pressure[i][j] = (grid.pressure[i + 1][j] + grid.pressure[i - 1][j] + grid.pressure[i][j + 1] + grid.pressure[i][j - 1] - divergence * dt) / 4.0f;


				grid.pressure[i][j] *= 0.5f;
                // I got really confused here but think about it like this, we are getting like, all the grids from up down left and right,
                // and finding the average pressure in all the 4 cells but remember how we got divergence?
                // if it's negative then you'd want all the boxes surrounding to go in, and if it's positive it'll be the other way around.
                // So if referenced cell (i, j) of that time step (dt) is a negative, and you take the net sum of all 4 cells minus the negative cell
                // it will become positive and all the boxes will push particles away and vice versa if divergence on (i,j) is positive.
                // It took me like 4 hours to figure that out I'm going to have a brain aneurism lol
            }
        }
    }
}

void addVorticity() {
    float curl[grid_width][grid_height] = { 0 };

    // Compute curl of velocity field
    for (int i = 1; i < grid_width - 1; i++) {
        for (int j = 1; j < grid_height - 1; j++) {
            float dw_dx = (grid.v[i + 1][j] - grid.v[i - 1][j]) / 2.0f;
            float dv_dy = (grid.u[i][j + 1] - grid.u[i][j - 1]) / 2.0f;
            curl[i][j] = dw_dx - dv_dy;
        }
    }

    // Apply vorticity force
    float vorticity_strength = 0.7f;
    for (int i = 1; i < grid_width - 1; i++) {
        for (int j = 1; j < grid_height - 1; j++) {
            float force = curl[i][j] * vorticity_strength;
            grid.u[i][j] += force * dt;
            grid.v[i][j] += force * dt;
        }
    }
}

void applyViscosity() {
    float viscosity = 0.015f; // Increase for more liquid-like spreading

    for (int i = 1; i < grid_width - 1; i++) {
        for (int j = 1; j < grid_height - 1; j++) {
            grid.u[i][j] = (1 - viscosity) * grid.u[i][j] + viscosity * (grid.u[i + 1][j] + grid.u[i - 1][j]) / 2.0f;
            grid.v[i][j] = (1 - viscosity) * grid.v[i][j] + viscosity * (grid.v[i][j + 1] + grid.v[i][j - 1]) / 2.0f;
        }
    }
}



void subtractPressureGradient() {
    for (int i = 1; i < grid_width - 1; i++) {
        for (int j = 1; j < grid_height - 1; j++) {
            grid.u[i][j] -= dt * (grid.pressure[i + 1][j] - grid.pressure[i - 1][j]) / 2.0f;
            grid.v[i][j] -= dt * (grid.pressure[i][j + 1] - grid.pressure[i][j - 1]) / 2.0f;
            // We are subtracting pressure gradient with respect to its timeframe and subtracting the x and y component
            // velocities to ensure natural change in velocities when taking pressure as a factor into account
        }
    }
}

void gridtoparticle() {
    // Now we finally interpolate the grid velocity components to the particles
    for (int p = 0; p < particle_num; p++) {
        Particle* pt = &particle[p];
        int i = (int)pt->x;
        int j = (int)pt->y;
        float fx = pt->x - i;
        float fy = pt->y - j;
        float nVx = 0.0f, nVy = 0.0f;
        for (int di = 0; di < 2; di++) {
            for (int dj = 0; dj < 2; dj++) {
                int cellX = i + di;
                int cellY = j + dj;
                float w = (di ? fx : (1 - fx)) * (dj ? fy : (1 - fy));

                if (cellX >= 0 && cellX < grid_width && cellY >= 0 && cellY < grid_height) {
                    nVx += grid.u[cellX][cellY] * w;
                    nVy += grid.v[cellX][cellY] * w;
                }

            }
        }
        pt->vx = nVx;
        pt->vy = nVy;
    }
}
void particles_result_displacement() {
    for (int p = 0; p < particle_num; p++) {
        Particle* pt = &particle[p];

        // Apply gravity
        pt->vy += -9.81f * dt;

        // Move particle
        pt->x += pt->vx * dt;
        pt->y += pt->vy * dt;

        // Ground collision
        if (pt->y < 0) {
            pt->y = 0;
            pt->vy = -pt->vy * 0.5f; // Bouncy effect to prevent stacking
            pt->vx *= 0.9f; // Friction
        }

        // Particle-particle collision (better stacking fix)
        float repulsion_strength = 0.75f;
        float repulsion_distance = 1.0f;

        for (int q = 0; q < particle_num; q++) {
            if (p == q) continue;

            Particle* neighbor = &particle[q];
            float dx = pt->x - neighbor->x;
            float dy = pt->y - neighbor->y;
            float dist = sqrt(dx * dx + dy * dy);

            if (dist < repulsion_distance && dist > 0.01f) {
                float overlap = repulsion_distance - dist;

                // Normalize direction
                dx /= dist;
                dy /= dist;

                // Apply repulsion in both x and y
                pt->x += overlap * 0.5f * dx;
                pt->y += overlap * 0.5f * dy;
                neighbor->x -= overlap * 0.5f * dx;
                neighbor->y -= overlap * 0.5f * dy;

                // Add slight random perturbation to break perfect stacking
                float perturbation = ((rand() % 200) - 100) / 500.0f;
                pt->x += perturbation;
                neighbor->x -= perturbation;

                // Damp velocities to reduce jitter
                pt->vx *= 0.95f;
                pt->vy *= 0.95f;
                neighbor->vx *= 0.95f;
                neighbor->vy *= 0.95f;
            }
        }
    }
}



void render_ascii() {
    char buffer[grid_width * grid_height + 1500]; 
    int offset = 0;

    offset += sprintf_s(buffer + offset, grid_width * grid_height + 1000 - offset, "\033[H\033[J"); 

    // Initialize screen with empty dots
    for (int j = grid_height - 1; j >= 0; j--) {
        for (int i = 0; i < grid_width; i++) {
            buffer[offset++] = ' '; 
        }
        buffer[offset++] = '\n'; 
    }

    // Place particles
    for (int p = 0; p < particle_num; p++) {
        Particle* pt = &particle[p];
        int locx = (int)pt->x;
        int locy = (int)pt->y;

        if (locx >= 0 && locx < grid_width && locy >= 0 && locy < grid_height) {
            int index = (grid_height - 1 - locy) * (grid_width + 1) + locx; // Calculate buffer index
            buffer[index] = '*';
        }
    }

    buffer[offset] = '\0'; // Null-terminate the buffer

    puts(buffer); // Print everything in one go
}



int main(void) { 
    spawn_particles();
    gridset();

    for (int i = 0; i < 1000; i++) {
        particle2grid();
        uravity();
        pressuresolve(50);
        addVorticity();
        applyViscosity();
        subtractPressureGradient();
        gridtoparticle();
        particles_result_displacement();
        render_ascii();
        printf("\n")* grid_height;
        Sleep(10);
    }

    return 0;
}
