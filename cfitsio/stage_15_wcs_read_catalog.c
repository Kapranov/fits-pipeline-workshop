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

    /* 3. Read table metrics to find out exactly how many rows were stored */
    long num_rows;
    if (fits_get_num_rows(fptr, &num_rows, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    printf("=========================================================================================\n");
    printf("         STAGE 15: READING DISCOVERED CATALOG WITH CELESTIAL WCS COORDINATES             \n");
    printf("=========================================================================================\n");
    printf(" Found Extension Layer 'STAR_DATA' containing %ld rows.\n", num_rows);
    printf("-----------------------------------------------------------------------------------------\n");
    printf("  Star ID |       Name       |  X (pix)  |  Y (pix)  |    RA (deg)   |   DEC (deg)  |  Flux  \n");
    printf("-----------------------------------------------------------------------------------------\n");

    /* 4. Allocate temporary read data variables */
    int star_id = 0;
    float x_pos = 0.0f;
    float y_pos = 0.0f;
    double ra_deg = 0.0;
    double dec_deg = 0.0;
    float flux = 0.0f;

    char star_name[20];
    char *name_ptr = star_name;
    int anynul = 0;

    /* 5. Loop through every row entry inside the data column grids */
    for (long r = 1; r <= num_rows; r++) {
        star_name[0] = '\0'; // Clear old string data

        /* Pull data row-by-row matching our updated 7-column layout schema
           Columns: 1:ID, 2:NAME, 3:X_POS, 4:Y_POS, 5:RA, 6:DEC, 7:FLUX */
        fits_read_col(fptr, TINT,    1, r, 1, 1, NULL, &star_id,  &anynul, &status);
        fits_read_col(fptr, TSTRING, 2, r, 1, 1, NULL, &name_ptr, &anynul, &status);
        fits_read_col(fptr, TFLOAT,  3, r, 1, 1, NULL, &x_pos,    &anynul, &status);
        fits_read_col(fptr, TFLOAT,  4, r, 1, 1, NULL, &y_pos,    &anynul, &status);
        fits_read_col(fptr, TDOUBLE, 5, r, 1, 1, NULL, &ra_deg,   &anynul, &status);
        fits_read_col(fptr, TDOUBLE, 6, r, 1, 1, NULL, &dec_deg,  &anynul, &status);
        fits_read_col(fptr, TFLOAT,  7, r, 1, 1, NULL, &flux,     &anynul, &status);

        if (status) {
            printf("Error reading data at row %ld\n", r);
            break;
        }

        /* 6. Display the formatted dataset cleanly on the terminal */
        // Using %14.6f for RA/DEC to capture the precise degree decimals
        printf("   %4d   |  %-15s |   %7.2f |   %7.2f | %13.6f | %12.6f | %7.4f \n", 
               star_id, star_name, x_pos, y_pos, ra_deg, dec_deg, flux);
    }

    printf("=========================================================================================\n");
    printf(" END OF REPORT: Verification complete.\n");
    printf("=========================================================================================\n");

    /* 7. Close file structures cleanly */
    fits_close_file(fptr, &status);
    if (status) {
        fits_report_error(stderr, status);
    }

    return status;
}
