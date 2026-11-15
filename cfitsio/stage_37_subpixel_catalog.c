#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "fitsio.h"

#define MAX_STARS 100
#define SNR_MIN_THRESHOLD 5.0f

typedef struct {
    int id;
    float x;
    float y;
} DetectedStar;

int main() {
    fitsfile *f_in, *f_out;
    int status = 0;

    /* 1. Open the 3D data time cube file from the master dataset package */
    if (fits_open_file(&f_in, "stage_25_master_observatory_dataset.fits", READONLY, &status)) {
        printf("Error: Could not open master dataset archive file.\n");
        fits_report_error(stderr, status);
        return status;
    }
    fits_movnam_hdu(f_in, IMAGE_HDU, "STARRY_SKY_CUBE", 0, &status);

    int bitpix, naxis;
    long naxes[] = {0, 0, 0};
    fits_get_img_param(f_in, 3, &bitpix, &naxis, naxes, &status);

    long width = naxes[0];
    long height = naxes[1];
    long num_frames = naxes[2];
    long npixels_frame = width * height;

    /* 2. Run local peak maxima search on Frame 1 for baseline anchors */
    float *frame1_pixels = (float *)malloc(npixels_frame * sizeof(float));
    long firstelem_frame1[] = {1, 1, 1};
    float nulval = 0.0f;
    int anynul = 0;
    fits_read_pix(f_in, TFLOAT, firstelem_frame1, npixels_frame, &nulval, frame1_pixels, &anynul, &status);

    DetectedStar star_list[MAX_STARS];
    int star_count = 0;
    float star_threshold = 0.5f;

    for (long y = 2; y < height - 2; y++) {
        for (long x = 2; x < width - 2; x++) {
            long idx = (y * width) + x;
            float val = frame1_pixels[idx];

            if (val >= star_threshold) {
                if (val >= frame1_pixels[idx - 1] && val >= frame1_pixels[idx + 1] &&
                    val >= frame1_pixels[idx - width] && val >= frame1_pixels[idx + width] &&
                    val >= frame1_pixels[idx - width - 1] && val >= frame1_pixels[idx - width + 1] &&
                    val >= frame1_pixels[idx + width - 1] && val >= frame1_pixels[idx + width + 1]) {

                    if (star_count < MAX_STARS) {
                        star_list[star_count].id = star_count + 1;
                        star_list[star_count].x = (float)(x + 1);
                        star_list[star_count].y = (float)(y + 1);
                        star_count++;
                    }
                }
            }
        }
    }
    free(frame1_pixels);

    /* 3. Create a BRAND NEW FITS file to hold the sub-pixel precision catalog */
    if (fits_create_file(&f_out, "!stage_37_subpixel_precision_catalog.fits", &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        return status;
    }

    /* Create the mandatory empty primary array baseline layer */
    int out_bitpix = 8;
    int out_naxis = 0;
    long out_naxes[] = {0};
    fits_create_img(f_out, out_bitpix, out_naxis, out_naxes, &status);

    /* 4. Design the New Binary Table Extension Schema layout
       Swapped old flat integers for high-precision X_CENTROID and Y_CENTROID floats */
    int tfields = 6;
    long nrows = 0;
    char *ttype[] = {"STAR_ID", "FRAME", "X_CENTROID", "Y_CENTROID", "NET_FLUX", "SNR"};
    char *tform[] = {"1J",      "1J",    "1E",         "1E",         "1E",       "1E"};
    char *tunit[] = {"count",   "index", "pixel",      "pixel",      "jansky",   "ratio"};

    if (fits_create_tbl(f_out, BINARY_TBL, nrows, tfields, ttype, tform, tunit, "SUBPIXEL_CATALOG", &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        fits_close_file(f_out, &status);
        return status;
    }

    /* 5. Process Batch 3D Photometry combined with Centroid calculations */
    float *pixels = (float *)malloc(npixels_frame * sizeof(float));
    double r_aperture = 4.0;
    double r_sky_in = 7.0;
    double r_sky_out = 12.0;
    long current_table_row = 1;

    for (int f = 0; f < num_frames; f++) {
        long firstelem_cube[] = {1, 1, f + 1};
        fits_read_pix(f_in, TFLOAT, firstelem_cube, npixels_frame, &nulval, pixels, &anynul, &status);

        for (int s = 0; s < star_count; s++) {
            // Target peak pixel coordinates base pointers
            long x_peak = (long)star_list[s].x - 1;
            long y_peak = (long)star_list[s].y - 1;

            /* --- SUB-PIXEL 3x3 INTENSITY CENTROID CALCULATION --- */
            double sum_w = 0.0;
            double sum_wx = 0.0;
            double sum_wy = 0.0;

            for (long j = -1; j <= 1; j++) {
                for (long i = -1; i <= 1; i++) {
                    long n_idx = ((y_peak + j) * width) + (x_peak + i);
                    double intensity = (double)pixels[n_idx];

                    sum_w  += intensity;
                    sum_wx += intensity * (double)(x_peak + i + 1); // 1-based index conversion
                    sum_wy += intensity * (double)(y_peak + j + 1);
                }
            }
            float centroid_x = (sum_w > 0.0) ? (float)(sum_wx / sum_w) : star_list[s].x;
            float centroid_y = (sum_w > 0.0) ? (float)(sum_wy / sum_w) : star_list[s].y;
            /* --------------------------------------------------- */

            /* Perform standard aperture ring flux tracking calculus */
            double c_x = (double)centroid_x - 1.0;
            double c_y = (double)centroid_y - 1.0;
            double raw_ap_flux = 0.0;
            int ap_pixel_count = 0;
            double total_sky_flux = 0.0;
            int sky_pixel_count = 0;

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

            double sky_mean = (sky_pixel_count > 0) ? (total_sky_flux / sky_pixel_count) : 0.0;
            float net_flux = (float)(raw_ap_flux - ((double)ap_pixel_count * sky_mean));

            /* Measure sky matrix standard deviation bounds variance */
            double sky_variance_sum = 0.0;
            for (long y = 0; y < height; y++) {
                for (long x = 0; x < width; x++) {
                    double dx = (double)x - c_x;
                    double dy = (double)y - c_y;
                    double dist = sqrt((dx * dx) + (dy * dy));
                    if (dist >= r_sky_in && dist <= r_sky_out) {
                        float val = pixels[(y * width) + x];
                        sky_variance_sum += (val - sky_mean) * (val - sky_mean);
                    }
                }
            }
            double sky_sigma = (sky_pixel_count > 1) ? sqrt(sky_variance_sum / (sky_pixel_count - 1)) : 0.01;
            double noise_denominator = sqrt(raw_ap_flux + ((double)ap_pixel_count * sky_sigma * sky_sigma));
            float snr_score = (noise_denominator > 0.0) ? (float)(net_flux / noise_denominator) : 0.0f;

            /* Filter out weak items under minimum SNR target constraints */
            if (snr_score < SNR_MIN_THRESHOLD) continue;

            int col_star_id = star_list[s].id;
            int col_frame = f + 1;

            /* 6. Append full fractional coordinate records into table structure layers */
            fits_write_col(f_out, TINT,   1, current_table_row, 1, 1, &col_star_id, &status);
            fits_write_col(f_out, TINT,   2, current_table_row, 1, 1, &col_frame,   &status);
            fits_write_col(f_out, TFLOAT, 3, current_table_row, 1, 1, &centroid_x,  &status); // Saved fractional float values
            fits_write_col(f_out, TFLOAT, 4, current_table_row, 1, 1, &centroid_y,  &status); // Saved fractional float values
            fits_write_col(f_out, TFLOAT, 5, current_table_row, 1, 1, &net_flux,    &status);
            fits_write_col(f_out, TFLOAT, 6, current_table_row, 1, 1, &snr_score,   &status);

            current_table_row++;
        }
    }

    printf("========================================================================\n");
    printf(" SUCCESS: High-Precision Sub-Pixel Binary Table Catalog Written!     \n");
    printf(" Saved File       : stage_37_subpixel_precision_catalog.fits                     \n");
    printf(" Entries Recorded : %ld high-quality rows stored with floats.          \n", current_table_row - 1);
    printf("========================================================================\n");

    /* 7. Memory release cleanup processing steps */
    free(pixels);
    fits_close_file(f_in, &status);
    fits_close_file(f_out, &status);
    return status;
}
