#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;
    int hdutype;

    // 1. Open the existing multi-extension FITS file in Read-Only mode
    if (fits_open_file(&fptr, "multi_ext.fits", READONLY, &status)) {
        fits_report_error(stderr, status);
        return status;
    }

    // 2. Search and locate the extension by its name ("SCI_DATA")
    // IMAGE_HDU explicitly restricts the search parameters to image-based matrices
    if (fits_movnam_hdu(fptr, IMAGE_HDU, "SCI_DATA", 0, &status)) {
        fprintf(stderr, "Error: Could not locate extension named 'SCI_DATA'\n");
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }
    printf("Successfully located and jumped to 'SCI_DATA' extension!\n");

    // 3. Dynamically discover the matrix size of this specific extension
    int bitpix, naxis;
    long naxes[2]; // Since we expect a 2D image matrix

    // fits_get_img_param reads NAXIS, NAXIS1, NAXIS2, etc., from the current HDU header
    if (fits_get_img_param(fptr, 2, &bitpix, &naxis, naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    long width = naxes[0];
    long height = naxes[1];
    long total_pixels = width * height;
    printf("Extension dimensions: %ld x %ld pixels\n", width, height);

    // 4. Allocate local system buffer memory to hold incoming pixels
    float *pixels = (float *)malloc(total_pixels * sizeof(float));
    if (pixels == NULL) {
        fprintf(stderr, "Error: Local heap memory allocation failed.\n");
        fits_close_file(fptr, &status);
        return 1;
    }

    // 5. Read the pixel data out of the data block
    LONGLONG firstelem = 1;
    LONGLONG nelements = (LONGLONG)total_pixels;
    float nulval = 0.0f; // Value to fill in if a blank/corrupted pixel is encountered
    int anynul;

    if (fits_read_img(fptr, TFLOAT, firstelem, nelements, &nulval, pixels, &anynul, &status)) {
        fits_report_error(stderr, status);
    } else {
        printf("Successfully read all %ld pixels into system memory.\n", total_pixels);

        // Let's print out a few sample pixel values to verify they match what we wrote
        printf("Sample data values:\n");
        printf("  Pixel [0,0]   = %f\n", pixels[0]);
        printf("  Pixel [1,0]   = %f\n", pixels[1]);
        printf("  Pixel [511,511] = %f\n", pixels[total_pixels - 1]);
    }

    // 6. Clean up and close down file handles cleanly
    fits_close_file(fptr, &status);
    free(pixels);

    if (status) {
        fits_report_error(stderr, status);
    }
    return status;
}
