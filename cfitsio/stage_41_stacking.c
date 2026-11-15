#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main() {
    fitsfile *f_in, *f_out;
    int status = 0;

    printf("========================================================================\n");
    printf("        STAGE 41: TIME-SERIES CO-ADDITION STACKING PIPELINE             \n");
    printf("========================================================================\n");
    printf(" Integrating 3D rotated cube frames into a flat 2D star trail image...\n");

    /* 1. Open the 3D rotated data cube file in READONLY mode */
    if (fits_open_file(&f_in, "stage_40_telescope_rotated_cube.fits", READONLY, &status)) {
        printf("Error: Could not open 'telescope_rotated_cube.fits'.\n");
        fits_report_error(stderr, status);
        return status;
    }
    fits_movnam_hdu(f_in, IMAGE_HDU, "ROTATED_SKY", 0, &status);

    int bitpix, naxis;
    long naxes[] = {0, 0, 0};
    fits_get_img_param(f_in, 3, &bitpix, &naxis, naxes, &status);

    long width      = naxes[0];
    long height     = naxes[1];
    long num_frames = (naxis == 3) ? naxes[2] : 1;
    long npixels_frame = width * height;

    /* 2. Create a BRAND NEW 2D output FITS container file */
    if (fits_create_file(&f_out, "!stage_41_stacked_star_trails.fits", &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        return status;
    }

    /* Create the mandatory empty primary array baseline layer */
    int out_bitpix = 8;
    int out_naxis = 0;
    long out_naxes[] = {0};
    fits_create_img(f_out, out_bitpix, out_naxis, out_naxes, &status);

    /* Append a true 2D Image Extension (NAXIS = 2) for the co-added integrated frame */
    long stack_naxes[] = {width, height};
    fits_create_img(f_out, FLOAT_IMG, 2, stack_naxes, &status);
    fits_write_key(f_out, TSTRING, "EXTNAME", "STAR_TRAILS", "Integrated Exposure Layer", &status);

    /* 3. Allocate dynamic memory arrays for processing */
    float *cube_pixels  = (float *)malloc(npixels_frame * sizeof(float));
    float *stacked_image = (float *)calloc(npixels_frame, sizeof(float)); // Initialized cleanly to 0.0

    if (cube_pixels == NULL || stacked_image == NULL) {
        printf("Memory allocation failed!\n");
        free(cube_pixels); free(stacked_image);
        fits_close_file(f_in, &status); fits_close_file(f_out, &status);
        return 1;
    }

    float nulval = 0.0f;
    int anynul = 0;

    /* 4. Co-add pixel arrays across the time dimension steps */
    for (int f = 0; f < num_frames; f++) {
        long firstelem_cube[] = {1, 1, f + 1};
        fits_read_pix(f_in, TFLOAT, firstelem_cube, npixels_frame, &nulval, cube_pixels, &anynul, &status);

        if (status) {
            printf("Error reading slice data at frame %d\n", f + 1);
            break;
        }

        /* Element-by-element array summation loop */
        for (long idx = 0; idx < npixels_frame; idx++) {
            stacked_image[idx] += cube_pixels[idx];
        }
        printf(" -> Stacked Frame Layer %d / %ld completely integrated.\n", f + 1, num_frames);
    }

    /* 5. Compute the final average mean flux intensity values per pixel */
    for (long idx = 0; idx < npixels_frame; idx++) {
        stacked_image[idx] /= (float)num_frames;
    }

    /* 6. Write the finalized 2D integrated flat science array onto storage sectors */
    long firstelem_out[] = {1, 1};
    fits_write_pix(f_out, TFLOAT, firstelem_out, npixels_frame, stacked_image, &status);

    printf("------------------------------------------------------------------------\n");
    printf(" SUCCESS: 2D integrated co-addition star trail image complete!       \n");
    printf(" Saved Output File: stage_41_stacked_star_trails.fits                           \n");
    printf("========================================================================\n");

    /* 7. Release processing resource handles cleanly */
    free(cube_pixels); free(stacked_image);
    fits_close_file(f_in, &status);
    fits_close_file(f_out, &status);
    return status;
}
