#include <stdio.h>
#include "fitsio.h"

// Reads the raw 2D short integer matrix directly from the FITS file
void c_read_image(const char* filename, short* data, int nx, int ny, int* status) {
    fitsfile *fptr;
    long fpixel[] = {1, 1};
    int anynul;

    if (fits_open_file(&fptr, filename, READONLY, status)) return;

    if (fits_read_pix(fptr, TSHORT, fpixel, (long)nx * (long)ny, NULL, data, &anynul, status)) {
        fits_close_file(fptr, status);
        return;
    }

    fits_close_file(fptr, status);
}

// Reads an Integer keyword value safely from the header (matching ftpkyj)
void c_read_int_key(const char* filename, const char* keyname, int* value, int* status) {
    fitsfile *fptr;
    int intval;
    if (fits_open_file(&fptr, filename, READONLY, status)) return;

    fits_read_key(fptr, TINT, keyname, &intval, NULL, status);
    *value = intval;
    fits_close_file(fptr, status);
}

// NEW: Dumps structural details and prints every single header card
void c_dump_all_headers(const char* filename, int* status) {
    fitsfile *fptr;
    int bitpix, naxis;
    long naxes[2] = {0, 0};
    int nkeys, keynum;
    char card[81];

    if (fits_open_file(&fptr, filename, READONLY, status)) return;

    // 1. Get image structural characteristics
    if (!fits_get_img_param(fptr, 2, &bitpix, &naxis, naxes, status)) {
        printf("\n=================== FITS FILE STRUCT INFO ===================\n");
        printf("Bitpix (Data Type):  %d ", bitpix);
        if (bitpix == 8)       printf("(Character/8-bit unsigned int)\n");
        else if (bitpix == 16)  printf("(16-bit short integer)\n");
        else if (bitpix == 32)  printf("(32-bit integer)\n");
        else if (bitpix == -32) printf("(32-bit floating point real)\n");
        else if (bitpix == -64) printf("(64-bit double precision real)\n");

        printf("Naxis (Dimensions):  %d\n", naxis);
        for (int i = 0; i < naxis && i < 2; i++) {
            printf("Dimension naxes[%d]:  %ld\n", i + 1, naxes[i]);
        }
    }

    // 2. Get total header count and loop over every card record
    fits_get_hdrspace(fptr, &nkeys, NULL, status);
    printf("\n==================== ALL HEADER KEYWORDS ====================\n");
    for (keynum = 1; keynum <= nkeys; keynum++) {
        if (fits_read_record(fptr, keynum, card, status)) break;
        printf("%03d: %s\n", keynum, card);
    }
    printf("=============================================================\n\n");

    fits_close_file(fptr, status);
}
