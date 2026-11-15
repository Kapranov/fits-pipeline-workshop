#include <stdio.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;       /* Pointer to the FITS file */
    int status = 0;       /* MUST initialize status to 0 */

    /* 1. Create the new file */
    if (fits_create_file(&fptr, "!stage_03_create_table.fits", &status)) {
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Create an empty Primary HDU (Mandatory to avoid error 252) */
    int bitpix = 8;
    int naxis = 0;
    long naxes[1] = {0};
    if (fits_create_img(fptr, bitpix, naxis, naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 3. Define the Table Structure */
    int tfields = 3;                       /* Number of columns */
    long nrows = 0;                        /* Initial number of rows (can grow dynamically) */
    char extname[] = "STAR_DATA";          /* Name of the extension HDU */

    /* Column formats: '1J' = 32-bit Integer, '20A' = 20-char String, '1E' = Single Precision Float */
    char *ttype[] = {"ID", "NAME", "FLUX"};
    char *tform[] = {"1J", "20A", "1E"};
    char *tunit[] = {"count", "none", "jansky"};

    /* 4. Append the Binary Table Extension */
    if (fits_create_tbl(fptr, BINARY_TBL, nrows, tfields, ttype, tform, tunit, extname, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    printf("Successfully created FITS file with a Binary Table extension.\n");

    /* 5. Close the file */
    fits_close_file(fptr, &status);
    if (status) {
        fits_report_error(stderr, status);
    }

    return status;
}
