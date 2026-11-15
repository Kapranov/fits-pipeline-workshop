#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

#define WIDTH  512
#define HEIGHT 512

int main() {
    fitsfile *fptr;
    int status = 0;

    // 1. Allocate memory and generate a synthetic 512x512 2D image matrix
    float *pixels = (float *)malloc(WIDTH * HEIGHT * sizeof(float));
    if (pixels == NULL) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return 1;
    }

    // Populate matrix with a simple geometric diagonal gradient
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            pixels[y * WIDTH + x] = (float)(x + y);
        }
    }

    // 2. Open / Create the physical FITS structural container
    if (fits_create_file(&fptr, "!image.fits", &status)) {
        fits_report_error(stderr, status);
        free(pixels);
        return status;
    }

    // 3. Configure the Primary HDU for a 2D matrix
    int bitpix = FLOAT_IMG;        // Data type inside FITS header (-32)
    int naxis = 2;                 // 2 dimensions (X and Y axes)
    long naxes[] = {WIDTH, HEIGHT}; // Matrix boundaries: [X_size, Y_size]

    if (fits_create_img(fptr, bitpix, naxis, naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        free(pixels);
        return status;
    }

    // 4. Write the 2D memory pixel array into the FITS image data block
    // Argument 3 is a flat scalar number representing the starting index.
    // FITS uses 1-based indexing, so the first pixel is 1.
    LONGLONG firstelem = 1;
    LONGLONG nelements = (LONGLONG)WIDTH * HEIGHT;

    if (fits_write_img(fptr, TFLOAT, firstelem, nelements, pixels, &status)) {
        fits_report_error(stderr, status);
    } else {
        /* --- OPTIONAL: Add Metadata Header Keywords --- */

        // 1. Write a String Keyword (e.g., target object name)
        // fits_update_key(fptr, datatype, keyword, value, comment, &status)
        fits_update_key(fptr, TSTRING, "OBJECT", "M31", "Target Andromeda Galaxy", &status);

        // 2. Write a Floating Point Keyword (e.g., exposure time in seconds)
        double exptime = 300.0;
        fits_update_key(fptr, TDOUBLE, "EXPTIME", &exptime, "Exposure time in seconds", &status);

        // 3. Write an Integer Keyword (e.g., number of co-added frames)
        int ncoadds = 5;
        fits_update_key(fptr, TINT, "NCOADDS", &ncoadds, "Number of frames integrated", &status);

        // 4. Add a generic comment card block
        fits_write_comment(fptr, "This FITS file was generated using custom C code.", &status);
    }

    if (status) {
        fits_report_error(stderr, status);
    } else {
        printf("Successfully injected custom metadata header keywords!\n");
    }

    return status;
}
