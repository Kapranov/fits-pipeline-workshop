#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;

    /* 1. Open the simulated sky file */
    if (fits_open_file(&fptr, "stage_07_create_starry_sky_header.fits", READONLY, &status)) {
        printf("Error: Could not open file.\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Move to the STARRY_SKY extension */
    if (fits_movnam_hdu(fptr, IMAGE_HDU, "STARRY_SKY", 0, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 3. Read image dimensions */
    int bitpix, naxis;
    long naxes[2] = {0, 0};
    if (fits_get_img_param(fptr, 2, &bitpix, &naxis, naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    long width = naxes[0];
    long height = naxes[1];
    long npixels = width * height;

    /* 4. Allocate memory to load the entire pixel array */
    float *pixels = (float *)malloc(npixels * sizeof(float));
    if (pixels == NULL) {
        printf("Memory allocation failed!\n");
        fits_close_file(fptr, &status);
        return 1;
    }

    /* 5. Read all pixels at once from disk into memory */
    long firstelem[2] = {1, 1};  /* FIXED: Initialize starting coordinates for both X and Y axes */
    float nulval = 0.0f;
    int anynul = 0;

    /* Pass the array directly (without the ampersand '&') */
    if (fits_read_pix(fptr, TFLOAT, firstelem, npixels, &nulval, pixels, &anynul, &status)) {
        fits_report_error(stderr, status);
        free(pixels);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 6. Scan the pixel data array - Duplicate loop completely removed */
    float max_brightness = -1.0f;
    long max_x = 0;
    long max_y = 0;
    double total_flux = 0.0;
    int high_intensity_sources = 0;
    float star_threshold = 0.5f;

    for (long y = 0; y < height; y++) {
        for (long x = 0; x < width; x++) {
            long idx = (y * width) + x;
            float val = pixels[idx];

            total_flux += val;

            if (val > max_brightness) {
                max_brightness = val;
                max_x = x + 1; // Convert back to FITS 1-based indexing for display
                max_y = y + 1;
            }

            if (val >= star_threshold) {
                high_intensity_sources++;
            }
        }
    }

    double average_background = total_flux / npixels;

    /* 7. Display the automated scan summary */
    printf("==================================================\n");
    printf("       AUTOMATED FITS IMAGE SCAN COMPLETE         \n");
    printf("==================================================\n");
    printf(" Total Pixels Scanned     : %ld (%ld x %ld)\n", npixels, width, height);
    printf(" Global Average Background: %.4f\n", average_background);
    printf(" Pixels Exceeding Threshold: %d\n", high_intensity_sources);
    printf("--------------------------------------------------\n");
    printf(" Brightest Source Found   : Value = %.4f\n", max_brightness);
    printf(" Target Location Map      : X = %ld, Y = %ld 🌟\n", max_x, max_y);
    printf("==================================================\n");

    /* 8. Free allocations and shut down handles cleanly */
    free(pixels);
    fits_close_file(fptr, &status);
    return status;
}
