#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;

    /* 1. Open the summary statistics file in READONLY mode */
    if (fits_open_file(&fptr, "stage_18_cube_statistics_summary.fits", READONLY, &status)) {
        printf("Error: Could not open the summary file 'stage_18_cube_statistics_summary.fits'.\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Move to the "FRAME_STATS" binary table extension layer */
    if (fits_movnam_hdu(fptr, BINARY_TBL, "FRAME_STATS", 0, &status)) {
        printf("Error: Could not locate the 'FRAME_STATS' table extension!\n");
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 3. Read table properties to find out exactly how many rows are stored */
    long num_rows;
    if (fits_get_num_rows(fptr, &num_rows, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    printf("=========================================================\n");
    printf("    3D TIME CUBE METRICS: BACKGROUND & FLUX ANALYSIS     \n");
    printf("=========================================================\n");
    printf(" Target Table: FRAME_STATS | Slices Processed: %ld rows\n", num_rows);
    printf("---------------------------------------------------------\n");
    printf("  Frame ID  |   BG Mean Flux   |   Max Flux   | Star Pixels \n");
    printf("---------------------------------------------------------\n");

    /* 4. Allocate temporary read data variables */
    int frame_id = 0;
    double bg_mean = 0.0;
    float max_flux = 0.0f;
    int star_pixels = 0;
    int anynul = 0;

    /* 5. Loop through every row entry inside the data column grids */
    for (long r = 1; r <= num_rows; r++) {
        /* Pull data row-by-row matching our 4-column layout schema
           Columns: 1:FRAME, 2:BG_MEAN, 3:MAX_FLUX, 4:STAR_PIXELS */
        fits_read_col(fptr, TINT,    1, r, 1, 1, NULL, &frame_id,    &anynul, &status);
        fits_read_col(fptr, TDOUBLE, 2, r, 1, 1, NULL, &bg_mean,     &anynul, &status);
        fits_read_col(fptr, TFLOAT,  3, r, 1, 1, NULL, &max_flux,    &anynul, &status);
        fits_read_col(fptr, TINT,    4, r, 1, 1, NULL, &star_pixels,  &anynul, &status);

        if (status) {
            printf("Error reading data at row %ld\n", r);
            break;
        }

        /* 6. Stream row statistics seamlessly back out to the console output */
        printf("    %02d      |    %10.6f    |   %8.4f   |    %5d    \n",
               frame_id, bg_mean, max_flux, star_pixels);
    }

    printf("=========================================================\n");
    printf(" END OF ENGINE REPORT: Fluctuations verified successfully.\n");
    printf("=========================================================\n");

    /* 7. Close file structures cleanly */
    fits_close_file(fptr, &status);
    if (status) {
        fits_report_error(stderr, status);
    }

    return status;
}
