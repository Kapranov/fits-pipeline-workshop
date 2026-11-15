#include <stdio.h>
#include <stdlib.h>
#include <time.h> /* Required to seed the random number generator */
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;

    /* Seed the random number generator so you get different noise every run */
    srand((unsigned int)time(NULL));

    /* 1. Create a fresh file (Bypasses status=104. '!' forces overwrite if it exists) */
    if (fits_create_file(&fptr, "!stage_05_create_noise.fits", &status)) {
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Initialize a clean, empty Primary HDU (Bypasses status=252) */
    int primary_bitpix = 8;
    int primary_naxis = 0;
    long primary_naxes[] = {0};
    if (fits_create_img(fptr, primary_bitpix, primary_naxis, primary_naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 3. Define the new Image Extension details (100x100 pixels) */
    int bitpix = FLOAT_IMG;
    int naxis = 2;
    long naxes[] = {100, 100};

    /* 4. Append the new Image HDU */
    if (fits_create_img(fptr, bitpix, naxis, naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 5. Label this extension layer */
    fits_write_key(fptr, TSTRING, "EXTNAME", "RANDOM_NOISE", "Extension name", &status);

    /* 6. Allocate memory using the fixed calculation */
    long npixels = naxes[0] * naxes[1];
    float *rand_pixels = (float *)malloc(npixels * sizeof(float));

    if (rand_pixels == NULL) {
        printf("Memory allocation failed!\n");
        fits_close_file(fptr, &status);
        return 1;
    }

    for (int i = 0; i < npixels; i++) {
        rand_pixels[i] = (float)rand() / (float)RAND_MAX;
    }

    /* 7. Write the random pixel array to the new extension */
    long firstelem = 1;
    if (fits_write_img(fptr, TFLOAT, firstelem, npixels, rand_pixels, &status)) {
        fits_report_error(stderr, status);
    } else {
        printf("Successfully generated 'stage_05_create_noise.fits' with a random noise extension!\n");
    }

    /* 8. Clean up memory and close the file safely */
    free(rand_pixels);
    fits_close_file(fptr, &status);

    if (status) {
        fits_report_error(stderr, status);
    }

    return status;
}
