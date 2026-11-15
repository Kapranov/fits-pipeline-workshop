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

    /* 2. Empty Primary HDU layout marker */
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

    // Name this extension layer
    fits_write_key(fptr, TSTRING, "EXTNAME", "STARRY_SKY", "Simulation Layer", &status);

    /* 4. Inject the Astronomical Header Metrics */
    int nstars = 50;
    double exptime = 300.0;
    char filter[] = "V";
    char telescope[] = "Virtual-Sim-Telescope";

    fits_write_key(fptr, TINT, "NSTARS", &nstars, "Number of simulated stars", &status);
    fits_write_key(fptr, TDOUBLE, "EXPTIME", &exptime, "Total exposure time in seconds", &status);
    fits_write_key(fptr, TSTRING, "FILTER", filter, "Optical filter band name", &status);
    fits_write_key(fptr, TSTRING, "TELESCOP", telescope, "Observational facility source", &status);

    fits_write_comment(fptr, "Synthetic celestial scene generation test file.", &status);
    fits_write_history(fptr, "Gaussian profiles applied to shape the stellar targets.", &status);

    /* 5. Allocate memory buffer */
    float *pixels = (float *)malloc(npixels * sizeof(float));
    if (pixels == NULL) {
        printf("Memory allocation failed!\n");
        fits_close_file(fptr, &status);
        return 1;
    }

    /* 6. Generate sky noise (0.00 to 0.05) */
    for (long i = 0; i < npixels; i++) {
        pixels[i] = ((float)rand() / (float)RAND_MAX) * 0.05f;
    }

    /* 7. Inject 50 Stars using PSF */
    for (int s = 0; s < nstars; s++) {
        float center_x = (float)(rand() % naxes[0]);
        float center_y = (float)(rand() % naxes[1]);
        float peak_flux = 0.5f + ((float)rand() / (float)RAND_MAX) * 2.5f;
        float sigma = 0.8f + ((float)rand() / (float)RAND_MAX) * 1.2f;

        int box_radius = (int)(sigma * 4);
        int start_x = (int)center_x - box_radius;
        int end_x   = (int)center_x + box_radius;
        int start_y = (int)center_y - box_radius;
        int end_y   = (int)center_y + box_radius;

        if (start_x < 0) start_x = 0;
        if (end_x >= naxes[0]) end_x = naxes[0] - 1;
        if (start_y < 0) start_y = 0;
        if (end_y >= naxes[1]) end_y = naxes[1] - 1;

        for (int y = start_y; y <= end_y; y++) {
            for (int x = start_x; x <= end_x; x++) {
                float dx = (float)x - center_x;
                float dy = (float)y - center_y;
                float dist_sq = (dx * dx) + (dy * dy);
                float star_profile = peak_flux * expf(-dist_sq / (2.0f * sigma * sigma));
                pixels[(y * naxes[0]) + x] += star_profile;
            }
        }
    }

    /* 8. Write the pixel matrix to the FITS array */
    long firstelem = 1;
    if (fits_write_img(fptr, TFLOAT, firstelem, npixels, pixels, &status)) {
        fits_report_error(stderr, status);
    } else {
        printf("Successfully generated 'stage_07_create_starry_sky_header.fits' with full metadata headers!\n");
    }

    /* 9. Clean up and close handles safely */
    free(pixels);
    fits_close_file(fptr, &status);

    if (status) {
        fits_report_error(stderr, status);
    }

    return status;
}
