#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "fitsio.h"

#define MAX_STARS 100

/* Structure to hold dynamically found star center positions */
typedef struct {
    int id;
    float x;
    float y;
} DetectedStar;

int main() {
    fitsfile *f_in, *f_out;
    int status = 0;

    /* 1. Open the 3D data time cube file in READONLY mode */
    if (fits_open_file(&f_in, "stage_17_time_cube.fits", READONLY, &status)) {
        printf("Error: Could not open 'stage_07_time_cube.fits'.\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Move to the STARRY_SKY_CUBE extension layer to parse pixels */
    if (fits_movnam_hdu(f_in, IMAGE_HDU, "STARRY_SKY_CUBE", 0, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        return status;
    }

    /* 3. Read image parameters configurations */
    int bitpix, naxis;
    long naxes[] = {0, 0, 0};
    fits_get_img_param(f_in, 3, &bitpix, &naxis, naxes, &status);

    long width = naxes[0];
    long height = naxes[1];
    long num_frames = naxes[2];
    long npixels_frame = width * height;

    /* 4. Allocate memory buffer and read the baseline Frame 1 to find the 50 star centers */
    float *frame1_pixels = (float *)malloc(npixels_frame * sizeof(float));
    long firstelem_frame1[] = {1, 1, 1};
    float nulval = 0.0f;
    int anynul = 0;

    fits_read_pix(f_in, TFLOAT, firstelem_frame1, npixels_frame, &nulval, frame1_pixels, &anynul, &status);

    /* Run local maxima detection loop on Frame 1 to locate all star center anchors */
    DetectedStar star_list[MAX_STARS];
    int star_count = 0;
    float star_threshold = 0.5f;

    for (long y = 1; y < height - 1; y++) {
        for (long x = 1; x < width - 1; x++) {
            long idx = (y * width) + x;
            float val = frame1_pixels[idx];

            if (val >= star_threshold) {
                if (val >= frame1_pixels[idx - 1] && val >= frame1_pixels[idx + 1] &&
                    val >= frame1_pixels[idx - width] && val >= frame1_pixels[idx + width] &&
                    val >= frame1_pixels[idx - width - 1] && val >= frame1_pixels[idx - width + 1] &&
                    val >= frame1_pixels[idx + width - 1] && val >= frame1_pixels[idx + width + 1]) {

                    if (star_count < MAX_STARS) {
                        star_list[star_count].id = star_count + 1;
                        star_list[star_count].x = (float)(x + 1); // 1-based indexing map
                        star_list[star_count].y = (float)(y + 1);
                        star_count++;
                    }
                }
            }
        }
    }
    free(frame1_pixels); // Clean up baseline buffer row footprint

    printf("========================================================\n");
    printf("   BATCH APERTURE PHOTOMETRY BATCH ENGINE INITIALIZED   \n");
    printf("========================================================\n");
    printf(" Located %d baseline stars to track across %ld frames.\n", star_count, num_frames);
    printf(" Generating new light curves table dataset...\n");
    printf("--------------------------------------------------------\n");

    /* 5. Create a BRAND NEW output summary file container for the light curves */
    if (fits_create_file(&f_out, "!stage_20_all_stars_light_curves.fits", &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        return status;
    }

    /* Mandatory primary empty baseline layout creation */
    int out_bitpix = 8;
    int out_naxis = 0;
    long out_naxes[] = {0};
    fits_create_img(f_out, out_bitpix, out_naxis, out_naxes, &status);

    /* 6. Design the Binary Table Extension Schema layout for our tracking profiles */
    int tfields = 5;
    long nrows = 0;
    char *ttype[] = {"STAR_ID", "FRAME", "X_CENTER", "Y_CENTER", "NET_FLUX"};
    char *tform[] = {"1J",      "1J",    "1E",       "1E",       "1E"};
    char *tunit[] = {"count",   "index", "pixel",    "pixel",    "jansky"};

    if (fits_create_tbl(f_out, BINARY_TBL, nrows, tfields, ttype, tform, tunit, "LIGHT_CURVES", &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        fits_close_file(f_out, &status);
        return status;
    }

    /* 7. Allocate frame slice buffer array memory size cushions */
    float *pixels = (float *)malloc(npixels_frame * sizeof(float));
    double r_aperture = 4.0;
    double r_sky_in = 7.0;
    double r_sky_out = 12.0;
    long current_table_row = 1;

    /* 8. Outer loop processes frames, Inner loop processes each star */
    for (int f = 0; f < num_frames; f++) {
        long firstelem_cube[] = {1, 1, f + 1};
        fits_read_pix(f_in, TFLOAT, firstelem_cube, npixels_frame, &nulval, pixels, &anynul, &status);

        for (int s = 0; s < star_count; s++) {
            double c_x = (double)star_list[s].x - 1.0;
            double c_y = (double)star_list[s].y - 1.0;

            double raw_ap_flux = 0.0;
            int ap_pixel_count = 0;
            double total_sky_flux = 0.0;
            int sky_pixel_count = 0;

            /* Compute Euclidean circle math distances over frame grid */
            for (long y = 0; y < height; y++) {
                for (long x = 0; x < width; x++) {
                    double dx = (double)x - c_x;
                    double dy = (double)y - c_y;
                    double dist = sqrt((dx * dx) + (dy * dy));

                    float val = pixels[(y * width) + x];

                    if (dist <= r_aperture) {
                        raw_ap_flux += val;
                        ap_pixel_count++;
                    } else if (dist >= r_sky_in && dist <= r_sky_out) {
                        total_sky_flux += val;
                        sky_pixel_count++;
                    }
                }
            }

            double sky_level = (sky_pixel_count > 0) ? (total_sky_flux / sky_pixel_count) : 0.0;
            float net_flux = (float)(raw_ap_flux - ((double)ap_pixel_count * sky_level));

            int col_star_id = star_list[s].id;
            int col_frame = f + 1;
            float col_x = star_list[s].x;
            float col_y = star_list[s].y;

            /* Append records column fields row-by-row into table mapping matrices */
            fits_write_col(f_out, TINT,   1, current_table_row, 1, 1, &col_star_id, &status);
            fits_write_col(f_out, TINT,   2, current_table_row, 1, 1, &col_frame,   &status);
            fits_write_col(f_out, TFLOAT, 3, current_table_row, 1, 1, &col_x,       &status);
            fits_write_col(f_out, TFLOAT, 4, current_table_row, 1, 1, &col_y,       &status);
            fits_write_col(f_out, TFLOAT, 5, current_table_row, 1, 1, &net_flux,   &status);

            current_table_row++;
        }
        printf(" Batch Photometry Processing Completed for Frame Layer %d / %ld\n", f + 1, num_frames);
    }

    /* 9. Free memory allocations and cleanly finalize file updates */
    free(pixels);
    fits_close_file(f_in, &status);
    fits_close_file(f_out, &status);

    if (status) {
        printf("An error occurred during final table writing.\n");
        fits_report_error(stderr, status);
    } else {
        printf("--------------------------------------------------------\n");
        printf(" SUCCESS: Batch light curves catalog compiled completely!\n");
        printf(" Generated File: stage_20_all_stars_light_curves.fits            \n");
        printf(" Total Database Records Written: %ld entries.\n", current_table_row - 1);
        printf("========================================================\n");
    }

    return status;
}
