#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;

    /* 1. Create a fresh file (Overwrites if exists using '!') */
    if (fits_create_file(&fptr, "!stage_04_create_image.fits", &status)) {
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Primary HDU (Blank) */
    int bitpix = 8;
    int naxis = 0;
    long naxes[1] = {0};
    if (fits_create_img(fptr, bitpix, naxis, naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 3. Add Binary Table Extension */
    int tfields = 3;
    long nrows = 0;
    char *ttype[] = {"ID", "NAME", "FLUX"};
    char *tform[] = {"1J", "20A", "1E"};
    char *tunit[] = {"count", "none", "jansky"};
    if (fits_create_tbl(fptr, BINARY_TBL, nrows, tfields, ttype, tform, tunit, "STAR_DATA", &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 4. Append Image Extension right after */
    int img_bitpix = FLOAT_IMG;
    int img_naxis = 2;
    long img_naxes[2] = {100, 100};
    if (fits_create_img(fptr, img_bitpix, img_naxis, img_naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }
    fits_write_key(fptr, TSTRING, "EXTNAME", "GRADIENT_IMG", "Extension name", &status);

    /* 5. Fill Image Array Data */
    long npixels = img_naxes[0] * img_naxes[1];
    float *pixels = (float *)malloc(npixels * sizeof(float));
    if (pixels) {
        for (int y = 0; y < img_naxes[1]; y++) {
            for (int x = 0; x < img_naxes[0]; x++) {
                pixels[y * img_naxes[0] + x] = (float)(x + y) / 198.0f;
            }
        }
        fits_write_img(fptr, TFLOAT, 1, npixels, pixels, &status);
        free(pixels);
    }

    /* 6. Safely Flush & Close */
    fits_close_file(fptr, &status);
    if (status) {
        fits_report_error(stderr, status);
    } else {
        printf("Success! Generated 'stahe_04_create_image.fits' containing both extensions.\n");
    }

    return status;
}
