#include <stdio.h>
#include "fitsio.h"

// Dynamically fetches the image dimensions from a specific HDU
void c_get_image_dims(const char* filename, int hdu_num, int* nx, int* ny, int* status) {
    fitsfile *fptr;
    int bitpix, naxis;
    long naxes[2] = {0, 0};
    int hdutype;

    if (fits_open_file(&fptr, filename, READONLY, status)) return;

    // Move to the requested HDU (1-indexed: 1 = Primary, 2 = First Extension)
    if (fits_movabs_hdu(fptr, hdu_num, &hdutype, status)) {
        fits_close_file(fptr, status);
        return;
    }

    // Verify it is an image extension before parsing dimensions
    if (hdutype != IMAGE_HDU) {
        *status = NOT_IMAGE; // CFITSIO error code for non-image HDUs
        fits_close_file(fptr, status);
        return;
    }

    if (!fits_get_img_param(fptr, 2, &bitpix, &naxis, naxes, status)) {
        *nx = (int)naxes[0];
        *ny = (int)naxes[1];
    }
    fits_close_file(fptr, status);
}

// Reads the raw 2D floating-point matrix from a specific HDU
void c_read_image_float(const char* filename, int hdu_num, float* data, int nx, int ny, int* status) {
    fitsfile *fptr;
    long fpixel[] = {1, 1};
    int anynul, hdutype;

    if (fits_open_file(&fptr, filename, READONLY, status)) return;

    // Move to the requested HDU
    if (fits_movabs_hdu(fptr, hdu_num, &hdutype, status)) {
        fits_close_file(fptr, status);
        return;
    }

    if (fits_read_pix(fptr, TFLOAT, fpixel, (long)nx * (long)ny, NULL, data, &anynul, status)) {
        fits_close_file(fptr, status);
        return;
    }

    fits_close_file(fptr, status);
}

// Reads an Integer keyword value from a specific HDU
void c_read_int_key(const char* filename, int hdu_num, const char* keyname, int* value, int* status) {
    fitsfile *fptr;
    int intval, hdutype;
    if (fits_open_file(&fptr, filename, READONLY, status)) return;

    // Move to the requested HDU
    if (fits_movabs_hdu(fptr, hdu_num, &hdutype, status)) {
        fits_close_file(fptr, status);
        return;
    }

    fits_read_key(fptr, TINT, keyname, &intval, NULL, status);
    *value = intval;
    fits_close_file(fptr, status);
}
