#include <stdio.h>
#include <stdlib.h>
#include <math.h> /* Required for fabs() mapping transformations */
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

    printf("=========================================================================================\n");
    printf("         STAGE 15: READING DISCOVERED CATALOG WITH CELESTIAL WCS COORDINATES             \n");
    printf("=========================================================================================\n");
    printf(" Found Extension Layer 'STAR_DATA' containing %ld rows.\n", num_rows);
    printf("-----------------------------------------------------------------------------------------\n");
    printf("  Star ID |       Name       |  X (pix)  |  Y (pix)  |    RA (HMS)     |    DEC (DMS)    |  Flux  \n");
    printf("-----------------------------------------------------------------------------------------\n");

    /* 5. Loop through every row entry inside the data column grids */
    for (long r = 1; r <= num_rows; r++) {
        star_name[0] = '\0'; // Clear string footprint array index

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

        /* --- RA HMS CONVERSION MATH --- */
        double total_hours = ra_deg / 15.0;
        int ra_h = (int)total_hours;
        double total_minutes = (total_hours - ra_h) * 60.0;
        int ra_m = (int)total_minutes;
        double ra_s = (total_minutes - ra_m) * 60.0;

        /* --- DEC DMS CONVERSION MATH --- */
        double abs_dec = fabs(dec_deg);
        int dec_d = (int)abs_dec;
        double total_arcmin = (abs_dec - dec_d) * 60.0;
        int dec_m = (int)total_arcmin;
        double dec_s = (total_arcmin - dec_m) * 60.0;

        // Restore spatial sign marker value logic
        char sign = (dec_deg >= 0) ? '+' : '-';

        /* 6. Display the fully transformed coordinate grid layout */
        printf("   %4d   |  %-15s |   %7.2f |   %7.2f |  %02dh%02dm%05.2fs  |  %c%02dd%02dm%05.2fs  | %7.4f \n",
               star_id, star_name, x_pos, y_pos, ra_h, ra_m, ra_s, sign, dec_d, dec_m, dec_s, flux);
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
