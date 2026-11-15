#define _DEFAULT_SOURCE  /* Unlocks modern system headers on Linux */
#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main() {
    fitsfile *f_sci, *f_bg, *f_reduced;
    int status = 0;

    printf("========================================================================\n");
    printf("        STAGE 34: HIGH-PERFORMANCE DATA REDUCTION PIPELINE              \n");
    printf("========================================================================\n");
    printf(" Initializing concurrent pixel-by-pixel background subtraction arrays...\n");

    /* 1. Open the raw master science cube file in READONLY mode */
    if (fits_open_file(&f_sci, "stage_25_master_observatory_dataset.fits", READONLY, &status)) {
        printf("Error: Could not open master science dataset file.\n");
        fits_report_error(stderr, status);
        return status;
    }
    fits_movnam_hdu(f_sci, IMAGE_HDU, "STARRY_SKY_CUBE", 0, &status);

    /* 2. Open your estimated 2D background maps cube file in READONLY mode */
    if (fits_open_file(&f_bg, "stage_33_master_background_maps.fits", READONLY, &status)) {
        printf("Error: Could not open estimated background maps file.\n");
        fits_report_error(stderr, status);
        fits_close_file(f_sci, &status);
        return status;
    }
    fits_movnam_hdu(f_bg, IMAGE_HDU, "BACKGROUND_MESH", 0, &status);

    /* 3. Read image cube boundaries parameter metrics (Expecting identical 3D shapes) */
    int bitpix, naxis;
    long naxes[] = {0, 0, 0};
    fits_get_img_param(f_sci, 3, &bitpix, &naxis, naxes, &status);

    long width = naxes[0];
    long height = naxes[1];
    long num_frames = naxes[2];
    long npixels_frame = width * height;
    long total_pixels = npixels_frame * num_frames;

    /* 4. Create a BRAND NEW target file container to save the flat data science outputs */
    if (fits_create_file(&f_reduced, "!stage_34_master_reduced_science.fits", &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_sci, &status);
        fits_close_file(f_bg, &status);
        return status;
    }

    /* Create the mandatory empty primary array baseline layer */
    int out_bitpix = 8;
    int out_naxis = 0;
    long out_naxes[] = {0};
    fits_create_img(f_reduced, out_bitpix, out_naxis, out_naxes, &status);

    /* Append the 3D Image Cube Extension for the flattened reduced array outputs */
    long red_naxes[] = {width, height, num_frames};
    fits_create_img(f_reduced, FLOAT_IMG, 3, red_naxes, &status);
    fits_write_key(f_reduced, TSTRING, "EXTNAME", "REDUCED_SCIENCE", "Flattened Background-Subtracted Cube", &status);

    /* 5. Allocate runtime processing buffer memory blocks */
    float *sci_pixels = (float *)malloc(npixels_frame * sizeof(float));
    float *bg_pixels  = (float *)malloc(npixels_frame * sizeof(float));
    float *red_pixels = (float *)malloc(npixels_frame * sizeof(float));

    if (sci_pixels == NULL || bg_pixels == NULL || red_pixels == NULL) {
        printf("Memory allocation failed!\n");
        free(sci_pixels); free(bg_pixels); free(red_pixels);
        fits_close_file(f_sci, &status); fits_close_file(f_bg, &status); fits_close_file(f_reduced, &status);
        return 1;
    }

    float nulval = 0.0f;
    int anynul = 0;

    /* 6. Run sequential frame data matrix arithmetic reduction processing */
    for (int f = 0; f < num_frames; f++) {
        long firstelem_cube[] = {1, 1, f + 1};

        /* Read raw data and corresponding background map frame layer slice concurrently */
        fits_read_pix(f_sci, TFLOAT, firstelem_cube, npixels_frame, &nulval, sci_pixels, &anynul, &status);
        fits_read_pix(f_bg,  TFLOAT, firstelem_cube, npixels_frame, &nulval, bg_pixels,  &anynul, &status);

        if (status) {
            printf("Error reading slice data maps at frame %d\n", f + 1);
            break;
        }

        /* --- ARITHMETIC ELEMENT-BY-ELEMENT SUBTRACT SUBTRACTION LOOP --- */
        for (long idx = 0; idx < npixels_frame; idx++) {
            red_pixels[idx] = sci_pixels[idx] - bg_pixels[idx];
        }
        /* --------------------------------------------------------------- */

        /* Write the flattened reduced frame array slice immediately to storage */
        fits_write_pix(f_reduced, TFLOAT, firstelem_cube, npixels_frame, red_pixels, &status);
        printf(" -> Reduced Frame Layer %d / %ld: Subtracted continuous spatial noise grid.\n", f + 1, num_frames);
    }

    /* 7. Close file descriptors cleanly and release resources */
    free(sci_pixels); free(bg_pixels); free(red_pixels);
    fits_close_file(f_sci, &status);
    fits_close_file(f_bg, &status);
    fits_close_file(f_reduced, &status);

    if (status) {
        printf("--------------------------------------------------------\n");
        printf(" An error occurred during final data reduction pipeline execution.\n");
        fits_report_error(stderr, status);
    } else {
        printf("--------------------------------------------------------\n");
        printf(" SUCCESS: Science data array reduced and flattened perfectly!  \n");
        printf(" Saved Output File: stage_34_master_reduced_science.fits                \n");
        printf("====================================================================\n");
    }

    return status;
}
