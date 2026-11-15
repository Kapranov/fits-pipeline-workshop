#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "fitsio.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int main() {
    fitsfile *f_in, *f_out;
    int status = 0;

    printf("========================================================================\n");
    printf("        STAGE 40: 3D SPATIAL TELESCOPE FIELD ROTATION ENGINE            \n");
    printf("========================================================================\n");
    printf(" Simulating Alt-Azimuth tracking drift via Bilinear Interpolation...\n");

    /* 1. Open the master science file in READONLY mode */
    if (fits_open_file(&f_in, "stage_25_master_observatory_dataset.fits", READONLY, &status)) {
        printf("Error: Could not open master observatory dataset file.\n");
        fits_report_error(stderr, status);
        return status;
    }
    fits_movnam_hdu(f_in, IMAGE_HDU, "STARRY_SKY_CUBE", 0, &status);

    int bitpix, naxis;
    long naxes[] = {0, 0, 0};
    fits_get_img_param(f_in, 3, &bitpix, &naxis, naxes, &status);

    long width      = naxes[0];
    long height     = naxes[1];
    long num_frames = (naxis == 3) ? naxes[2] : 1;
    long npixels_frame = width * height;

    /* 2. Create a BRAND NEW file specifically to store the Rotated Data Cube */
    if (fits_create_file(&f_out, "!stage_40_telescope_rotated_cube.fits", &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        return status;
    }

    /* Create the mandatory empty primary array baseline layer */
    int out_bitpix = 8;
    int out_naxis = 0;
    long out_naxes[] = {0};
    fits_create_img(f_out, out_bitpix, out_naxis, out_naxes, &status);

    /* Append the 3D Image Cube Extension for the rotated output structure */
    long rot_naxes[] = {width, height, num_frames};
    fits_create_img(f_out, FLOAT_IMG, 3, rot_naxes, &status);
    fits_write_key(f_out, TSTRING, "EXTNAME", "ROTATED_SKY", "Field Rotation Simulation", &status);

    /* 3. Allocate temporary single-frame runtime data array buffers */
    float *raw_pixels  = (float *)malloc(npixels_frame * sizeof(float));
    float *rot_pixels  = (float *)malloc(npixels_frame * sizeof(float));

    if (raw_pixels == NULL || rot_pixels == NULL) {
        printf("Memory allocation failed!\n");
        free(raw_pixels); free(rot_pixels);
        fits_close_file(f_in, &status); fits_close_file(f_out, &status);
        return 1;
    }

    double center_x = (double)(width - 1) / 2.0;
    double center_y = (double)(height - 1) / 2.0;
    float nulval = 0.0f;
    int anynul = 0;

    /* 4. Process each frame sequentially over the time-series dimension cube */
    for (int f = 0; f < num_frames; f++) {
        long firstelem_cube[] = {1, 1, f + 1};
        fits_read_pix(f_in, TFLOAT, firstelem_cube, npixels_frame, &nulval, raw_pixels, &anynul, &status);

        /* Calculate incremental angular angle steps per frame (15 degrees per frame shift) */
        double angle_deg = (double)f * 15.0;
        double angle_rad = angle_deg * M_PI / 180.0;
        double cos_a = cos(angle_rad);
        double sin_a = sin(angle_rad);

        /* Sweep every pixel coordinate in the target image array canvas grid */
        for (long y = 0; y < height; y++) {
            for (long x = 0; x < width; x++) {

                /* Map backward coordinate translation matrix relative to center point anchors */
                double dx = (double)x - center_x;
                double dy = (double)y - center_y;

                double orig_x = dx * cos_a + dy * sin_a + center_x;
                double orig_y = -dx * sin_a + dy * cos_a + center_y;

                /* 5. Execute Bilinear Interpolation for smooth pixel rendering */
                int x1 = (int)floor(orig_x);
                int y1 = (int)floor(orig_y);
                int x2 = x1 + 1;
                int y2 = y1 + 1;

                /* Boundary check validation - fill empty space out-of-bounds pixels with dark black (0) */
                if (x1 < 0 || y1 < 0 || x2 >= width || y2 >= height) {
                    rot_pixels[(y * width) + x] = 0.0f;
                    continue;
                }

                double wx = orig_x - (double)x1;
                double wy = orig_y - (double)y1;

                float p11 = raw_pixels[(y1 * width) + x1];
                float p21 = raw_pixels[(y1 * width) + x2];
                float p12 = raw_pixels[(y2 * width) + x1];
                float p22 = raw_pixels[(y2 * width) + x2];

                float interpolated_val = (float)(
                    (1.0 - wx) * (1.0 - wy) * p11 +
                    wx * (1.0 - wy) * p21 +
                    (1.0 - wx) * wy * p12 +
                    wx * wy * p22
                );

                rot_pixels[(y * width) + x] = interpolated_val;
            }
        }

        /* 6. Write the rotated frame slice immediately onto disk storage */
        fits_write_pix(f_out, TFLOAT, firstelem_cube, npixels_frame, rot_pixels, &status);
        printf(" -> Transformed Frame Layer %d / %ld: Rotated matrix canvas smoothly by %0.1f°\n", f + 1, num_frames, angle_deg);
    }

    printf("------------------------------------------------------------------------\n");
    printf(" SUCCESS: 3D field rotation dataset generated successfully!            \n");
    printf(" Saved Output File: stage_40_telescope_rotated_cube.fits                        \n");
    printf("========================================================================\n");

    /* 7. Release resources and close down file handles cleanly */
    free(raw_pixels); free(rot_pixels);
    fits_close_file(f_in, &status);
    fits_close_file(f_out, &status);
    return status;
}
