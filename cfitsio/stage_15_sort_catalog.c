#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "fitsio.h"

/* Structure to track catalog entries in memory for sorting */
typedef struct {
    int id;
    char name[20];
    float x;
    float y;
    double ra;
    double dec;
    float flux;
} StarRecord;

/* Comparison helper function for QuickSort (descending order by flux magnitude) */
int compare_stars(const void *a, const void *b) {
    StarRecord *starA = (StarRecord *)a;
    StarRecord *starB = (StarRecord *)b;

    if (starA->flux < starB->flux) return 1;
    if (starA->flux > starB->flux) return -1;
    return 0;
}

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

    /* 4. Allocate temporary array memory buffer to hold records */
    StarRecord *catalog = (StarRecord *)malloc(num_rows * sizeof(StarRecord));
    if (catalog == NULL) {
        printf("Memory allocation failed!\n");
        fits_close_file(fptr, &status);
        return 1;
    }

    int anynul = 0;

    /* 5. Read all columns row-by-row into our localized struct array array memory */
    for (long r = 1; r <= num_rows; r++) {
        char *name_ptr = catalog[r - 1].name;
        catalog[r - 1].name[0] = '\0';

        fits_read_col(fptr, TINT,    1, r, 1, 1, NULL, &catalog[r - 1].id,   &anynul, &status);
        fits_read_col(fptr, TSTRING, 2, r, 1, 1, NULL, &name_ptr,            &anynul, &status);
        fits_read_col(fptr, TFLOAT,  3, r, 1, 1, NULL, &catalog[r - 1].x,    &anynul, &status);
        fits_read_col(fptr, TFLOAT,  4, r, 1, 1, NULL, &catalog[r - 1].y,    &anynul, &status);
        fits_read_col(fptr, TDOUBLE, 5, r, 1, 1, NULL, &catalog[r - 1].ra,   &anynul, &status);
        fits_read_col(fptr, TDOUBLE, 6, r, 1, 1, NULL, &catalog[r - 1].dec,  &anynul, &status);
        fits_read_col(fptr, TFLOAT,  7, r, 1, 1, NULL, &catalog[r - 1].flux, &anynul, &status);
    }

    if (status) {
        printf("Error fetching columns from disk.\n");
        fits_report_error(stderr, status);
        free(catalog);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 6. Sort the struct array in place based on descending flux levels */
    qsort(catalog, num_rows, sizeof(StarRecord), compare_stars);

    /* 7. Display the sorted sexagesimal matrix results */
    printf("=========================================================================================\n");
    printf("     STAGE 15: READING SORTED DATA CATALOG (BRIGHTEST FLUX TARGETS FIRST)                \n");
    printf("=========================================================================================\n");
    printf(" Total Stars Processed: %ld unique listings.\n", num_rows);
    printf("-----------------------------------------------------------------------------------------\n");
    printf("  Rank ID |   Orig ID   |       Name       |  X (pix)  |  Y (pix)  |    RA (HMS)     |    DEC (DMS)    |  Flux  \n");
    printf("-----------------------------------------------------------------------------------------\n");

    for (long i = 0; i < num_rows; i++) {
        /* RA HMS Conversion */
        double total_hours = catalog[i].ra / 15.0;
        int ra_h = (int)total_hours;
        double total_minutes = (total_hours - ra_h) * 60.0;
        int ra_m = (int)total_minutes;
        double ra_s = (total_minutes - ra_m) * 60.0;

        /* DEC DMS Conversion */
        double abs_dec = fabs(catalog[i].dec);
        int dec_d = (int)abs_dec;
        double total_arcmin = (abs_dec - dec_d) * 60.0;
        int dec_m = (int)total_arcmin;
        double dec_s = (total_arcmin - dec_m) * 60.0;
        char sign = (catalog[i].dec >= 0) ? '+' : '-';

        printf("   %4ld   |    %4d   |  %-15s |   %7.2f |   %7.2f |  %02dh%02dm%05.2fs  |  %c%02dd%02dm%05.2fs  | %7.4f \n",
               i + 1, catalog[i].id, catalog[i].name, catalog[i].x, catalog[i].y, ra_h, ra_m, ra_s, sign, dec_d, dec_m, dec_s, catalog[i].flux);
    }

    printf("=========================================================================================\n");
    printf(" END OF REPORT: Verification complete.\n");
    printf("=========================================================================================\n");

    /* 8. Clean up heap footprints and close handles */
    free(catalog);
    fits_close_file(fptr, &status);
    return status;
}
