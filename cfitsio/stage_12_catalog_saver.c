#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;

    /* 1. Open the stage_07 simulation file in READWRITE mode */
    if (fits_open_file(&fptr, "stage_07_create_starry_sky_header.fits", READWRITE, &status)) {
        printf("Error: Could not open stage_07 file. Make sure it exists!\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Target the STARRY_SKY extension to pull the pixels into memory first */
    if (fits_movnam_hdu(fptr, IMAGE_HDU, "STARRY_SKY", 0, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    int bitpix, naxis;
    long naxes[2] = {0, 0};
    fits_get_img_param(fptr, 2, &bitpix, &naxis, naxes, &status);
    long width = naxes[0];
    long height = naxes[1];
    long npixels = width * height;

    float *pixels = (float *)malloc(npixels * sizeof(float));
    long firstelem_img[] = {1, 1};
    float nulval = 0.0f;
    int anynul = 0;
    fits_read_pix(fptr, TFLOAT, firstelem_img, npixels, &nulval, pixels, &anynul, &status);

    /* 3. CREATE THE "STAR_DATA" BINARY TABLE EXTENSION (The missing step!) */
    int tfields = 5;
    long nrows = 0; // Starts empty, will expand dynamically row-by-row
    char *ttype[] = {"ID", "NAME", "X_POS", "Y_POS", "FLUX"};
    char *tform[] = {"1J", "20A",  "1E",    "1E",    "1E"};
    char *tunit[] = {"count", "none", "pixel", "pixel", "jansky"};

    // This creates and automatically moves the file focus to the new table HDU
    if (fits_create_tbl(fptr, BINARY_TBL, nrows, tfields, ttype, tform, tunit, "STAR_DATA", &status)) {
        printf("Error: Failed to create the 'STAR_DATA' binary table structure!\n");
        fits_report_error(stderr, status);
        free(pixels);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 4. Scan pixel matrix and write coordinates directly into our new table layout rows */
    float star_threshold = 0.5f;
    int star_count = 0;

    for (long y = 1; y < height - 1; y++) {
        for (long x = 1; x < width - 1; x++) {
            long idx = (y * width) + x;
            float center_val = pixels[idx];

            if (center_val >= star_threshold) {
                if (center_val >= pixels[idx - 1] && center_val >= pixels[idx + 1] &&
                    center_val >= pixels[idx - width] && center_val >= pixels[idx + width] &&
                    center_val >= pixels[idx - width - 1] && center_val >= pixels[idx - width + 1] &&
                    center_val >= pixels[idx + width - 1] && center_val >= pixels[idx + width + 1]) {

                    star_count++;

                    int col_id = star_count;
                    float fits_x = (float)(x + 1);
                    float fits_y = (float)(y + 1);
                    float col_flux = center_val;
                    char col_name[32];
                    sprintf(col_name, "FOUND_STAR_%02d", star_count);

                    /* Write record fields row-by-row into table columns */
                    fits_write_col(fptr, TINT,    1, star_count, 1, 1, &col_id,   &status);

                    char *name_ptr = col_name;
                    fits_write_col(fptr, TSTRING, 2, star_count, 1, 1, &name_ptr, &status);

                    fits_write_col(fptr, TFLOAT,  3, star_count, 1, 1, &fits_x,   &status);
                    fits_write_col(fptr, TFLOAT,  4, star_count, 1, 1, &fits_y,   &status);
                    fits_write_col(fptr, TFLOAT,  5, star_count, 1, 1, &col_flux, &status);
                }
            }
        }
    }

    /* 5. Close the file descriptors safely */
    free(pixels);
    fits_close_file(fptr, &status);

    if (status) {
        fits_report_error(stderr, status);
    } else {
        printf("========================================================\n");
        printf(" STAGE 12 COMPLETE: Table created & data saved!         \n");
        printf(" 'STAR_DATA' extension added to your file.               \n");
        printf(" Wrote %d detected stars directly into FITS Table rows. \n", star_count);
        printf("========================================================\n");
    }

    return status;
}
