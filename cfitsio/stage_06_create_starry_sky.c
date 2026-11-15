#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h> /* Required for exp() and pow() */
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;

    srand((unsigned int)time(NULL));

    /* 1. Create a fresh FITS file */
    if (fits_create_file(&fptr, "!stage_06_create_starry_sky.fits", &status)) {
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Empty Primary HDU (Mandatory layout marker) */
    int primary_bitpix = 8;
    int primary_naxis = 0;
    long primary_naxes[] = {0};
    fits_create_img(fptr, primary_bitpix, primary_naxis, primary_naxes, &status);

    /* 3. Define the Starry Sky Image Dimensions (256 x 256) */
    int bitpix = FLOAT_IMG;
    int naxis = 2;
    long naxes[] = {256, 256};
    long npixels = naxes[0] * naxes[1];

    if (fits_create_img(fptr, bitpix, naxis, naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    fits_write_key(fptr, TSTRING, "EXTNAME", "STARRY_SKY", "Simulation Layer", &status);

    /* 4. Allocate memory buffer */
    float *pixels = (float *)malloc(npixels * sizeof(float));
    if (pixels == NULL) {
        printf("Memory allocation failed!\n");
        fits_close_file(fptr, &status);
        return 1;
    }

    /* 5. Generate faint background sky noise (floor range: 0.00 to 0.05) */
    for (long i = 0; i < npixels; i++) {
        pixels[i] = ((float)rand() / (float)RAND_MAX) * 0.05f;
    }

    /* 6. Inject 50 Random Stars using a Gaussian Point Spread Function (PSF) */
    int num_stars = 50;
    for (int s = 0; s < num_stars; s++) {
        // Random center coordinate for the star
        float center_x = (float)(rand() % naxes[0]);
        float center_y = (float)(rand() % naxes[1]);

        // Random peak intensity (brightness) and core width (sigma)
        float peak_flux = 0.5f + ((float)rand() / (float)RAND_MAX) * 2.5f; // 0.5 to 3.0
        float sigma = 0.8f + ((float)rand() / (float)RAND_MAX) * 1.2f;     // 0.8 to 2.0 pixels wide

        // To optimize performance, only loop through a tight bounding box around the star
        int box_radius = (int)(sigma * 4); // 4-sigma coverage captures 99.9% of the star light
        int start_x = (int)center_x - box_radius;
        int end_x   = (int)center_x + box_radius;
        int start_y = (int)center_y - box_radius;
        int end_y   = (int)center_y + box_radius;

        // Keep bounding box within boundaries of our pixel array grid
        if (start_x < 0) start_x = 0;
        if (end_x >= naxes[0]) end_x = naxes[0] - 1;
        if (start_y < 0) start_y = 0;
        if (end_y >= naxes[1]) end_y = naxes[1] - 1;

        for (int y = start_y; y <= end_y; y++) {
            for (int x = start_x; x <= end_x; x++) {
                // Calculate distance squared from pixel center to star center
                float dx = (float)x - center_x;
                float dy = (float)y - center_y;
                float dist_sq = (dx * dx) + (dy * dy);

                // Compute 2D Gaussian light distribution profile
                float star_profile = peak_flux * expf(-dist_sq / (2.0f * sigma * sigma));

                // Add the star profile onto our background noise layer
                long pixel_index = (y * naxes[0]) + x;
                pixels[pixel_index] += star_profile;
            }
        }
    }

    /* 7. Write the starry pixel matrix to the FITS array */
    long firstelem = 1;
    if (fits_write_img(fptr, TFLOAT, firstelem, npixels, pixels, &status)) {
        fits_report_error(stderr, status);
    } else {
        printf("Successfully generated 'stage_06_create_starry_sky.fits' with 50 Gaussian stars!\n");
    }

    /* 8. Clean up and close handles safely */
    free(pixels);
    fits_close_file(fptr, &status);

    if (status) {
        fits_report_error(stderr, status);
    }

    return status;
}

