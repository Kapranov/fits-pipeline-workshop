#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;

    /* 1. Open the 3D data cube file */
    if (fits_open_file(&fptr, "stage_17_time_cube.fits", READONLY, &status)) {
        printf("Error: Could not open 'stage_17_time_cube.fits'.\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Move to the STARRY_SKY_CUBE extension layer */
    if (fits_movnam_hdu(fptr, IMAGE_HDU, "STARRY_SKY_CUBE", 0, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 3. Read image parameters */
    int bitpix, naxis;
    long naxes[] = {0, 0, 0}; /* FIXED: Added array brackets [] */
    if (fits_get_img_param(fptr, 3, &bitpix, &naxis, naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    long width = naxes[0];
    long height = naxes[1];
    long num_frames = (naxis == 3) ? naxes[2] : 1;
    long npixels_frame = width * height;

    /* 4. Allocate 2D plane pixel memory buffer */
    float *pixels = (float *)malloc(npixels_frame * sizeof(float));
    if (pixels == NULL) {
        printf("Memory allocation failed!\n");
        fits_close_file(fptr, &status);
        return 1;
    }

    float star_threshold = 0.5f;
    float nulval = 0.0f;
    int anynul = 0;

    printf("========================================================================\n");
    printf("         STAGE 19: 3D SPACE-TIME STELLAR DRIFT MULTI-TRACKER            \n");
    printf("========================================================================\n");
    printf(" Processing: %ld frames | Resolution: %ld x %ld\n", num_frames, width, height);
    printf("------------------------------------------------------------------------\n");
    printf("  Frame ID  |   Star ID   |   X Position   |   Y Position   |   Peak Flux \n");
    printf("------------------------------------------------------------------------\n");

    /* 5. Loop sequentially through the time dimension frames slices */
    for (int f = 0; f < num_frames; f++) {
        // FIXED: Added array brackets [] for 3-axis indexing coordinate tracking
        long firstelem_cube[] = {1, 1, f + 1};

        if (fits_read_pix(fptr, TFLOAT, firstelem_cube, npixels_frame, &nulval, pixels, &anynul, &status)) {
            printf("Error reading slice data at frame %d\n", f + 1);
            fits_report_error(stderr, status);
            break;
        }

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

                        printf("     %02d     |     %02d      |     %7.2f    |     %7.2f    |   %8.4f\n",
                               f + 1, star_count, (float)(x + 1), (float)(y + 1), center_val);
                    }
                }
            }
        }
    }

    printf("========================================================================\n");
    printf(" TRACKING COMPLETE: All time slices processed cleanly.                   \n");
    printf("========================================================================\n");

    /* 6. Release resources and close down file handles safely */
    free(pixels);
    fits_close_file(fptr, &status);
    return status;
}
