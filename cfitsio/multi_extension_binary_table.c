#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

#define WIDTH  512
#define HEIGHT 512

int main() {
    fitsfile *fptr;
    int status = 0;

    // Allocate memory for our image extension data
    float *pixels = (float *)malloc(WIDTH * HEIGHT * sizeof(float));
    if (pixels == NULL) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return 1;
    }
    for (int i = 0; i < WIDTH * HEIGHT; i++) pixels[i] = (float)i;

    // 1. Create the new physical FITS file container
    if (fits_create_file(&fptr, "!multi_ext.fits", &status)) {
        fits_report_error(stderr, status);
        free(pixels);
        return status;
    }

    // ====================================================================
    // HDU 1: PRIMARY HDU (Empty data, metadata only)
    // ====================================================================
    int bitpix = BYTE_IMG;
    int naxis = 0;
    long naxes[1] = {0};

    if (fits_create_img(fptr, bitpix, naxis, naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        free(pixels);
        return status;
    }
    // Add a global header keyword to the Primary HDU
    fits_update_key(fptr, TSTRING, "ORIGIN", "MyObservatory", "Global source", &status);

    // ====================================================================
    // HDU 2: IMAGE EXTENSION (Appending our 2D Matrix)
    // ====================================================================
    // Calling fits_create_img again automatically creates a new extension (XTENSION = 'IMAGE')
    int ext_bitpix = FLOAT_IMG;
    int ext_naxis = 2;
    long ext_naxes[] = {WIDTH, HEIGHT};

    if (fits_create_img(fptr, ext_bitpix, ext_naxis, ext_naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        free(pixels);
        return status;
    }

    // Name this specific extension so it can be easily found later
    fits_update_key(fptr, TSTRING, "EXTNAME", "SCI_DATA", "Extension Name", &status);
    fits_update_key(fptr, TSTRING, "OBJECT", "M31", "Target object", &status);

    // Write the pixel data into this newly appended extension
    LONGLONG firstelem = 1;
    LONGLONG nelements = (LONGLONG)WIDTH * HEIGHT;

    if (fits_write_img(fptr, TFLOAT, firstelem, nelements, pixels, &status)) {
        fits_report_error(stderr, status);
    } else {
        printf("Successfully created multi-extension FITS file with an image extension.\n");
    }

        // ====================================================================
    // HDU 3: BINARY TABLE EXTENSION
    // ====================================================================
    // 1. Define the table structure dimensions
    int ttype = BINARY_TBL;       // Type of table extension
    long nrows = 3;               // Initial number of rows
    int tfields = 2;              // Number of columns

    // 2. Define the columns (Names, Formats, and Units)
    char *ttype_names[] = {"STAR_ID", "MAGNITUDE"}; // Column names
    char *tform[] = {"10A", "1D"};                 // 10A = 10-char string, 1D = 1 double float
    char *tunit[] = {"", "mag"};                   // Physical units
    char *extname = "STARS_CATALOG";               // Extension name

    // 3. Create the empty table extension section
    if (fits_create_tbl(fptr, ttype, nrows, tfields, ttype_names, tform, tunit, extname, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        free(pixels);
        return status;
    }

    // 4. Prepare your data arrays
    char *star_ids[] = {"Alpha", "Beta", "Gamma"};
    double magnitudes[] = {4.25, 1.12, 2.84};

    // 5. Write the data into the columns (1-based index)
    long firstrow = 1;
    // Rename it to something distinct like tbl_firstelem
    long tbl_firstelem = 1;

    // Then update your writing functions to use the new name:
    fits_write_col(fptr, TSTRING, 1, firstrow, tbl_firstelem, nrows, star_ids, &status);
    fits_write_col(fptr, TDOUBLE, 2, firstrow, tbl_firstelem, nrows, magnitudes, &status);

    // Write column 2 (Doubles)
    if (fits_write_col(fptr, TDOUBLE, 2, firstrow, firstelem, nrows, magnitudes, &status)) {
        fits_report_error(stderr, status);
    } else {
        printf("Successfully appended Binary Table section.\n");
    }


    // Clean up and close
    fits_close_file(fptr, &status);
    free(pixels);

    if (status) {
        fits_report_error(stderr, status);
    }
    return status;
}
