#include <stdio.h>
#include "fitsio.h" /* Required by every program that uses CFITSIO */

int main() {
    fitsfile *fptr;   /* Pointer to the FITS file */
    int status = 0;   /* MUST initialize status to 0 before calling CFITSIO routines */

    /* 1. Create the new empty FITS file */
    if (fits_create_file(&fptr, "!stage_02_create_empty.fits", &status)) {
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Create the Primary HDU (this writes the required 'SIMPLE' keyword)
       bitpix = 8, naxis = 0 means a null/empty primary array (no image data yet) */
    int bitpix = 8;
    int naxis = 0;
    long naxes[1] = {0};

    if (fits_create_img(fptr, bitpix, naxis, naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    printf("Successfully created a valid blank FITS file: stage_02_create_empty.fits\n");

    /* 3. Always close the file to flush buffers and free resources */
    fits_close_file(fptr, &status);

    /* Check for errors during closure */
    if (status) {
        fits_report_error(stderr, status);
    }

    return status;
}
