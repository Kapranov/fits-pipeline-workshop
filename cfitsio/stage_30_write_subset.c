#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main() {
    fitsfile *f_in, *f_out;
    int status = 0;

    /* 1. Open the Master Dataset file in READONLY mode */
    if (fits_open_file(&f_in, "stage_25_master_observatory_dataset.fits", READONLY, &status)) {
        printf("Error: Could not open master archive dataset file.\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Target the STARRY_SKY_CUBE image extension layer (HDU 2) */
    if (fits_movnam_hdu(f_in, IMAGE_HDU, "STARRY_SKY_CUBE", 0, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        return status;
    }

    /* 3. DEFINE THE 3D SUBSET EXTRACTION BOUNDS (5x5 pixels across 5 time frames)
       X: 26 to 30  |  Y: 46 to 50  |  Frame: 1 to 5 */
    long fpixels[] = {26, 46, 1};
    long lpixels[] = {30, 50, 5};
    long inc[]     = {1, 1, 1};

    long box_width  = lpixels[0] - fpixels[0] + 1;
    long box_height = lpixels[1] - fpixels[1] + 1;
    long num_frames = lpixels[2] - fpixels[2] + 1;
    long total_subset_pixels = box_width * box_height * num_frames;

    /* Allocate small buffer array for our 5x5x5 cutout */
    float *subset_buffer = (float *)malloc(total_subset_pixels * sizeof(float));
    if (subset_buffer == NULL) {
        printf("Memory allocation failed!\n");
        fits_close_file(f_in, &status);
        return 1;
    }

    float nulval = 0.0f;
    int anynul = 0;

    /* Extract the 3D data block cutout from disk storage */
    if (fits_read_subset(f_in, TFLOAT, fpixels, lpixels, inc, &nulval, subset_buffer, &anynul, &status)) {
        printf("Error extracting the localized data subset from master.\n");
        fits_report_error(stderr, status);
        free(subset_buffer);
        fits_close_file(f_in, &status);
        return status;
    }

    /* 4. CREATE THE BRAND NEW MINI STANDALONE FITS CUBE CONTAINER
       The '!' token forces an overwrite of old files if they exist */
    if (fits_create_file(&f_out, "!star_cutout_cube.fits", &status)) {
        printf("Error: Could not initialize the new sub-cube FITS file.\n");
        fits_report_error(stderr, status);
        free(subset_buffer);
        fits_close_file(f_in, &status);
        return status;
    }

    /* 5. DEFINE THE MINI 3D OUTPUT SHAPE ARRAY
       Unlike the master cube (256x256x5), this file's absolute primary dimensions are 5x5x5! */
    int out_bitpix = FLOAT_IMG;
    int out_naxis = 3;
    long out_naxes[] = {box_width, box_height, num_frames};

    /* Build the fresh 3D Primary HDU structure matrix natively inside our new container file */
    if (fits_create_img(f_out, out_bitpix, out_naxis, out_naxes, &status)) {
        fits_report_error(stderr, status);
        free(subset_buffer);
        fits_close_file(f_in, &status);
        fits_close_file(f_out, &status);
        return status;
    }

    /* Write descriptive tracking keywords directly into the new primary header block */
    fits_write_key(f_out, TSTRING, "EXTNAME", "STAR_CUTOUT", "Target Region of Interest Cutout", &status);
    fits_write_key(f_out, TINT,    "ORIG_X",  &fpixels[0],   "Original lower-left X index pixel boundary", &status);
    fits_write_key(f_out, TINT,    "ORIG_Y",  &fpixels[1],   "Original lower-left Y index pixel boundary", &status);

    /* 6. WRITE THE SUBSET DATA STREAM INTO THE NEW PRIMARY IMAGE SLOTS
       The coordinate arrays specify where inside the new file to land the data array.
       Since this file is exactly 5x5x5, we populate from start coordinates {1,1,1} to end coordinates {5,5,5}. */
    long out_fpixels[] = {1, 1, 1};
    long out_lpixels[] = {box_width, box_height, num_frames};

    if (fits_write_subset(f_out, TFLOAT, out_fpixels, out_lpixels, subset_buffer, &status)) {
        printf("Error writing subset matrix to the new destination file.\n");
        fits_report_error(stderr, status);
    } else {
        printf("====================================================================\n");
        printf(" SUCCESS: Localized 3D Region of Interest Cloned Successfully!       \n");
        printf(" Saved File : star_cutout_cube.fits                                  \n");
        printf(" Dimensions : %ld width x %ld height x %ld frames time series depth  \n", out_naxes[0], out_naxes[1], out_naxes[2]);
        printf(" Core Logs  : Embedded lower-left anchor tracking indices: (%ld, %ld)\n", fpixels[0], fpixels[1]);
        printf("====================================================================\n");
    }

    /* 7. Clean up resource memory footprints and close handles safely */
    free(subset_buffer);
    fits_close_file(f_in, &status);
    fits_close_file(f_out, &status);
    return status;
}
