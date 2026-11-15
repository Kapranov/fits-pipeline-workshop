#include <stdio.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;

    /* 1. Create the file structural container */
    if (fits_create_file(&fptr, "!empty.fits", &status)) {
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Initialize a valid, empty Primary HDU.
       BITPIX = 8 (Byte data type placeholder)
       NAXIS = 0  (Zero dimensions = no actual image data matrix) */
    int bitpix = BYTE_IMG;
    int naxis = 0;
    long naxes[1] = {0};

    if (fits_create_img(fptr, bitpix, naxis, naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status); // attempt cleanup
        return status;
    }

    printf("Successfully created a valid, empty FITS file.\n");

    /* 3. Safely close the file now that it has a valid header structure */
    fits_close_file(fptr, &status);
    if (status) {
        fits_report_error(stderr, status);
    }

    return status;
}
