#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "fitsio.h"

int main() {
    fitsfile *f_in, *f_out;
    int status = 0;

    printf("========================================================================\n");
    printf("        STAGE 38: 2D CELESTIAL PIXEL CONVOLUTION BLUR ENGINE            \n");
    printf("========================================================================\n");
    printf(" Simulating telescope defocusing using a 5x5 Gaussian Kernel...\n");

    /* 1. Open the master repository file in READONLY mode */
    if (fits_open_file(&f_in, "stage_25_master_observatory_dataset.fits", READONLY, &status)) {
        printf("Error: Could not open master observatory dataset file.\n");
        fits_report_error(stderr, status);
        return status;
    }
    fits_movnam_hdu(f_in, IMAGE_HDU, "STARRY_SKY_CUBE", 0, &status);

    int bitpix, naxis;
    long naxes[] = {0, 0, 0};
    fits_get_img_param(f_in, 3, &bitpix, &naxis, naxes, &status);

    /* FIXED: Added array subscript brackets to grab dimensional lengths safely */
    long width      = naxes[0];
    long height     = naxes[1];
    long num_frames = (naxis == 3) ? naxes[2] : 1;
    long npixels_frame = width * height;

    /* 2. Create a BRAND NEW file specifically to store the Blurred Data Cube */
    if (fits_create_file(&f_out, "!stage_38_telescope_blurred_cube.fits", &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        return status;
    }

    /* Create the mandatory empty primary array baseline layer */
    int out_bitpix = 8;
    int out_naxis = 0;
    long out_naxes[] = {0};
    fits_create_img(f_out, out_bitpix, out_naxis, out_naxes, &status);

    /* Append the 3D Image Cube Extension for the blurred output structure */
    long blur_naxes[] = {width, height, num_frames};
    fits_create_img(f_out, FLOAT_IMG, 3, blur_naxes, &status);
    fits_write_key(f_out, TSTRING, "EXTNAME", "BLURRED_SKY", "Defocused Telescope Simulation", &status);

    /* 3. DYNAMICALLY COMPUTE A NORMALIZED 5x5 GAUSSIAN CONVOLUTION KERNEL */
    float kernel[5][5];
    float sigma_blur = 1.4f; // Controls the intensity width of the focus blur profile
    float kernel_sum = 0.0f;

    for (int j = -2; j <= 2; j++) {
        for (int i = -2; i <= 2; i++) {
            float r_sq = (float)(i * i + j * j);
            kernel[j + 2][i + 2] = expf(-r_sq / (2.0f * sigma_blur * sigma_blur));
            kernel_sum += kernel[j + 2][i + 2];
        }
    }
    // Normalize the matrix array elements so total light flux energy remains conserved
    for (int j = 0; j < 5; j++) {
        for (int i = 0; i < 5; i++) {
            kernel[j][i] /= kernel_sum;
        }
    }

    /* 4. Allocate temporary single-frame runtime data arrays buffers */
    float *raw_pixels  = (float *)malloc(npixels_frame * sizeof(float));
    float *blur_pixels = (float *)malloc(npixels_frame * sizeof(float));

    float nulval = 0.0f;
    int anynul = 0;

    /* 5. Process each frame sequentially over the time-series dimension cube */
    for (int f = 0; f < num_frames; f++) {
        long firstelem_cube[] = {1, 1, f + 1};
        fits_read_pix(f_in, TFLOAT, firstelem_cube, npixels_frame, &nulval, raw_pixels, &anynul, &status);

        /* Sweep the convolution kernel array across the spatial 2D matrix pixels */
        for (long y = 0; y < height; y++) {
            for (long x = 0; x < width; x++) {
                float convolution_sum = 0.0f;

                // Evaluate the localized 5x5 neighborhood window block
                for (int ky = -2; ky <= 2; ky++) {
                    long py = y + ky;
                    for (int kx = -2; kx <= 2; kx++) {
                        long px = x + kx;

                        /* FIXED: Replicate-Edge Border Strategy inside cleaned local assignments loop */
                        if (px < 0) px = 0;
                        if (py < 0) py = 0;
                        if (px >= width)  px = width - 1;
                        if (py >= height) py = height - 1;

                        long source_idx = (py * width) + px;
                        convolution_sum += raw_pixels[source_idx] * kernel[ky + 2][kx + 2];
                    }
                }
                blur_pixels[(y * width) + x] = convolution_sum;
            }
        }

        /* 6. Write the blurred telescope frame slice immediately onto storage sectors */
        fits_write_pix(f_out, TFLOAT, firstelem_cube, npixels_frame, blur_pixels, &status);
        printf(" -> Convolved Frame Layer %d / %ld: Applied 5x5 focus blur profile mapping.\n", f + 1, num_frames);
    }

    printf("------------------------------------------------------------------------\n");
    printf(" SUCCESS: 3D defocused telescope model generated successfully!          \n");
    printf(" Saved Output File: stage_38_telescope_blurred_cube.fits                         \n");
    printf("========================================================================\n");

    /* 7. Release resources and close down file handles cleanly */
    free(raw_pixels); free(blur_pixels);
    fits_close_file(f_in, &status);
    fits_close_file(f_out, &status);
    return status;
}
