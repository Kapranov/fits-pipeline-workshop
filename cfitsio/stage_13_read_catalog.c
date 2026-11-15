#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;

    /* 1. Open the updated FITS file in READONLY mode */
    if (fits_open_file(&fptr, "stage_07_create_starry_sky_header.fits", READONLY, &status)) {
        printf("Error: Could not open the FITS file. Check path and permissions.\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Move to the "STAR_DATA" binary table extension layer */
    if (fits_movnam_hdu(fptr, BINARY_TBL, "STAR_DATA", 0, &status)) {
        printf("Error: Could not locate the 'STAR_DATA' binary table extension!\n");
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 3. Read table properties to find out exactly how many rows are stored */
    int num_cols;
    long num_rows;
    if (fits_get_num_rows(fptr, &num_rows, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    printf("===============================================================\n");
    printf("     STAGE 15: READING BACK DISCOVERED FITS BINARY CATALOG    \n");
    printf("===============================================================\n");
    printf(" Found Extension Layer 'STAR_DATA' containing %ld rows.\n", num_rows);
    printf("---------------------------------------------------------------\n");
    printf("  Star ID |       Name       |  X (pixel)  |  Y (pixel)  |   Flux   \n");
    printf("---------------------------------------------------------------\n");

    /* 4. Allocate temporary read data buffers */
    int star_id = 0;
    float x_pos = 0.0f;
    float y_pos = 0.0f;
    float flux = 0.0f;

    // String buffers for TSTRING matching need to be pointers to char arrays
    char star_name[21];
    char *name_ptr = star_name;
    int anynul = 0;

    /* 5. Loop through every row entry inside the data column grids */
    for (long r = 1; r <= num_rows; r++) {
        // Clear old string footprint memory
        star_name[0] = '\0';

        /* Pull data row-by-row matching schema mapping fields
           Arguments: fptr, datatype, colnum, rownum, firstelem, nelements, nulval, data_buffer, anynul, status */
        fits_read_col(fptr, TINT,    1, r, 1, 1, NULL, &star_id,  &anynul, &status);
        fits_read_col(fptr, TSTRING, 2, r, 1, 1, NULL, &name_ptr, &anynul, &status);
        fits_read_col(fptr, TFLOAT,  3, r, 1, 1, NULL, &x_pos,    &anynul, &status);
        fits_read_col(fptr, TFLOAT,  4, r, 1, 1, NULL, &y_pos,    &anynul, &status);
        fits_read_col(fptr, TFLOAT,  5, r, 1, 1, NULL, &flux,     &anynul, &status);

        if (status) {
            printf("Error reading data at row %ld\n", r);
            break;
        }

        /* 6. Stream row statistics seamlessly back out to console output */
        printf("   %4d   |  %-15s |   %7.2f   |   %7.2f   |  %7.4f \n",
               star_id, star_name, x_pos, y_pos, flux);
    }

    printf("===============================================================\n");
    printf(" END OF REPORT: Verification complete.\n");
    printf("===============================================================\n");

    /* 7. Close your hook structures down neatly */
    fits_close_file(fptr, &status);
    if (status) {
        fits_report_error(stderr, status);
    }

    return status;
}
