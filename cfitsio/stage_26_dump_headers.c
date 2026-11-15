#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;

    /* 1. Open your master observatory unified archive dataset file */
    if (fits_open_file(&fptr, "stage_25_master_observatory_dataset.fits", READONLY, &status)) {
        printf("Error: Could not open master archive dataset file.\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Move to your 3D Space-Time Image Cube Layer */
    if (fits_movnam_hdu(fptr, IMAGE_HDU, "STARRY_SKY_CUBE", 0, &status)) {
        printf("Error: Failed to shift focus to 'STARRY_SKY_CUBE' HDU layer.\n");
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 3. Query the active header to find out the total number of keyword cards stored inside */
    int total_keys = 0;
    int space_left = 0;
    if (fits_get_hdrspace(fptr, &total_keys, &space_left, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    printf("========================================================================\n");
    printf("         AUTOMATED MULTI-EXTENSION HEADER CARD CATALOG DUMPER           \n");
    printf("========================================================================\n");
    printf(" Active HDU Layer focus : STARRY_SKY_CUBE | Total Header Cards Found: %d\n", total_keys);
    printf("------------------------------------------------------------------------\n");
    printf(" Card # | Keyword Name |            Value           |     Comment Block \n");
    printf("------------------------------------------------------------------------\n");

    /* 4. Pre-allocate character buffers using correct CFITSIO macro lengths */
    char key_name[FLEN_KEYWORD];
    char value_str[FLEN_VALUE];
    char comment_str[FLEN_COMMENT];

    /* 5. Automatically loop sequentially from index 1 to the total number of keys */
    for (int k = 1; k <= total_keys; k++) {
        if (fits_read_keyn(fptr, k, key_name, value_str, comment_str, &status)) {
            fits_report_error(stderr, status);
            status = 0; /* Reset status flag pointer to keep the loop scanning gracefully */
            continue;
        }

        /* 6. Stream card details back out to console alignment template columns */
        printf("  %03d   |   %-10s | %-26s | %s\n", k, key_name, value_str, comment_str);
    }

    printf("========================================================================\n");
    printf(" COMPLETED DIRECTORY SCRAPE SUMMARY: Scanned all internal header lines.\n");
    printf("========================================================================\n");

    /* 7. Close file structures cleanly */
    fits_close_file(fptr, &status);
    return status;
}
