#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;

    /* 1. Open the simulated 3D sky file */
    if (fits_open_file(&fptr, "stage_07_create_starry_sky_header.fits", READONLY, &status)) {
        printf("Error: Could not open file.\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Move to the STARRY_SKY image cube extension */
    if (fits_movnam_hdu(fptr, IMAGE_HDU, "STARRY_SKY", 0, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 3. Read image parameters (Now expecting 3 dimensions!) */
    int bitpix, naxis;
    long naxes[3] = {0, 0, 0}; /* Width, Height, Frames */
    if (fits_get_img_param(fptr, 3, &bitpix, &naxis, naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    long width = naxes[0];
    long height = naxes[1];
    long num_frames = (naxis == 3) ? naxes[2] : 1; /* Fallback to 1 if it's a 2D file */
    long npixels_frame = width * height;

    printf("==================================================\n");
    printf("      AUTOMATED 3D IMAGE CUBE SCANNER INITIALIZED \n");
    printf("==================================================\n");
    printf(" Image Dimensions : %ld x %ld pixels\n", width, height);
    printf(" Total Time Slices: %ld frames\n", num_frames);
    printf("==================================================\n\n");

    /* 4. Allocate memory buffer to load ONE frame layer at a time */
    float *pixels = (float *)malloc(npixels_frame * sizeof(float));
    if (pixels == NULL) {
        printf("Memory allocation failed!\n");
        fits_close_file(fptr, &status);
        return 1;
    }

    float star_threshold = 0.5f;
    float nulval = 0.0f;
    int anynul = 0;

    /* 5. Sequentially loop through and analyze each frame layer */
    for (int f = 0; f < num_frames; f++) {
        // Specify starting pixel coordinate array for this specific 3D slice: {X, Y, Frame}
        // Remember FITS tracking uses 1-based indexing!
        long firstelem_cube[3] = {1, 1, f + 1}; 

        if (fits_read_pix(fptr, TFLOAT, firstelem_cube, npixels_frame, &nulval, pixels, &anynul, &status)) {
            printf("Error reading data at frame %d\n", f + 1);
            fits_report_error(stderr, status);
            break;
        }

        /* 6. Scan metrics inside the isolated active frame array */
        float max_brightness = -1.0f;
        long max_x = 0, max_y = 0;
        double total_flux = 0.0;
        int high_intensity_sources = 0;

        for (long y = 0; y < height; y++) {
            for (long x = 0; x < width; x++) {
                long idx = (y * width) + x;
                float val = pixels[idx];

                total_flux += val;

                if (val > max_brightness) {
                    max_brightness = val;
                    max_x = x + 1; // Map back to FITS 1-based layout coords
                    max_y = y + 1;
                }

                if (val >= star_threshold) {
                    high_intensity_sources++;
                }
            }
        }

        double average_background = total_flux / npixels_frame;

        /* 7. Display statistics summary block for this individual frame slice */
        printf("--------------------------------------------------\n");
        printf(" FRAME SLICE FRAME #%d ANALYSIS REPORT\n", f + 1);
        printf("--------------------------------------------------\n");
        printf(" Global Average Background : %.4f\n", average_background);
        printf(" Pixels Above Threshold    : %d\n", high_intensity_sources);
        printf(" Max Core Pixel Value      : %.4f\n", max_brightness);
        printf(" Brightest Source Coordinate: X = %ld, Y = %ld 🌟\n", max_x, max_y);
    }

    printf("==================================================\n");
    printf(" END OF MULTI-FRAME PROCESSING ENGINE REPORT\n");
    printf("==================================================\n");

    /* 8. Free allocations and shut down safely */
    free(pixels);
    fits_close_file(fptr, &status);
    return status;
}
