#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;
    int num_hdus = 0;

    /* 1. Open the master observatory unified archive dataset file */
    if (fits_open_file(&fptr, "stage_25_master_observatory_dataset.fits", READONLY, &status)) {
        printf("Error: Could not open master archive dataset file.\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Automatically query the file to find out the total number of HDU layers */
    if (fits_get_num_hdus(fptr, &num_hdus, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    printf("========================================================================\n");
    printf("         MASTER OBSERVATORY DEEP DATA ENTIRE HEADER DUMPER              \n");
    printf("========================================================================\n");
    printf(" Found Total HDU Extension Layers inside Master Container File: %d\n", num_hdus);

    /* 3. Pre-allocate character buffers for header cards parsing */
    char key_name[FLEN_KEYWORD];
    char value_str[FLEN_VALUE];
    char comment_str[FLEN_COMMENT];
    char hdu_name[FLEN_VALUE];

    /* 4. Outer Loop: Sequentially step through every physical HDU index slot */
    for (int hdu_idx = 1; hdu_idx <= num_hdus; hdu_idx++) {
        int hdu_type = 0;
        status = 0; /* CRITICAL FIX 1: Explicitly clear status at start of every loop iteration */

        /* Move focus to the specific absolute HDU layer index position */
        if (fits_movabs_hdu(fptr, hdu_idx, &hdu_type, &status)) {
            fits_report_error(stderr, status);
            break;
        }

        /* Attempt to read the EXTNAME keyword for this specific extension layer */
        hdu_name[0] = '\0'; // Clear old string footprint safely
        fits_read_key(fptr, TSTRING, "EXTNAME", hdu_name, NULL, &status);

        if (status == MEMBER_NOT_FOUND || status == 202) {
            if (hdu_idx == 1) {
                sprintf(hdu_name, "PRIMARY");
            } else {
                sprintf(hdu_name, "UNNAMED_EXTENSION");
            }
            status = 0; /* CRITICAL FIX 2: Reset status back to success immediately */
        }

        /* Query the active extension layer to find out its card limits */
        int total_keys = 0;
        int space_left = 0;
        fits_get_hdrspace(fptr, &total_keys, &space_left, &status);

        if (status) {
            fits_report_error(stderr, status);
            status = 0; // Clear error to preserve outer loop continuity
            continue;
        }

        /* Print layer separator layout card header block template */
        printf("\n========================================================================\n");
        printf(" 📂  HDU LAYER #%d | EXTENSION NAME: %s | ", hdu_idx, hdu_name);

        if (hdu_idx == 1) {
            printf("TYPE: PRIMARY HDU ARRAY\n");
        } else if (hdu_type == IMAGE_HDU) {
            printf("TYPE: IMAGE EXTENSION LAYER\n");
        } else if (hdu_type == BINARY_TBL) {
            printf("TYPE: BINARY DATA TABLE\n");
        } else {
            printf("TYPE: SPECIAL EXTENSION LAYER\n");
        }
        printf(" Total Header Cards Found: %d\n", total_keys);
        printf("------------------------------------------------------------------------\n");
        printf(" Card # | Keyword Name |            Value           |     Comment Block \n");
        printf("------------------------------------------------------------------------\n");

        /* 5. Inner Loop: Automatically scan and dump lines inside the open layer */
        for (int k = 1; k <= total_keys; k++) {
            if (fits_read_keyn(fptr, k, key_name, value_str, comment_str, &status)) {
                status = 0; /* Reset flag to bypass unparseable internal card elements gracefully */
                continue;
            }
            printf("  %03d   |   %-10s | %-26s | %s\n", k, key_name, value_str, comment_str);
        }
        printf("------------------------------------------------------------------------\n");
    }

    printf("\n========================================================================\n");
    printf(" DEEP DUMP ENTIRE STRUCTURE AUDIT METRICS COMPLETE: All extensions logged.\n");
    printf("========================================================================\n");

    /* 6. Close file structures cleanly */
    status = 0; // Clear any trailing error ticks before hitting final block close
    fits_close_file(fptr, &status);
    return status;
}
