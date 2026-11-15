#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main() {
    fitsfile *f_in, *f_out;
    int status = 0;

    /* 1. Open the master dataset file in READONLY mode */
    if (fits_open_file(&f_in, "stage_25_master_observatory_dataset.fits", READONLY, &status)) {
        printf("Error: Could not open master archive dataset file.\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* Target the STARRY_SKY_CUBE extension layer (HDU 2) */
    if (fits_movnam_hdu(f_in, IMAGE_HDU, "STARRY_SKY_CUBE", 0, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        return status;
    }

    /* 2. Allocate memory and extract Frame Slice #1 to blur */
    long width = 256;
    long height = 256;
    long npixels = width * height;

    float *raw_pixels = (float *)malloc(npixels * sizeof(float));
    float *blurred_pixels = (float *)malloc(npixels * sizeof(float));

    if (raw_pixels == NULL || blurred_pixels == NULL) {
        printf("Memory allocation failed!\n");
        fits_close_file(f_in, &status);
        return 1;
    }

    long firstelem_cube[] = {1, 1, 1}; /* Read Frame 1 */
    float nulval = 0.0f;
    int anynul = 0;
    fits_read_pix(f_in, TFLOAT, firstelem_cube, npixels, &nulval, raw_pixels, &anynul, &status);
    fits_close_file(f_in, &status); // Done reading from master

    printf("====================================================================\n");
    printf("     STAGE 31: 2D CELESTIAL GAUSSIAN CONVOLUTION SMOOTHING ENGINE    \n");
    printf("====================================================================\n");
    printf(" Running a 3x3 blurring kernel matrix across all 65,536 pixels...\n");

    /* 3. RUN THE MATRIX CONVOLUTION LOOP
       We skip the outermost border pixels to avoid memory out-of-bounds arrays */
    float kernel[3][3] = {
        {1.0f/16.0f, 2.0f/16.0f, 1.0f/16.0f},
        {2.0f/16.0f, 4.0f/16.0f, 2.0f/16.0f},
        {1.0f/16.0f, 2.0f/16.0f, 1.0f/16.0f}
    };

    // Copy edge pixels untouched to maintain boundary safety profiles
    for (long i = 0; i < npixels; i++) blurred_pixels[i] = raw_pixels[i];

    for (long y = 1; y < height - 1; y++) {
        for (long x = 1; x < width - 1; x++) {
            float pixel_sum = 0.0f;

            // Multiply kernel weights against the active 3x3 pixel neighborhood
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    long neighbor_idx = ((y + ky) * width) + (x + kx);
                    pixel_sum += raw_pixels[neighbor_idx] * kernel[ky + 1][kx + 1];
                }
            }

            long center_idx = (y * width) + x;
            blurred_pixels[center_idx] = pixel_sum;
        }
    }

    /* 4. CREATE A STANDALONE 2D FITS FILE FOR THE BLURRED OUTPUT IMAGE */
    if (fits_create_file(&f_out, "!stage_31_blurred_sky_slice.fits", &status)) {
        fits_report_error(stderr, status);
        free(raw_pixels);
        free(blurred_pixels);
        return status;
    }

    int out_bitpix = FLOAT_IMG;
    int out_naxis = 2;
    long out_naxes[] = {width, height};

    fits_create_img(f_out, out_bitpix, out_naxis, out_naxes, &status);
    fits_write_key(f_out, TSTRING, "EXTNAME", "BLURRED_IMAGE", "Gaussian Blur Processed Slice", &status);
    fits_write_comment(f_out, "Image convolution filter layer applied to simulate mirror PSF smearing.", &status);

    /* 5. Write the smoothed pixel array matrix into the fresh destination file */
    long out_fpixels[] = {1, 1};
    if (fits_write_pix(f_out, TFLOAT, out_fpixels, npixels, blurred_pixels, &status)) {
        printf("Error writing convolved matrix to destination file.\n");
        fits_report_error(stderr, status);
    } else {
        printf("--------------------------------------------------------------------\n");
        printf(" SUCCESS: Standard image convolution processing complete!            \n");
        printf(" Saved File : stage_31_blurred_sky_slice.fits                                \n");
        printf(" Image Shape: 256 x 256 pixels standalone 2D canvas                 \n");
        printf("====================================================================\n");
    }

    /* 6. Release heap footprints and close handles safely */
    free(raw_pixels);
    free(blurred_pixels);
    fits_close_file(f_out, &status);
    return status;
}
