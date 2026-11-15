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

    /* --- INJECT WORLD COORDINATE SYSTEM (WCS) METADATA --- */
    // Let's center our image at RA = 19h 25m 30s (291.375 deg), DEC = +42d 47m 00s (42.7833 deg)
    double crval1 = 291.3750;  /* Reference RA (deg) */
    double crval2 = 42.7833;   /* Reference DEC (deg) */

    // Set the reference pixel to the exact physical center of our 256x256 image
    double crpix1 = 128.5;     /* Center X pixel index location */
    double crpix2 = 128.5;     /* Center Y pixel index location */

    // Pixel plate scale: 0.25 arcseconds per pixel converted to degrees (0.25 / 3600)
    double cdelt1 = -0.00006944; /* RA pixel step (Negative: RA increases right-to-left) */
    double cdelt2 =  0.00006944; /* DEC pixel step */

    fits_write_key(fptr, TSTRING, "CTYPE1", "RA---TAN", "Coordinate type / Tangent Projection", &status);
    fits_write_key(fptr, TSTRING, "CTYPE2", "DEC--TAN", "Coordinate type / Tangent Projection", &status);

    fits_write_key(fptr, TDOUBLE, "CRVAL1", &crval1, "Ref value of primary axis (deg)", &status);
    fits_write_key(fptr, TDOUBLE, "CRVAL2", &crval2, "Ref value of secondary axis (deg)", &status);

    fits_write_key(fptr, TDOUBLE, "CRPIX1", &crpix1, "Ref pixel location of primary axis", &status);
    fits_write_key(fptr, TDOUBLE, "CRPIX2", &crpix2, "Ref pixel location of secondary axis", &status);

    fits_write_key(fptr, TDOUBLE, "CDELT1", &cdelt1, "Spatial increment per pixel (deg)", &status);
    fits_write_key(fptr, TDOUBLE, "CDELT2", &cdelt2, "Spatial increment per pixel (deg)", &status);

    fits_write_key(fptr, TSTRING, "CUNIT1", "deg", "Physical units of primary axis", &status);
    fits_write_key(fptr, TSTRING, "CUNIT2", "deg", "Physical units of secondary axis", &status);
    /* ----------------------------------------------------- */

    /* 3. CREATE THE "STAR_DATA" BINARY TABLE EXTENSION */
    // We add 2 extra columns for RA and DEC sky coordinate values!
    int tfields = 7;
    long nrows = 0;
    char *ttype[] = {"ID", "NAME", "X_POS", "Y_POS", "RA", "DEC", "FLUX"};
    char *tform[] = {"1J", "20A",  "1E",    "1E",    "1D", "1D",  "1E"};
    char *tunit[] = {"count", "none", "pixel", "pixel", "deg", "deg", "jansky"};

    if (fits_create_tbl(fptr, BINARY_TBL, nrows, tfields, ttype, tform, tunit, "STAR_DATA", &status)) {
        printf("Error: Failed to create table structure!\n");
        fits_report_error(stderr, status);
        free(pixels);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 4. Scan pixel matrix, compute RA/DEC using WCS, and save rows */
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
                    char col_name[20];
                    sprintf(col_name, "FOUND_STAR_%02d", star_count);

                    /* --- WCS LINEAR SKY TRANSLATION MATH --- */
                    // Basic linear mapping transformation: Sky_Coord = CRVAL + CDELT * (Pixel_Coord - CRPIX)
                    double star_ra  = crval1 + cdelt1 * ((double)fits_x - crpix1);
                    double star_dec = crval2 + cdelt2 * ((double)fits_y - crpix2);
                    /* -------------------------------------- */

                    /* Write record fields row-by-row into table columns */
                    fits_write_col(fptr, TINT,    1, star_count, 1, 1, &col_id,   &status);
                    char *name_ptr = col_name;
                    fits_write_col(fptr, TSTRING, 2, star_count, 1, 1, &name_ptr, &status);
                    fits_write_col(fptr, TFLOAT,  3, star_count, 1, 1, &fits_x,   &status);
                    fits_write_col(fptr, TFLOAT,  4, star_count, 1, 1, &fits_y,   &status);

                    // Write new computed WCS positioning coordinates
                    fits_write_col(fptr, TDOUBLE, 5, star_count, 1, 1, &star_ra,  &status);
                    fits_write_col(fptr, TDOUBLE, 6, star_count, 1, 1, &star_dec, &status);

                    fits_write_col(fptr, TFLOAT,  7, star_count, 1, 1, &col_flux, &status);
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
        printf(" STAGE 14 COMPLETE: WCS Added & Stars Tracked!           \n");
        printf(" Wrote %d detected stars with RA/DEC positions to table. \n", star_count);
        printf("========================================================\n");
    }

    return status;
}
