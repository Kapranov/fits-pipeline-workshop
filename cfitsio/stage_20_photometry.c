#include <stdio.h>
#include <stdlib.h>
#include <math.h>
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

    /* 3. Read image parameter dimensions */
    int bitpix, naxis;
    long naxes[] = {0, 0, 0};
    fits_get_img_param(fptr, 3, &bitpix, &naxis, naxes, &status);

    long width = naxes[0];
    long height = naxes[1];
    long num_frames = naxes[2];
    long npixels_frame = width * height;

    /* 4. Request user input for target star parameters */
    double target_x, target_y;
    printf("==================================================\n");
    printf("       APERTURE PHOTOMETRY EXTRACTION ENGINE       \n");
    printf("==================================================\n");
    printf("Enter target star center X (1 to %ld): ", width);
    if (scanf("%lf", &target_x) != 1) return 1;
    printf("Enter target star center Y (1 to %ld): ", height);
    if (scanf("%lf", &target_y) != 1) return 1;

    // Standard aperture radii definitions in pixels
    double r_aperture = 4.0;  // Bounding inner circle radius
    double r_sky_in = 7.0;    // Inner background ring border radius
    double r_sky_out = 12.0;  // Outer background ring border radius

    /* Convert user 1-indexed coordinate input back to C 0-indexed offset values */
    double c_x = target_x - 1.0;
    double c_y = target_y - 1.0;

    /* Allocate memory for single 2D plane pixel memory cushion buffer */
    float *pixels = (float *)malloc(npixels_frame * sizeof(float));
    if (pixels == NULL) {
        printf("Memory allocation failed!\n");
        fits_close_file(fptr, &status);
        return 1;
    }

    float nulval = 0.0f;
    int anynul = 0;

    printf("\nPerforming photometry using: R_ap=%0.1f, R_in=%0.1f, R_out=%0.1f\n", r_aperture, r_sky_in, r_sky_out);
    printf("------------------------------------------------------------------------\n");
    printf("  Frame  |  Raw Ap Flux  |  Sky Bkg (Val/Pix)  |  Net Star Flux (Jansky) \n");
    printf("------------------------------------------------------------------------\n");

    /* 5. Process each frame sequentially over the time-series dimension cube */
    for (int f = 0; f < num_frames; f++) {
        long firstelem_cube[] = {1, 1, f + 1};

        if (fits_read_pix(fptr, TFLOAT, firstelem_cube, npixels_frame, &nulval, pixels, &anynul, &status)) {
            fits_report_error(stderr, status);
            break;
        }

        double raw_ap_flux = 0.0;
        int ap_pixel_count = 0;

        double total_sky_flux = 0.0;
        int sky_pixel_count = 0;

        /* 6. Scan the active frame grid to map geometric circular boundaries */
        for (long y = 0; y < height; y++) {
            for (long x = 0; x < width; x++) {
                // Compute accurate Euclidean distance from current pixel to target star center
                double dx = (double)x - c_x;
                double dy = (double)y - c_y;
                double dist = sqrt((dx * dx) + (dy * dy));

                long idx = (y * width) + x;
                float val = pixels[idx];

                // Check if pixel falls inside the inner star aperture
                if (dist <= r_aperture) {
                    raw_ap_flux += val;
                    ap_pixel_count++;
                }
                // Check if pixel falls inside the concentric outer sky ring
                else if (dist >= r_sky_in && dist <= r_sky_out) {
                    total_sky_flux += val;
                    sky_pixel_count++;
                }
            }
        }

        /* 7. Compute net light intensity metrics by subtracting background offsets */
        double sky_background_level = (sky_pixel_count > 0) ? (total_sky_flux / sky_pixel_count) : 0.0;
        double net_star_flux = raw_ap_flux - ((double)ap_pixel_count * sky_background_level);

        printf("   %02d    |   %10.4f  |      %10.6f     |       %10.4f\n",
               f + 1, raw_ap_flux, sky_background_level, net_star_flux);
    }

    printf("========================================================================\n");

    /* 8. Memory deallocation cleanup and exit */
    free(pixels);
    fits_close_file(fptr, &status);
    return status;
}
