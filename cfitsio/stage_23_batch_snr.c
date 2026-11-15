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

    /* 1. Open the 3D data time cube file */
    if (fits_open_file(&f_in, "stage_17_time_cube.fits", READONLY, &status)) { /* FIXED: Changed &fptr to &f_in */
        printf("Error: Could not open 'stage_17_time_cube.fits'.\n");
        fits_report_error(stderr, status);
        return status;
    }

    if (fits_movnam_hdu(f_in, IMAGE_HDU, "STARRY_SKY_CUBE", 0, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        return status;
    }

    int bitpix, naxis;
    long naxes[] = {0, 0, 0}; /* FIXED: Array brackets added */
    if (fits_get_img_param(f_in, 3, &bitpix, &naxis, naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        return status;
    }

    long width = naxes[0];
    long height = naxes[1];
    long num_frames = naxes[2];
    long npixels_frame = width * height;

    /* 2. Run peak-finder on Frame 1 to extract coordinate anchors */
    float *frame1_pixels = (float *)malloc(npixels_frame * sizeof(float));
    long firstelem_frame1[] = {1, 1, 1};
    float nulval = 0.0f;
    int anynul = 0;
    fits_read_pix(f_in, TFLOAT, firstelem_frame1, npixels_frame, &nulval, frame1_pixels, &anynul, &status);

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
                        star_list[star_count].x = (float)(x + 1);
                        star_list[star_count].y = (float)(y + 1);
                        star_count++;
                    }
                }
            }
        }
    }
    free(frame1_pixels);

    /* 3. Create the NEW FITS output container with quality tracking */
    if (fits_create_file(&f_out, "!stage_23_all_stars_snr_catalog.fits", &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        return status;
    }

    int out_bitpix = 8;
    int out_naxis = 0;
    long out_naxes[] = {0};
    fits_create_img(f_out, out_bitpix, out_naxis, out_naxes, &status);

    int tfields = 6;
    long nrows = 0;
    char *ttype[] = {"STAR_ID", "FRAME", "X_CENTER", "Y_CENTER", "NET_FLUX", "SNR"};
    char *tform[] = {"1J",      "1J",    "1E",       "1E",       "1E",       "1E"};
    char *tunit[] = {"count",   "index", "pixel",    "pixel",    "jansky",   "ratio"};

    if (fits_create_tbl(f_out, BINARY_TBL, nrows, tfields, ttype, tform, tunit, "SNR_CATALOG", &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        fits_close_file(f_out, &status);
        return status;
    }

    /* 4. Process Batch Aperture Photometry with Real-Time SNR Filtering */
    float *pixels = (float *)malloc(npixels_frame * sizeof(float));
    double r_aperture = 4.0;
    double r_sky_in = 7.0;
    double r_sky_out = 12.0;
    long current_table_row = 1;
    long total_dropped_rows = 0;

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

            /* First pass: calculate sums for fluxes and backgrounds */
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

            /* Second pass: compute Standard Deviation / Variance (RMS noise) of sky ring */
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

            /* Compute the CCD Equation SNR score */
            double noise_denominator = sqrt(raw_ap_flux + ((double)ap_pixel_count * sky_sigma * sky_sigma));
            float snr_score = (noise_denominator > 0.0) ? (float)(net_flux / noise_denominator) : 0.0f;

            /* Quality Check Filter Block */
            if (snr_score < SNR_MIN_THRESHOLD) {
                total_dropped_rows++;
                continue;
            }

            int col_star_id = star_list[s].id;
            int col_frame = f + 1;
            float col_x = star_list[s].x;
            float col_y = star_list[s].y;

            /* 5. Stream verified data rows directly into the file */
            fits_write_col(f_out, TINT,   1, current_table_row, 1, 1, &col_star_id, &status);
            fits_write_col(f_out, TINT,   2, current_table_row, 1, 1, &col_frame,   &status);
            fits_write_col(f_out, TFLOAT, 3, current_table_row, 1, 1, &col_x,       &status);
            fits_write_col(f_out, TFLOAT, 4, current_table_row, 1, 1, &col_y,       &status);
            fits_write_col(f_out, TFLOAT, 5, current_table_row, 1, 1, &net_flux,   &status);
            fits_write_col(f_out, TFLOAT, 6, current_table_row, 1, 1, &snr_score,  &status);

            current_table_row++;
        }
    }

    free(pixels);
    fits_close_file(f_in, &status);
    fits_close_file(f_out, &status);

    if (status) {
        fits_report_error(stderr, status);
    } else {
        printf("--------------------------------------------------------\n");
        printf(" SUCCESS: Compiled optimized SNR analysis catalog!      \n");
        printf(" Saved File       : stage_23_all_stars_snr_catalog.fits          \n");
        printf(" Quality Target   : SNR >= %0.1f required               \n", SNR_MIN_THRESHOLD);
        printf(" Confirmed Records: %ld entries logged dynamically.    \n", current_table_row - 1);
        printf(" Filtered/Dropped : %ld low-signal entries discarded.   \n", total_dropped_rows);
        printf("========================================================\n");
    }
    return status;
}
