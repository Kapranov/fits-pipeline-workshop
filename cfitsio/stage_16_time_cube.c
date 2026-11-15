#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;

    srand((unsigned int)time(NULL));

    /* 1. Create a fresh FITS file */
    if (fits_create_file(&fptr, "!stage_07_create_starry_sky_header.fits", &status)) {
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Primary HDU Layout Marker */
    int primary_bitpix = 8;
    int primary_naxis = 0;
    long primary_naxes[] = {0};
    fits_create_img(fptr, primary_bitpix, primary_naxis, primary_naxes, &status);

    /* 3. Define a 3D Image Cube Extension (256 columns x 256 rows x 5 time frames) */
    int bitpix = FLOAT_IMG;
    int naxis = 3;                  /* FIXED: Changed to 3 dimensions */
    long naxes[] = {256, 256, 5}; /* Width, Height, Time Frames */
    long npixels_frame = naxes[0] * naxes[1];
    long total_pixels = npixels_frame * naxes[2];

    if (fits_create_img(fptr, bitpix, naxis, naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }
    fits_write_key(fptr, TSTRING, "EXTNAME", "STARRY_SKY", "Time Series Simulation", &status);

    /* 4. Allocate memory buffer for the entire 3D array cube */
    float *cube_pixels = (float *)malloc(total_pixels * sizeof(float));
    if (cube_pixels == NULL) {
        printf("Memory allocation failed!\n");
        fits_close_file(fptr, &status);
        return 1;
    }

    /* 5. Pre-generate stable baseline positions for 50 tracking targets */
    int nstars = 50;
    float base_x[50], base_y[50], base_flux[50], base_sigma[50];
    for (int s = 0; s < nstars; s++) {
        base_x[s] = (float)(rand() % naxes[0]);
        base_y[s] = (float)(rand() % naxes[1]);
        base_flux[s] = 0.5f + ((float)rand() / (float)RAND_MAX) * 2.5f;
        base_sigma[s] = 0.8f + ((float)rand() / (float)RAND_MAX) * 1.2f;
    }

    /* 6. Render the 3D data slices loop sequentially */
    for (int frame = 0; frame < naxes[2]; frame++) {
        long frame_offset = frame * npixels_frame;

        // Apply background sky glow noise floor for this frame layer
        for (long i = 0; i < npixels_frame; i++) {
            cube_pixels[frame_offset + i] = ((float)rand() / (float)RAND_MAX) * 0.05f;
        }

        // Inject stars with minor variations simulating atmospheric twinkling
        for (int s = 0; s < nstars; s++) {
            // Modulate brightness intensity up or down by up to 20% per frame
            float twist_flux = base_flux[s] * (1.0f + (((float)rand() / (float)RAND_MAX) * 0.4f - 0.2f));

            // Jitter the positions slightly by a fraction of a pixel (sub-pixel drift)
            float jitter_x = base_x[s] + (((float)rand() / (float)RAND_MAX) * 0.4f - 0.2f);
            float jitter_y = base_y[s] + (((float)rand() / (float)RAND_MAX) * 0.4f - 0.2f);

            int box_radius = (int)(base_sigma[s] * 4);
            int start_x = (int)jitter_x - box_radius;
            int end_x   = (int)jitter_x + box_radius;
            int start_y = (int)jitter_y - box_radius;
            int end_y   = (int)jitter_y + box_radius;

            if (start_x < 0) start_x = 0;
            if (end_x >= naxes[0]) end_x = naxes[0] - 1;
            if (start_y < 0) start_y = 0;
            if (end_y >= naxes[1]) end_y = naxes[1] - 1;

            for (int y = start_y; y <= end_y; y++) {
                for (int x = start_x; x <= end_x; x++) {
                    float dx = (float)x - jitter_x;
                    float dy = (float)y - jitter_y;
                    float dist_sq = (dx * dx) + (dy * dy);
                    float profile = twist_flux * expf(-dist_sq / (2.0f * base_sigma[s] * base_sigma[s]));

                    cube_pixels[frame_offset + (y * naxes[0]) + x] += profile;
                }
            }
        }
    }

    /* 7. Write the entire continuous 3D data cube to disk storage */
    long firstelem_cube[] = {1, 1, 1}; /* FIXED: Passed 3 index pointers matching standard array rules */
    if (fits_write_pix(fptr, TFLOAT, firstelem_cube, total_pixels, cube_pixels, &status)) {
        fits_report_error(stderr, status);
    } else {
        printf("SUCCESS: Generated a 3D time-series image cube (256x256x5 frames)!\n");
    }

    /* 8. Safe shutdown processing */
    free(cube_pixels);
    fits_close_file(fptr, &status);
    return status;
}

