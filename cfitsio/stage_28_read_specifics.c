#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;

    /* 1. Open the master repository file in READONLY mode */
    if (fits_open_file(&fptr, "stage_25_master_observatory_dataset.fits", READONLY, &status)) {
        printf("Error: Could not open master archive dataset file.\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Target the STARRY_SKY_CUBE extension layer (HDU 2) */
    if (fits_movnam_hdu(fptr, IMAGE_HDU, "STARRY_SKY_CUBE", 0, &status)) {
        printf("Error: Could not find 'STARRY_SKY_CUBE' image extension!\n");
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 3. EXTRACT SPECIFIC METADATA HEADERS BY NAME */
    double exptime = 0.0;
    char filter[FLEN_VALUE] = {0};
    char telescope[FLEN_VALUE] = {0};

    // fits_read_key inputs: fptr, datatype, keyname, output_buffer, comment_buf, status
    fits_read_key(fptr, TDOUBLE, "EXPTIME",  &exptime,   NULL, &status);
    fits_read_key(fptr, TSTRING, "FILTER",   filter,     NULL, &status);
    fits_read_key(fptr, TSTRING, "TELESCOP", telescope,  NULL, &status);

    if (status) {
        printf("[WARNING]: Some named keywords were missing from the header.\n");
        status = 0; /* Reset flag status to success to keep the pipeline moving */
    }

    printf("========================================================================\n");
    printf("        TARGET EXTRACTION REPORT: METADATA & DATA SLICE                 \n");
    printf("========================================================================\n");
    printf(" Observatory Facility: %s\n", telescope);
    printf(" Optical Band Filter : %s\n", filter);
    printf(" Exposure Duration   : %.1f seconds\n", exptime);
    printf("------------------------------------------------------------------------\n");

    /* 4. READ A SPECIFIC 2D PIXEL FRAME SLICE FROM THE 3D CUBE */
    int target_frame_slice = 3; /* Let's explicitly target frame 3 of our 5 frames */
    long width = 256;
    long height = 256;
    long npixels_frame = width * height;

    float *slice_pixels = (float *)malloc(npixels_frame * sizeof(float));
    if (slice_pixels == NULL) {
        printf("Memory allocation failed!\n");
        fits_close_file(fptr, &status);
        return 1;
    }

    /* Set up coordinates for our 3D entry index vector: {X_Start, Y_Start, Frame_Start}
       FITS tracking uses 1-based indexing! */
    long firstelem_cube[] = {1, 1, target_frame_slice};
    float nulval = 0.0f;
    int anynul = 0;

    if (fits_read_pix(fptr, TFLOAT, firstelem_cube, npixels_frame, &nulval, slice_pixels, &anynul, &status)) {
        printf("Error extraction array metrics from frame slice %d.\n", target_frame_slice);
        fits_report_error(stderr, status);
    } else {
        /* 5. Process statistical calculations on the isolated 2D slice data */
        float slice_max = -1.0f;
        double slice_flux_sum = 0.0;

        for (long i = 0; i < npixels_frame; i++) {
            float val = slice_pixels[i];
            slice_flux_sum += val;
            if (val > slice_max) {
                slice_max = val;
            }
        }

        printf(" Successfully Read 2D Slice Data from Time Frame #%d\n", target_frame_slice);
        printf(" -> Slice Maximum Pixel Core Value : %.4f\n", slice_max);
        printf(" -> Slice Average Background Flux  : %.4f\n", (slice_flux_sum / npixels_frame));
    }

    printf("========================================================================\n");

    /* 6. Release resources and close down file handle safely */
    free(slice_pixels);
    fits_close_file(fptr, &status);
    return status;
}
