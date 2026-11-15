#include <stdio.h>
#include "fitsio.h" /* Required by every program that uses CFITSIO */

int main() {
    fitsfile *fptr;   /* Pointer to the FITS file */
    int status = 0;   /* MUST initialize status to 0 before calling CFITSIO routines */

    /* 1. Create the new empty FITS file
       Adding '!' before the filename forces CFITSIO to overwrite it if it exists */
    if (fits_create_file(&fptr, "!stage_01_create_blank.fits", &status)) {
        /* If status is non-zero, an error occurred */
        fits_report_error(stderr, status);
        return status;
    }

    printf("Successfully created an empty FITS file: stage_01_create_blank.fits\n");

    /* 2. Always close the file to flush buffers and free resources */
    fits_close_file(fptr, &status);

    /* Check for errors during closure */
    if (status) {
        fits_report_error(stderr, status);
    }

    return status;
}
