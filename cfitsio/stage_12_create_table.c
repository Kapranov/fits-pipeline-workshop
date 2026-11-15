#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;

    /* Seed the random number generator */
    srand((unsigned int)time(NULL));

    /* 1. Create a fresh file (Forces overwrite if it already exists) */
    if (fits_create_file(&fptr, "!stage_07_create_starry_sky_header.fits", &status)) {
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Primary HDU (Blank baseline structural marker to prevent error 252) */
    int primary_bitpix = 8;
    int primary_naxis = 0;
    long primary_naxes[] = {0};
    fits_create_img(fptr, primary_bitpix, primary_naxis, primary_naxes, &status);

    /* 3. Build the Binary Table Extension Layout Structure ("STAR_DATA")
       We add columns for ID, Name, X coordinate, Y coordinate, and Peak Flux */
    int tfields = 5;
    long nrows = 0;
    char *ttype[] = {"ID", "NAME", "X_POS", "Y_POS", "FLUX"};
    char *tform[] = {"1J", "20A",  "1E",    "1E",    "1E"};
    char *tunit[] = {"count", "none", "pixel", "pixel", "jansky"};

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

    /* 5. Inject Astronomical Header Metrics (For your reading program to find) */
    int nstars = 50;
    double exptime = 300.0;
    char filter[] = "V";
    char telescope[] = "Virtual-Sim-Telescope";

    fits_write_key(fptr, TINT, "NSTARS", &nstars, "Number of simulated stars", &status);
    fits_write_key(fptr, TDOUBLE, "EXPTIME", &exptime, "Total exposure time in seconds", &status);
    fits_write_key(fptr, TSTRING, "FILTER", filter, "Optical filter band name", &status);
    fits_write_key(fptr, TSTRING, "TELESCOP", telescope, "Observational facility source", &status);
    fits_write_comment(fptr, "Synthetic celestial scene generation test file.", &status);

    /* 6. Allocate memory buffer and generate background sky noise */
    float *pixels = (float *)malloc(npixels * sizeof(float));
    if (pixels == NULL) {
        printf("Memory allocation failed!\n");
        fits_close_file(fptr, &status);
        return 1;
    }

    for (long i = 0; i < npixels; i++) {
        pixels[i] = ((float)rand() / (float)RAND_MAX) * 0.05f;
    }

    /* 7. Generate 50 Random Stars AND simultaneously write them to the Binary Table */
    for (int s = 0; s < nstars; s++) {
        float center_x = (float)(rand() % naxes[0]);
        float center_y = (float)(rand() % naxes[1]);
        float peak_flux = 0.5f + ((float)rand() / (float)RAND_MAX) * 2.5f;
        float sigma = 0.8f + ((float)rand() / (float)RAND_MAX) * 1.2f;

        // Render Gaussian Profile onto the pixel array grid buffer
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

        /* --- Binary Table Logging --- */
        // Temporarily switch context to the table HDU to log the values
        fits_movnam_hdu(fptr, BINARY_TBL, "STAR_DATA", 0, &status);

        int row = s + 1; // 1-based index row position
        int star_id = row;
        float fits_x = center_x + 1.0f; // Convert to FITS 1-based system
        float fits_y = center_y + 1.0f;
        char star_name[20];
        sprintf(star_name, "INJECTED_STAR_%02d", row);

        // Write row data into respective columns
        fits_write_col(fptr, TINT,    1, row, 1, 1, &star_id,   &status);
        char *name_ptr = star_name;
        fits_write_col(fptr, TSTRING, 2, row, 1, 1, &name_ptr, &status);
        fits_write_col(fptr, TFLOAT,  3, row, 1, 1, &fits_x,    &status);
        fits_write_col(fptr, TFLOAT,  4, row, 1, 1, &fits_y,    &status);
        fits_write_col(fptr, TFLOAT,  5, row, 1, 1, &peak_flux, &status);

        // Switch context back to the image HDU for the next loop iteration
        fits_movnam_hdu(fptr, IMAGE_HDU, "STARRY_SKY", 0, &status);
    }

    /* 8. Write finished pixel matrix to the image extension */
    long firstelem_img[] = {1, 1};
    fits_write_pix(fptr, TFLOAT, firstelem_img, npixels, pixels, &status);

    /* 9. Clean up memory allocations and safely close the file hook handle */
    free(pixels);
    fits_close_file(fptr, &status);

    if (status) {
        printf("An error occurred during final processing steps.\n");
        fits_report_error(stderr, status);
    } else {
        printf("================================================================\n");
        printf(" SUCCESS: Completed Star Generation and Catalog Initialization!\n");
        printf(" File generated: stage_07_create_starry_sky_header.fits\n");
        printf(" 'STAR_DATA' Binary Table extension contains all 50 star rows.\n");
        printf("================================================================\n");
    }

    return status;
}
