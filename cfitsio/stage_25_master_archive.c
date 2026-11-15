#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main() {
    fitsfile *f_cube, *f_table, *f_master;
    int status = 0;

    printf("========================================================\n");
    printf("     STAGE 25: OBSERVATORY MASTER ARCHIVE PACKAGER      \n");
    printf("========================================================\n");
    printf(" Fusing 3D WCS-Time Cube and Clean SNR Tables...\n");

    /* 1. Create the brand new Master Archive FITS container */
    if (fits_create_file(&f_master, "!stage_25_master_observatory_dataset.fits", &status)) {
        printf("Error: Could not create master archive file.\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* Initialize the mandatory empty primary array baseline layer */
    int primary_bitpix = 8;
    int primary_naxis = 0;
    long primary_naxes[] = {0};
    fits_create_img(f_master, primary_bitpix, primary_naxis, primary_naxes, &status);

    /* 2. Open your 3D time cube file and copy its image extension layer */
    if (fits_open_file(&f_cube, "stage_17_time_cube.fits", READONLY, &status)) {
        printf("Error: Missing 'stage_17_time_cube.fits' package.\n");
        fits_report_error(stderr, status);
        fits_close_file(f_master, &status);
        return status;
    }

    // Move focus to the 3D space-time block layer
    if (fits_movnam_hdu(f_cube, IMAGE_HDU, "STARRY_SKY_CUBE", 0, &status)) {
        fits_report_error(stderr, status);
    } else {
        /* Copy the current open HDU layer straight into the end of our master container file */
        fits_copy_hdu(f_cube, f_master, 0, &status);
        printf(" -> Successfully integrated 3D WCS-TIME Image Cube extension layer.\n");
    }
    fits_close_file(f_cube, &status);

    /* 3. Open your cleaned SNR table file and copy its binary catalog extension */
    if (fits_open_file(&f_table, "stage_23_all_stars_snr_catalog.fits", READONLY, &status)) {
        printf("Error: Missing 'stage_23_all_stars_snr_catalog.fits' database file.\n");
        fits_report_error(stderr, status);
        fits_close_file(f_master, &status);
        return status;
    }

    // Move focus to the verified table data block layer
    if (fits_movnam_hdu(f_table, BINARY_TBL, "SNR_CATALOG", 0, &status)) {
        fits_report_error(stderr, status);
    } else {
        /* Copy the current open table HDU layer straight into the end of our master container file */
        fits_copy_hdu(f_table, f_master, 0, &status);

        /* Optional: Rename this layer inside the master file to something descriptive */
        fits_movabs_hdu(f_master, 3, NULL, &status); // Move focus to HDU 3 (the newly added table)
        fits_update_key(f_master, TSTRING, "EXTNAME", "CLEAN_CATALOG", "Cleaned Photometry Summary", &status);
        printf(" -> Successfully integrated 87-Row Quality Filtered Table extension layer.\n");
    }
    fits_close_file(f_table, &status);

    /* 4. Flush updates to storage and close the master file descriptor safely */
    fits_close_file(f_master, &status);

    if (status) {
        printf("--------------------------------------------------------\n");
        printf(" An error occurred during final archive packaging pipeline.\n");
        fits_report_error(stderr, status);
    } else {
        printf("--------------------------------------------------------\n");
        printf(" SUCCESS: Observatory Master Archive created successfully! \n");
        printf(" Output Path Generated: stage_25_master_observatory_dataset.fits  \n");
        printf("========================================================\n");
    }

    return status;
}
