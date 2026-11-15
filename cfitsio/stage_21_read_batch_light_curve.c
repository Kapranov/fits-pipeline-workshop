#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;

    /* 1. Open the batch light curves FITS table in READONLY mode */
    if (fits_open_file(&fptr, "stage_20_all_stars_light_curves.fits", READONLY, &status)) {
        printf("Error: Could not open the batch file 'stage_20_all_stars_light_curves.fits'.\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Move to the "LIGHT_CURVES" binary table extension layer */
    if (fits_movnam_hdu(fptr, BINARY_TBL, "LIGHT_CURVES", 0, &status)) {
        printf("Error: Could not locate the 'LIGHT_CURVES' table extension!\n");
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 3. Read table properties to find out the total row record count */
    long num_rows;
    if (fits_get_num_rows(fptr, &num_rows, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 4. Request the target Star ID filter from the user */
    int target_star_id = 0;
    printf("=========================================================\n");
    printf("        BATCH FITS LIGHT CURVE EXTRACTION UTILITY        \n");
    printf("=========================================================\n");
    printf(" Total database records available: %ld rows\n", num_rows);
    printf(" Enter Star ID to look up (e.g., 1 to 50): ");
    if (scanf("%d", &target_star_id) != 1) return 1;

    printf("\n---------------------------------------------------------\n");
    printf("  Extracting Light Curve Data For Star ID: %d\n", target_star_id);
    printf("---------------------------------------------------------\n");
    printf("  Frame ID  |   X Position   |   Y Position   |   Net Flux   \n");
    printf("---------------------------------------------------------\n");

    /* 5. Set up temporary column read variables */
    int star_id = 0;
    int frame_id = 0;
    float x_center = 0.0f;
    float y_center = 0.0f;
    float net_flux = 0.0f;
    int anynul = 0;
    int print_count = 0;

    /* 6. Scan every row, filtering for the requested Star ID */
    for (long r = 1; r <= num_rows; r++) {
        /* Read the first column (STAR_ID) to check for a quick filter match */
        fits_read_col(fptr, TINT, 1, r, 1, 1, NULL, &star_id, &anynul, &status);

        if (star_id == target_star_id) {
            /* If it matches, pull the rest of the column fields for this row
               Columns: 1:STAR_ID, 2:FRAME, 3:X_CENTER, 4:Y_CENTER, 5:NET_FLUX */
            fits_read_col(fptr, TINT,   2, r, 1, 1, NULL, &frame_id,   &anynul, &status);
            fits_read_col(fptr, TFLOAT, 3, r, 1, 1, NULL, &x_center,   &anynul, &status);
            fits_read_col(fptr, TFLOAT, 4, r, 1, 1, NULL, &y_center,   &anynul, &status);
            fits_read_col(fptr, TFLOAT, 5, r, 1, 1, NULL, &net_flux,   &anynul, &status);

            if (status) {
                printf("Error reading data fields at database row %ld\n", r);
                break;
            }

            printf("    %02d      |    %8.2f    |    %8.2f    |   %8.4f\n",
                   frame_id, x_center, y_center, net_flux);
            print_count++;
        }
    }

    if (print_count == 0) {
        printf(" Warning: No records found matching Star ID %d!\n", target_star_id);
    }
    printf("=========================================================\n");

    /* 7. Close file structures cleanly */
    fits_close_file(fptr, &status);
    if (status) {
        fits_report_error(stderr, status);
    }

    return status;
}
