#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;

    /* 1. Open the 3D time cube file */
    if (fits_open_file(&fptr, "stage_25_master_observatory_dataset.fits", READONLY, &status)) {
        printf("Error: Could not open the master dataset file.\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Target the STARRY_SKY_CUBE extension layer */
    if (fits_movnam_hdu(fptr, IMAGE_HDU, "STARRY_SKY_CUBE", 0, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    int bitpix, naxis;
    long naxes[] = {0, 0, 0};
    fits_get_img_param(fptr, 3, &bitpix, &naxis, naxes, &status);

    long width = naxes[0];
    long height = naxes[1];
    long num_frames = naxes[2];
    long npixels_frame = width * height;

    float *pixels = (float *)malloc(npixels_frame * sizeof(float));
    float star_threshold = 0.5f;
    float nulval = 0.0f;
    int anynul = 0;

    printf("========================================================================\n");
    printf("        STAGE 26: SUB-PIXEL FRACTIONAL CENTROID TRACKING ENGINE        \n");
    printf("========================================================================\n");
    printf(" Target Layer: STARRY_SKY_CUBE | Slices Processed: %ld frames\n", num_frames);
    printf("------------------------------------------------------------------------\n");
    printf("  Frame  |  Star ID  |  Peak X/Y (int)  |   Centroid X   |   Centroid Y   | Peak Flux \n");
    printf("------------------------------------------------------------------------\n");

    /* 3. Loop sequentially through the time dimension frames slices */
    for (int f = 0; f < num_frames; f++) {
        long firstelem_cube[] = {1, 1, f + 1};
        fits_read_pix(fptr, TFLOAT, firstelem_cube, npixels_frame, &nulval, pixels, &anynul, &status);

        int star_count = 0;

        for (long y = 2; y < height - 2; y++) {
            for (long x = 2; x < width - 2; x++) {
                long idx = (y * width) + x;
                float center_val = pixels[idx];

                if (center_val >= star_threshold) {
                    /* Identify the integer local peak maximum */
                    if (center_val >= pixels[idx - 1] && center_val >= pixels[idx + 1] &&
                        center_val >= pixels[idx - width] && center_val >= pixels[idx + width] &&
                        center_val >= pixels[idx - width - 1] && center_val >= pixels[idx - width + 1] &&
                        center_val >= pixels[idx + width - 1] && center_val >= pixels[idx + width + 1]) {

                        star_count++;

                        /* --- SUB-PIXEL 3x3 INTENSITY CENTROID CALCULATION --- */
                        double sum_w = 0.0;
                        double sum_wx = 0.0;
                        double sum_wy = 0.0;

                        for (long j = -1; j <= 1; j++) {
                            for (long i = -1; i <= 1; i++) {
                                long n_idx = ((y + j) * width) + (x + i);
                                double intensity = (double)pixels[n_idx];

                                /* Use intensity directly as a weighting factor */
                                sum_w  += intensity;
                                sum_wx += intensity * (double)(x + i + 1); // 1-based indexing map
                                sum_wy += intensity * (double)(y + j + 1);
                            }
                        }

                        double centroid_x = (sum_w > 0.0) ? (sum_wx / sum_w) : (double)(x + 1);
                        double centroid_y = (sum_w > 0.0) ? (sum_wy / sum_w) : (double)(y + 1);
                        /* --------------------------------------------------- */

                        printf("   %02d    |    %02d     |   (%3ld, %3ld)   |    %9.4f   |    %9.4f   |  %7.4f\n",
                               f + 1, star_count, x + 1, y + 1, centroid_x, centroid_y, center_val);
                    }
                }
            }
        }
    }
    printf("========================================================================\n");

    free(pixels);
    fits_close_file(fptr, &status);
    return status;
}
