#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fitsio.h"

#define MAX_STARS 1000

int main() {
    fitsfile *fptr = NULL;
    int status = 0;
    int ii, jj;

    // --- HDU 1: Image Gradient Parameters ---
    long fpixel = 1, naxis = 2;
    long naxes[2] = { 300, 200 };
    long nelements = naxes[0] * naxes[1];

    short *img_array = (short *)malloc(nelements * sizeof(short));
    if (!img_array) {
        fprintf(stderr, "Image allocation failed\n");
        return 1;
    }
    for (jj = 0; jj < naxes[1]; jj++) {
        for (ii = 0; ii < naxes[0]; ii++) {
            img_array[jj * naxes[0] + ii] = ii + jj;
        }
    }

    // --- HDU 2: ASCII Table (Text Payload) Parameters ---
    char *txt_ttype[] = { "TEXT_LINE" };
    char *txt_tform[] = { "A40" };
    char *txt_tunit[] = { " " };
    char text_payload[] = "Hello world";
    char *txt_ptr[] = { text_payload };

    // --- HDU 3: Binary Table Parameters ---
    long star_hips[MAX_STARS];
    char star_names[MAX_STARS][64];
    float star_v_mags[MAX_STARS];
    float star_mv_mags[MAX_STARS];
    float star_dists[MAX_STARS];
    long star_charts[MAX_STARS];
    int star_count = 0;

    FILE *dat_file = fopen("closest_stars.dat", "r");
    if (dat_file != NULL) {
        char line[256];
        if (fgets(line, sizeof(line), dat_file) != NULL) { /* Skip header */ }

        while (fgets(line, sizeof(line), dat_file) && star_count < MAX_STARS) {
            line[strcspn(line, "\r\n")] = 0;
            if (strlen(line) == 0) continue;

            char *token = strtok(line, "\t");
            int col_idx = 0;

            while (token != NULL) {
                while (*token == ' ') token++;
                char *end = token + strlen(token) - 1;
                while (end > token && *end == ' ') { *end = '\0'; end--; }

                if (col_idx == 0) star_hips[star_count] = atol(token);
                else if (col_idx == 1) {
                    if (strlen(token) == 0) strcpy(star_names[star_count], "Unnamed");
                    else strncpy(star_names[star_count], token, 63);
                }
                else if (col_idx == 2) star_v_mags[star_count] = (float)atof(token);
                else if (col_idx == 3) star_mv_mags[star_count] = (float)atof(token);
                else if (col_idx == 4) star_dists[star_count] = (float)atof(token);
                else if (col_idx == 5) star_charts[star_count] = atol(token);

                col_idx++;
                token = strtok(NULL, "\t");
            }
            if (col_idx >= 5) star_count++;
        }
        fclose(dat_file);
    }

    char *tbl_ttype[] = { "HIP", "Name", "V", "M_V", "Distance", "Chart" };
    char *tbl_tform[] = { "1J",  "64A",  "1E", "1E",  "1E",       "1J" };
    char *tbl_tunit[] = { " ",   " ",    "mag","mag", "ly",       " " };

    // ==========================================
    // EXECUTION STEP 1: Main Container & HDU 1
    // ==========================================
    if (fits_create_file(&fptr, "!demo_wcs.fits", &status)) {
        fits_report_error(stderr, status);
        free(img_array);
        return status;
    }

    // Write Primary Image HDU (The Gradient)
    fits_create_img(fptr, SHORT_IMG, naxis, naxes, &status);
    fits_write_img(fptr, TSHORT, fpixel, nelements, img_array, &status);
    free(img_array);

    // -------------------------------------------------------------
    // AUTOMATED WCS MAPPING INJECTION (For Primary Gradient Image)
    // -------------------------------------------------------------
    /* Coordinate Type: RA and DEC using Gnomonic (TAN) projection */
    fits_update_key(fptr, TSTRING, "CTYPE1", "RA---TAN", "Coordinate type for Axis 1", &status);
    fits_update_key(fptr, TSTRING, "CTYPE2", "DEC--TAN", "Coordinate type for Axis 2", &status);

    /* Reference Pixel: Center of our 300x200 pixel matrix */
    double crpix1 = 150.0;
    double crpix2 = 100.0;
    fits_update_key(fptr, TDOUBLE, "CRPIX1", &crpix1, "Reference pixel on Axis 1", &status);
    fits_update_key(fptr, TDOUBLE, "CRPIX2", &crpix2, "Reference pixel on Axis 2", &status);

    /* Reference Coordinates: Pointing at Alpha Centauri's general patch (RA: 219.9°, Dec: -60.8°) */
    double crval1 = 219.9014;
    double crval2 = -60.8339;
    fits_update_key(fptr, TDOUBLE, "CRVAL1", &crval1, "RA at reference pixel (deg)", &status);
    fits_update_key(fptr, TDOUBLE, "CRVAL2", &crval2, "DEC at reference pixel (deg)", &status);

    /* Pixel Scale Matrix: 0.00027778 degrees per pixel (translates to 1 arcsecond per pixel) */
    /* Negative CDELT1 is standard convention to flip RA so West is to the right */
    double cdelt1 = -0.00027778;
    double cdelt2 =  0.00027778;
    fits_update_key(fptr, TDOUBLE, "CDELT1", &cdelt1, "Pixel scale Axis 1 (deg/pixel)", &status);
    fits_update_key(fptr, TDOUBLE, "CDELT2", &cdelt2, "Pixel scale Axis 2 (deg/pixel)", &status);

    /* Coordinate System Epoch */
    fits_update_key(fptr, TSTRING, "RADECSYS", "FK5", "Coordinate system framework", &status);
    double equinox = 2000.0;
    fits_update_key(fptr, TDOUBLE, "EQUINOX", &equinox, "Equinox of coordinates", &status);
    // -------------------------------------------------------------

    // ==========================================
    // EXECUTION STEP 2: HDU 2 (ASCII Text)
    // ==========================================
    fits_create_tbl(fptr, ASCII_TBL, 1, 1, txt_ttype, txt_tform, txt_tunit, "TEXT_MESSAGE", &status);
    fits_write_col(fptr, TSTRING, 1, 1, 1, 1, txt_ptr, &status);

    // ==========================================
    // EXECUTION STEP 3: HDU 3 (The Parsed Table)
    // ==========================================
    fits_create_tbl(fptr, BINARY_TBL, star_count, 6, tbl_ttype, tbl_tform, tbl_tunit, "CLOSEST_STARS", &status);

    if (star_count > 0) {
        char *name_ptrs[MAX_STARS];
        for (ii = 0; ii < star_count; ii++) { name_ptrs[ii] = star_names[ii]; }

        fits_write_col(fptr, TLONG,   1, 1, 1, star_count, star_hips, &status);
        fits_write_col(fptr, TSTRING, 2, 1, 1, star_count, name_ptrs, &status);
        fits_write_col(fptr, TFLOAT,  3, 1, 1, star_count, star_v_mags, &status);
        fits_write_col(fptr, TFLOAT,  4, 1, 1, star_count, star_mv_mags, &status);
        fits_write_col(fptr, TFLOAT,  5, 1, 1, star_count, star_dists, &status);
        fits_write_col(fptr, TLONG,   6, 1, 1, star_count, star_charts, &status);
    }

    // ==========================================
    // EXECUTION STEP 4: Close File Handler
    // ==========================================
    fits_close_file(fptr, &status);

    if (status) {
        fits_report_error(stderr, status);
    } else {
        printf("Success! Created WCS mapped file containing %d star rows.\n", star_count);
    }

    return status;
}
