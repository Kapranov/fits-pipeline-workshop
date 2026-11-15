#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int read_section_6() {
    fitsfile *fptr;
    int status = 0;
    int hdu_type;

    // 1. Open your master multi-extension FITS file
    if (fits_open_file(&fptr, "wcs_output.fits", READONLY, &status)) {
        fits_report_error(stderr, status);
        return status;
    }

    // 2. Move directly to HDU index 6 (1-based index)
    int hdu_num = 6;
    if (fits_movabs_hdu(fptr, hdu_num, &hdu_type, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    // Double check that it's actually an image extension
    if (hdu_type != IMAGE_HDU) {
        fprintf(stderr, "Error: HDU 6 is not an image extension!\n");
        fits_close_file(fptr, &status);
        return 1;
    }

    // 3. Read the image structural dimensions (NAXIS and NAXES)
    int naxis;
    long naxes[2] = {0, 0}; // Assumes a 2D image dimension layout

    fits_get_img_dim(fptr, &naxis, &status);
    fits_get_img_size(fptr, 2, naxes, &status);

    if (status) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    printf("Successfully opened HDU 6! Dimensions: %ld x %ld\n", naxes[0], naxes[1]);

    // 4. Allocate a buffer large enough to store the pixels
    long total_pixels = naxes[0] * naxes[1];
    float *img_pixels = (float *)malloc(total_pixels * sizeof(float));
    if (img_pixels == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for image buffer.\n");
        fits_close_file(fptr, &status);
        return 1;
    }

    // 5. Read the pixel data elements array
    long firstelem = 1;
    float nulval = 0.0f; // Value to assign to undefined/blank pixels
    int anynul;

    if (fits_read_img(fptr, TFLOAT, firstelem, total_pixels, &nulval, img_pixels, &anynul, &status)) {
        fits_report_error(stderr, status);
    } else {
        // Print out the first 5 pixel coordinates to verify contents
        printf("First 5 pixels inside HDU 6 matrix:\n");
        for (int i = 0; i < 5 && i < total_pixels; i++) {
            printf("  Pixel [%d]: %f\n", i, img_pixels[i]);
        }
    }

    // Clean up memory allocations and close files
    free(img_pixels);
    fits_close_file(fptr, &status);

    return status;
}

int main() {
    // Run the section reader operation logic
    read_section_6();
}
