#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;

    /* 1. Open the master repository file in READONLY mode */
    if (fits_open_file(&fptr, "stage_25_master_observatory_dataset.fits", READONLY, &status)) {
        printf("Error: Could not open master archive dataset file.\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Target the STARRY_SKY_CUBE image extension layer (HDU 2) */
    if (fits_movnam_hdu(fptr, IMAGE_HDU, "STARRY_SKY_CUBE", 0, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 3. DEFINE THE 3D BOUNDING BOX BOUNDARIES
       Let's isolate a 5x5 pixel window centered at X=28, Y=48 across all 5 frames.
       X range: 26 to 30  |  Y range: 46 to 50  |  Frame range: 1 to 5
       Remember: FITS maps coordinates using 1-based indexing! */
    long fpixels[] = {26, 46, 1};  /* Starting pixel coordinate vector [X, Y, Frame] */
    long lpixels[] = {30, 50, 5};  /* Ending pixel coordinate vector   [X, Y, Frame] */
    long inc[]     = {1, 1, 1};    /* Step increment step size (read every pixel continuously) */

    long box_width  = lpixels[0] - fpixels[0] + 1;
    long box_height = lpixels[1] - fpixels[1] + 1;
    long num_frames = lpixels[2] - fpixels[2] + 1;
    long total_subset_pixels = box_width * box_height * num_frames;

    /* 4. Allocate a localized small buffer array for our 5x5x5 cutout */
    float *subset_buffer = (float *)malloc(total_subset_pixels * sizeof(float));
    if (subset_buffer == NULL) {
        printf("Memory allocation failed!\n");
        fits_close_file(fptr, &status);
        return 1;
    }

    float nulval = 0.0f;
    int anynul = 0;

    /* 5. Extract the 3D data block cutout from disk storage */
    if (fits_read_subset(fptr, TFLOAT, fpixels, lpixels, inc, &nulval, subset_buffer, &anynul, &status)) {
        printf("Error extracting the localized data subset.\n");
        fits_report_error(stderr, status);
    } else {
        printf("========================================================================\n");
        printf("       SUB-ARRAY SECTOR REPORT: FAST REGION OF INTEREST (ROI) READ      \n");
        printf("========================================================================\n");
        printf(" Target Center Coordinates : X = 28, Y = 48\n");
        printf(" Bounding Sub-Array Size   : %ld x %ld pixels cutout window\n", box_width, box_height);
        printf(" Time Series Slices Depth  : Scanned across %ld frames\n", num_frames);
        printf(" Total Memory Footprint    : Loaded just %ld float entries into RAM\n", total_subset_pixels);
        printf("------------------------------------------------------------------------\n");

        /* 6. Display the core pixel matrix fluctuation map over time frames */
        long frame_pixels = box_width * box_height;

        for (int f = 0; f < num_frames; f++) {
            long frame_offset = f * frame_pixels;

            // Calculate coordinates for the absolute center of our 5x5 cutout window
            // In our 5x5 block, index (X=2, Y=2) corresponds to the true star center (X=28, Y=48)
            long center_idx = frame_offset + (2 * box_width) + 2;
            float center_core_val = subset_buffer[center_idx];

            printf(" Frame #%d -> Center Core Pixel Value (X=28, Y=48): %8.4f\n", f + 1, center_core_val);
        }
    }

    printf("========================================================================\n");

    /* 7. Clean up resource memory footprints and close handle */
    free(subset_buffer);
    fits_close_file(fptr, &status);
    return status;
}
