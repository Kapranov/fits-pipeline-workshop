#include <stdio.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;

    /* 1. Open the simulated sky file */
    if (fits_open_file(&fptr, "stage_07_create_starry_sky_header.fits", READONLY, &status)) {
        printf("Error: Could not open file. Check filename and path.\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Move to the STARRY_SKY image layer */
    if (fits_movnam_hdu(fptr, IMAGE_HDU, "STARRY_SKY", 0, &status)) {
        printf("Error: Could not find 'STARRY_SKY' extension layer!\n");
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 3. Read image dimensions to prevent out-of-bounds array reads */
    int bitpix, naxis;
    long naxes[2] = {0, 0};

    // fits_get_img_param reads bitpix, naxis, and naxes array configurations all at once
    if (fits_get_img_param(fptr, 2, &bitpix, &naxis, naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    printf("Successfully connected to image layer (%ld x %ld pixels).\n", naxes[0], naxes[1]);

    /* 4. Request target coordinate inputs from the user */
    long input_x, input_y;
    printf("Enter X coordinate (1 to %ld): ", naxes[0]);
    if (scanf("%ld", &input_x) != 1) return 1;

    printf("Enter Y coordinate (1 to %ld): ", naxes[1]);
    if (scanf("%ld", &input_y) != 1) return 1;

    /* Validate that inputs fit inside the grid boundary sizes */
    if (input_x < 1 || input_x > naxes[0] || input_y < 1 || input_y > naxes[1]) {
        printf("Error: Coordinates are out of bounds!\n");
        fits_close_file(fptr, &status);
        return 1;
    }

    /* 5. Convert 2D (X, Y) layout to a 1D FITS element index index
       FITS files use 1-based indexing. The formula to find a pixel offset is:
       firstelem = ((Y - 1) * Width) + X */
    long firstelem = ((input_y - 1) * naxes[0]) + input_x;

    float pixel_value = 0.0f;
    long nelements = 1;      // We only want to read 1 single pixel
    float nulval = 0.0f;     // Value to return if the pixel is undefined/blank
    int anynul = 0;          // Will be set to true if a null value was encountered

    /* 6. Extract the single target pixel value from disk memory */
    if (fits_read_pix(fptr, TFLOAT, &firstelem, nelements, &nulval, &pixel_value, &anynul, &status)) {
        fits_report_error(stderr, status);
    } else {
        printf("\n===========================================\n");
        printf(" Pixel Coordinates : X = %ld, Y = %ld\n", input_x, input_y);
        printf(" Brightness Value   : %.4f\n", pixel_value);

        // Contextual analysis of what the pixel value represents
        if (pixel_value > 0.5f) {
            printf(" Analysis          : Target hits a bright star core! 🌟\n");
        } else if (pixel_value > 0.06f) {
            printf(" Analysis          : Target hits the outer halo of a star.\n");
        } else {
            printf(" Analysis          : Target hits empty background sky glow.\n");
        }
        printf("===========================================\n");
    }

    /* 7. Close file stream */
    fits_close_file(fptr, &status);
    return status;
}
