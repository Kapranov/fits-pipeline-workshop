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

    /* 5. Read all pixels at once */
    long firstelem[2] = {1, 1};
    float nulval = 0.0f;
    int anynul = 0;
    if (fits_read_pix(fptr, TFLOAT, firstelem, npixels, &nulval, pixels, &anynul, &status)) {
        fits_report_error(stderr, status);
        free(pixels);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 6. Scan the pixel matrix for Local Maxima */
    float star_threshold = 0.5f; // Threshold to filter background noise
    int star_count = 0;

    printf("==================================================\n");
    printf("        DETECTED STAR CATALOG (LOCAL MAXIMA)       \n");
    printf("==================================================\n");
    printf("  Star #  |   X Coord   |   Y Coord   |  Peak Flux \n");
    printf("--------------------------------------------------\n");

    // Loop through pixels, skipping the 1-pixel outer border to prevent array out-of-bounds checks
    for (long y = 1; y < height - 1; y++) {
        for (long x = 1; x < width - 1; x++) {
            long idx = (y * width) + x;
            float center_val = pixels[idx];

            // Condition 1: Must be brighter than the noise threshold
            if (center_val >= star_threshold) {

                // Condition 2: Must be strictly greater than or equal to all 8 neighbors
                if (center_val >= pixels[idx - 1] &&          // Left
                    center_val >= pixels[idx + 1] &&          // Right
                    center_val >= pixels[idx - width] &&      // Bottom
                    center_val >= pixels[idx + width] &&      // Top
                    center_val >= pixels[idx - width - 1] &&  // Bottom-Left
                    center_val >= pixels[idx - width + 1] &&  // Bottom-Right
                    center_val >= pixels[idx + width - 1] &&  // Top-Left
                    center_val >= pixels[idx + width + 1]) {  // Top-Right

                    star_count++;

                    // Convert standard C 0-indexing to FITS 1-indexing for spatial tracking
                    printf("   %4d   |    %5ld    |    %5ld    |   %8.4f\n", 
                           star_count, x + 1, y + 1, center_val);
                }
            }
        }
    }

    printf("==================================================\n");
    printf(" Total Unique Stars Detected: %d\n", star_count);
    printf("==================================================\n");

    /* 7. Free resources and exit */
    free(pixels);
    fits_close_file(fptr, &status);
    return status;
}
