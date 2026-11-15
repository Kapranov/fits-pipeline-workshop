#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main() {
    fitsfile *f_in, *f_out;
    int status = 0;

    /* 1. Open the source 3D image cube simulation file in READONLY mode */
    if (fits_open_file(&f_in, "stage_07_create_starry_sky_header.fits", READONLY, &status)) {
        printf("Error: Could not open source FITS file.\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Target the STARRY_SKY image cube extension */
    if (fits_movnam_hdu(f_in, IMAGE_HDU, "STARRY_SKY", 0, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        return status;
    }

    /* 3. Read image parameters */
    int bitpix, naxis;
    long naxes[3] = {0, 0, 0};
    if (fits_get_img_param(f_in, 3, &bitpix, &naxis, naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        return status;
    }

    long width = naxes[0];
    long height = naxes[1];
    long num_frames = (naxis == 3) ? naxes[2] : 1;
    long npixels_frame = width * height;

    /* 4. Create the NEW summary output FITS file container */
    if (fits_create_file(&f_out, "!stage_17_frame_statistics_summary.fits", &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        return status;
    }

    /* Create the mandatory empty primary array baseline layer */
    int out_bitpix = 8;
    int out_naxis = 0;
    long out_naxes[] = {0};
    fits_create_img(f_out, out_bitpix, out_naxis, out_naxes, &status);

    /* 5. Design the Binary Table Extension Schema layout for our stats data */
    int tfields = 4;
    long nrows = 0; // Will grow dynamically loop-by-loop
    char *ttype[] = {"FRAME", "BG_MEAN", "MAX_FLUX", "STAR_PIXELS"};
    char *tform[] = {"1J",    "1D",      "1E",       "1J"};
    char *tunit[] = {"index", "flux",    "jansky",   "count"};

    if (fits_create_tbl(f_out, BINARY_TBL, nrows, tfields, ttype, tform, tunit, "FRAME_STATS", &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        fits_close_file(f_out, &status);
        return status;
    }

    /* 6. Allocate runtime frame buffer array memory */
    float *pixels = (float *)malloc(npixels_frame * sizeof(float));
    if (pixels == NULL) {
        printf("Memory allocation failed!\n");
        fits_close_file(f_in, &status);
        fits_close_file(f_out, &status);
        return 1;
    }

    float star_threshold = 0.5f;
    float nulval = 0.0f;
    int anynul = 0;

    printf("========================================================\n");
    printf("   PROCESSING 3D CUBE & STREAMING TO NEW SUMMARY FILE  \n");
    printf("========================================================\n");

    /* 7. Read slices from f_in, calculate metrics, and write rows to f_out */
    for (int f = 0; f < num_frames; f++) {
        long firstelem_cube[3] = {1, 1, f + 1};

        if (fits_read_pix(f_in, TFLOAT, firstelem_cube, npixels_frame, &nulval, pixels, &anynul, &status)) {
            fits_report_error(stderr, status);
            break;
        }

        float max_brightness = -1.0f;
        double total_flux = 0.0;
        int high_intensity_sources = 0;

        for (long idx = 0; idx < npixels_frame; idx++) {
            float val = pixels[idx];
            total_flux += val;

            if (val > max_brightness) {
                max_brightness = val;
            }
            if (val >= star_threshold) {
                high_intensity_sources++;
            }
        }

        int col_frame = f + 1;
        double col_bg_mean = total_flux / npixels_frame;
        float col_max_flux = max_brightness;
        int col_star_pixels = high_intensity_sources;

        /* Write data fields row-by-row into the new table extension layout columns */
        fits_write_col(f_out, TINT,    1, col_frame, 1, 1, &col_frame,       &status);
        fits_write_col(f_out, TDOUBLE, 2, col_frame, 1, 1, &col_bg_mean,     &status);
        fits_write_col(f_out, TFLOAT,  3, col_frame, 1, 1, &col_max_flux,     &status);
        fits_write_col(f_out, TINT,    4, col_frame, 1, 1, &col_star_pixels,   &status);

        printf(" Frame %d processed -> Wrote to summary catalog entry row.\n", col_frame);
    }

    /* 8. Close file handles cleanly and flush to storage */
    free(pixels);
    fits_close_file(f_in, &status);
    fits_close_file(f_out, &status);

    if (status) {
        printf("An error occurred during final pipeline write tracking operations.\n");
        fits_report_error(stderr, status);
    } else {
        printf("========================================================\n");
        printf(" SUCCESS: File generated cleanly!                      \n");
        printf(" Output Path: stage_17_frame_statistics_summary.fits            \n");
        printf("========================================================\n");
    }

    return status;
}
