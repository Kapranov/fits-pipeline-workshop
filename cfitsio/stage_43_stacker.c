#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main() {
    fitsfile *f_in, *f_out;
    int status = 0;

    printf("========================================================\n");
    printf("     STAGE 43: 3D CO-ADDITION IMAGE STACKING ENGINE     \n");
    printf("========================================================\n");
    printf(" Combining frame vectors to eliminate background noise floor...\n");

    /* 1. Open the de-rotated 3D data cube file */
    if (fits_open_file(&f_in, "stage_42_derotated_observatory_cube.fits", READONLY, &status)) {
        printf("Error: Could not open 'stage_42_derotated_observatory_cube.fits'.\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Target the DEROTATED_SKY extension layer */
    if (fits_movnam_hdu(f_in, IMAGE_HDU, "DEROTATED_SKY", 0, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        return status;
    }

    /* 3. Read 3D image tracking properties and dimensions */
    int bitpix, naxis;
    long naxes[] = {0, 0, 0};
    fits_get_img_param(f_in, 3, &bitpix, &naxis, naxes, &status);

    long width = naxes[0];
    long height = naxes[1];
    long num_frames = naxes[2];
    long npixels_frame = width * height;

    /* 4. Allocate frame buffers */
    float *pixels = (float *)malloc(npixels_frame * sizeof(float));
    double *stack_accumulator = (double *)calloc(npixels_frame, sizeof(double));
    float *final_stacked_frame = (float *)malloc(npixels_frame * sizeof(float));

    if (pixels == NULL || stack_accumulator == NULL || final_stacked_frame == NULL) {
        printf("Error: Memory allocation failed!\n");
        fits_close_file(f_in, &status);
        return 1;
    }

    float nulval = 0.0f;
    int anynul = 0;

    /* 5. Sequentially parse the cube and accumulate pixel matrices */
    for (int f = 0; f < num_frames; f++) {
        long firstelem_cube[] = {1, 1, f + 1};

        if (fits_read_pix(f_in, TFLOAT, firstelem_cube, npixels_frame, &nulval, pixels, &anynul, &status)) {
            printf("Error reading slice data at frame %d\n", f + 1);
            fits_report_error(stderr, status);
            break;
        }

        for (long idx = 0; idx < npixels_frame; idx++) {
            stack_accumulator[idx] += (double)pixels[idx];
        }
        printf(" -> Accumulated frame slice %d / %ld into stacking buffer.\n", f + 1, num_frames);
    }
    fits_close_file(f_in, &status);

    /* 6. Compute the mean average grid */
    for (long idx = 0; idx < npixels_frame; idx++) {
        final_stacked_frame[idx] = (float)(stack_accumulator[idx] / (double)num_frames);
    }

    /* 7. Create a clean, single-HDU 2D Master Image file */
    if (fits_create_file(&f_out, "!stage_43_stacked_master_image.fits", &status)) {
        fits_report_error(stderr, status);
        free(pixels); free(stack_accumulator); free(final_stacked_frame);
        return status;
    }

    int out_bitpix = FLOAT_IMG;
    int out_naxis = 2;
    long out_naxes[] = {width, height};

    // FIXED: Explicitly initialize HDU 0 exactly ONCE to avoid buffer layout corruption
    fits_create_img(f_out, out_bitpix, out_naxis, out_naxes, &status);
    fits_write_comment(f_out, "Master Co-added 2D image collapsed from 3D time series.", &status);

    /* 8. Write the pixel matrix safely to HDU 0 */
    long firstelem_output[] = {1, 1};
    if (fits_write_pix(f_out, TFLOAT, firstelem_output, npixels_frame, final_stacked_frame, &status)) {
        printf("Error: Failed to write pixels to disk layout!\n");
        fits_report_error(stderr, status);
    } else {
        printf("--------------------------------------------------------\n");
        printf(" SUCCESS: Stacking engine finalized completely!       \n");
        printf(" Generated Master File: stage_43_stacked_master_image.fits\n");
        printf("========================================================\n");
    }

    /* 9. Clean up memory allocations */
    free(pixels);
    free(stack_accumulator);
    free(final_stacked_frame);
    fits_close_file(f_out, &status);

    return status;
}
