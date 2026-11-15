#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;

    srand((unsigned int)time(NULL));

    /* 1. Create a fresh file (Overwrites if it already exists) */
    if (fits_create_file(&fptr, "!stage_12_perfect_simulation.fits", &status)) {
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Primary HDU (Blank structural layout baseline marker) */
    int primary_bitpix = 8;
    int primary_naxis = 0;
    long primary_naxes[] = {0};
    fits_create_img(fptr, primary_bitpix, primary_naxis, primary_naxes, &status);

    /* 3. Build the Binary Table Extension Layout Structure */
    int tfields = 3;
    long nrows = 0;
    char *ttype[] = {"ID", "NAME", "FLUX"};
    char *tform[] = {"1J", "20A", "1E"};
    char *tunit[] = {"count", "none", "jansky"};

    if (fits_create_tbl(fptr, BINARY_TBL, nrows, tfields, ttype, tform, tunit, "STAR_DATA", &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 4. Append the 2D Simulation Image Extension Layout (256x256) */
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

    /* 5. Allocate memory buffer and generate image arrays */
    float *pixels = (float *)malloc(npixels * sizeof(float));
    if (pixels == NULL) {
        printf("Memory allocation failed!\n");
        fits_close_file(fptr, &status);
        return 1;
    }

    // Sky noise floor initialization
    for (long i = 0; i < npixels; i++) {
        pixels[i] = ((float)rand() / (float)RAND_MAX) * 0.05f;
    }

    // Inject 50 Random Stars using a Gaussian PSF profile
    int num_stars = 50;
    for (int s = 0; s < num_stars; s++) {
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

    /* 6. Write the pixels to the image extension */
    long firstelem_img[] = {1, 1};
    fits_write_pix(fptr, TFLOAT, firstelem_img, npixels, pixels, &status);

    /* 7. Move back over to the "STAR_DATA" binary table layer */
    if (fits_movnam_hdu(fptr, BINARY_TBL, "STAR_DATA", 0, &status)) {
        fits_report_error(stderr, status);
        free(pixels);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 8. Scan the pixel matrix and write parameters row-by-row into the table columns */
    float star_threshold = 0.5f;
    int star_count = 0;

    for (long y = 1; y < naxes[1] - 1; y++) {
        for (long x = 1; x < naxes[0] - 1; x++) {
            long idx = (y * naxes[0]) + x;
            float center_val = pixels[idx];

            if (center_val >= star_threshold) {
                if (center_val >= pixels[idx - 1] && center_val >= pixels[idx + 1] &&
                    center_val >= pixels[idx - naxes[0]] && center_val >= pixels[idx + naxes[0]] &&
                    center_val >= pixels[idx - naxes[0] - 1] && center_val >= pixels[idx - naxes[0] + 1] &&
                    center_val >= pixels[idx + naxes[0] - 1] && center_val >= pixels[idx + naxes[0] + 1]) {

                    star_count++;

                    int col_id = star_count;
                    float col_flux = center_val;
                    char col_name[20];
                    sprintf(col_name, "SIM_STAR_%02d", star_count);

                    /* Write records into columns 1, 2, and 3 */
                    fits_write_col(fptr, TINT,    1, star_count, 1, 1, &col_id,   &status);

                    char *name_ptr = col_name;
                    fits_write_col(fptr, TSTRING, 2, star_count, 1, 1, &name_ptr, &status); // FIXED string pointer reference passing layout

                    fits_write_col(fptr, TFLOAT,  3, star_count, 1, 1, &col_flux, &status);
                }
            }
        }
    }

    /* 9. Clean up memory allocations and safely flush updates onto disk storage */
    free(pixels);
    fits_close_file(fptr, &status);

    if (status) {
        printf("An error occurred during final closure processing.\n");
        fits_report_error(stderr, status);
    } else {
        printf("========================================================\n");
        printf(" SUCCESS: Completed all-in-one stellar pipeline run!   \n");
        printf(" File generated: starry_sky_complete_simulation.fits    \n");
        printf(" Binary catalog successfully wrote %d star listings.   \n", star_count);
        printf("========================================================\n");
    }

    return status;
}
