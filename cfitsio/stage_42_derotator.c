#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "fitsio.h"

#define M_PI 3.14159265358979323846

int main() {
    fitsfile *f_in, *f_out;
    int status = 0;

    printf("========================================================\n");
    printf("     STAGE 42: ASTRO-FIELD DE-ROTATION FILTER ENGINE   \n");
    printf("========================================================\n");

    /* 1. Open the raw master observatory 3D data cube */
    if (fits_open_file(&f_in, "stage_25_master_observatory_dataset.fits", READONLY, &status)) {
        printf("Error: Could not open master archive dataset.\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Target the STARRY_SKY_CUBE extension layer */
    if (fits_movnam_hdu(f_in, IMAGE_HDU, "STARRY_SKY_CUBE", 0, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        return status;
    }

    /* 3. Extract dimensions from the 3D FITS profile */
    int bitpix, naxis;
    long naxes[] = {0, 0, 0};
    fits_get_img_param(f_in, 3, &bitpix, &naxis, naxes, &status);

    long width = naxes[0];
    long height = naxes[1];
    long num_frames = naxes[2];
    long npixels_frame = width * height;

    /* 4. Create a BRAND NEW FITS output container for the de-rotated cube */
    if (fits_create_file(&f_out, "!stage_42_derotated_observatory_cube.fits", &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        return status;
    }

    /* Mandatory primary empty baseline layout creation */
    int out_bitpix = FLOAT_IMG;
    int out_naxis = 3;
    fits_create_img(f_out, out_bitpix, out_naxis, naxes, &status);
    fits_write_key(f_out, TSTRING, "EXTNAME", "DEROTATED_SKY", "De-rotated Time Series", &status);

    double crval1 = 291.3750, crval2 = 42.7833;
    fits_write_key(f_out, TDOUBLE, "CRVAL1", &crval1, "Ref value RA", &status);
    fits_write_key(f_out, TDOUBLE, "CRVAL2", &crval2, "Ref value DEC", &status);

    /* REMOVED: fits_copy_header loop completely dropped to prevent dimension corruption */

    /* 5. Allocate frame buffer array memory size cushions */
    float *in_pixels = (float *)malloc(npixels_frame * sizeof(float));
    float *out_pixels = (float *)malloc(npixels_frame * sizeof(float));
    if (in_pixels == NULL || out_pixels == NULL) {
        printf("Memory allocation failed!\n");
        return 1;
    }

    float nulval = 0.0f;
    int anynul = 0;
    double center_x = (double)width / 2.0 - 0.5;
    double center_y = (double)height / 2.0 - 0.5;

    double rotation_rate_deg = 1.5;

    /* 6. Process each frame sequentially over the time-series dimension cube */
    for (int f = 0; f < num_frames; f++) {
        long firstelem_cube[] = {1, 1, f + 1};
        fits_read_pix(f_in, TFLOAT, firstelem_cube, npixels_frame, &nulval, in_pixels, &anynul, &status);

        double angle_deg = (double)f * rotation_rate_deg;
        double angle_rad = angle_deg * (M_PI / 180.0);
        double cos_a = cos(angle_rad);
        double sin_a = sin(angle_rad);

        /* 7. Run reverse geometric mapping transformation grid */
        for (long y = 0; y < height; y++) {
            for (long x = 0; x < width; x++) {
                long out_idx = (y * width) + x;

                double dx = (double)x - center_x;
                double dy = (double)y - center_y;

                int src_x = (int)round(dx * cos_a + dy * sin_a + center_x);
                int src_y = (int)round(-dx * sin_a + dy * cos_a + center_y);

                if (src_x >= 0 && src_x < width && src_y >= 0 && src_y < height) {
                    out_pixels[out_idx] = in_pixels[(src_y * width) + src_x];
                } else {
                    out_pixels[out_idx] = ((float)rand() / (float)RAND_MAX) * 0.01f;
                }
            }
        }

        fits_write_pix(f_out, TFLOAT, firstelem_cube, npixels_frame, out_pixels, &status);
        printf(" -> Frame %d / %ld aligned successfully (Rotated backward by %0.2f°)\n", f + 1, num_frames, angle_deg);
    }

    free(in_pixels);
    free(out_pixels);
    fits_close_file(f_in, &status);
    fits_close_file(f_out, &status);

    if (status) {
        printf("An error occurred during de-rotation filtering.\n");
        fits_report_error(stderr, status);
    } else {
        printf("--------------------------------------------------------\n");
        printf(" SUCCESS: 3D Field De-rotation Filter Complete!          \n");
        printf(" Output Generated: stage_42_derotated_observatory_cube.fits\n");
        printf("========================================================\n");
    }
    return status;
}
