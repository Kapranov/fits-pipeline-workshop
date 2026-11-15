#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;

    /* 1. Open the 3D convolved focus blur file */
    if (fits_open_file(&fptr, "stage_38_telescope_blurred_cube.fits", READONLY, &status)) {
        printf("Error: Could not open the blurred dataset file.\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Target the BLURRED_SKY image extension layer */
    if (fits_movnam_hdu(fptr, IMAGE_HDU, "BLURRED_SKY", 0, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    int bitpix, naxis;
    long naxes[] = {0, 0, 0};
    fits_get_img_param(fptr, 3, &bitpix, &naxis, naxes, &status);

    long width      = naxes[0];
    long height     = naxes[1];
    long num_frames = (naxis == 3) ? naxes[2] : 1;
    long npixels_frame = width * height;

    float *pixels = (float *)malloc(npixels_frame * sizeof(float));

    /* CRITICAL PARAMETER CHANGE: The peak intensities dropped because of convolution,
       so we lower our detection threshold to 0.2f to catch the expanded profiles cleanly */
    float star_threshold = 0.2f;
    float nulval = 0.0f;
    int anynul = 0;

    printf("========================================================================\n");
    printf("     STAGE 39: SUB-PIXEL FRACTIONAL TRACKING ON DEFOCUSED IMAGES       \n");
    printf("========================================================================\n");
    printf(" Target Layer: BLURRED_SKY | Slices Processed: %ld frames\n", num_frames);
    printf(" Lowered Extraction Threshold: %0.1f\n", star_threshold);
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

                                sum_w  += intensity;
                                sum_wx += intensity * (double)(x + i + 1); // Convert to 1-based indexing map
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
