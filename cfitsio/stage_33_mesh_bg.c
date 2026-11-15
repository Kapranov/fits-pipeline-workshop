#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "fitsio.h"

/* Helper comparison function for qsort to extract median values */
int compare_floats(const void *a, const void *b) {
    float fa = *(const float *)a;
    float fb = *(const float *)b;
    return (fa > fb) - (fa < fb);
}

int main() {
    fitsfile *f_in, *f_out;
    int status = 0;

    /* 1. Open the master repository file in READONLY mode */
    if (fits_open_file(&f_in, "stage_25_master_observatory_dataset.fits", READONLY, &status)) {
        printf("Error: Could not open master observatory dataset file.\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Target the STARRY_SKY_CUBE extension layer */
    if (fits_movnam_hdu(f_in, IMAGE_HDU, "STARRY_SKY_CUBE", 0, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        return status;
    }

    int bitpix, naxis;
    long naxes[] = {0, 0, 0};
    fits_get_img_param(f_in, 3, &bitpix, &naxis, naxes, &status);

    long width = naxes[0];
    long height = naxes[1];
    long num_frames = naxes[2];
    long npixels_frame = width * height;

    /* 3. Create a BRAND NEW file specifically to store the 2D Background Maps */
    if (fits_create_file(&f_out, "!stage_33_master_background_maps.fits", &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f_in, &status);
        return status;
    }

    /* Create the mandatory empty primary array baseline layer */
    int out_bitpix = 8;
    int out_naxis = 0;
    long out_naxes[] = {0};
    fits_create_img(f_out, out_bitpix, out_naxis, out_naxes, &status);

    /* Append the 3D Image Cube Extension for the Background Maps output structure */
    int bg_bitpix = FLOAT_IMG;
    int bg_naxis = 3;
    long bg_naxes[] = {width, height, num_frames};
    fits_create_img(f_out, bg_bitpix, bg_naxis, bg_naxes, &status);
    fits_write_key(f_out, TSTRING, "EXTNAME", "BACKGROUND_MESH", "Estimated 2D Sky Background Map", &status);

    /* 4. Allocate dynamic memory arrays for frame processing */
    float *pixels = (float *)malloc(npixels_frame * sizeof(float));
    float *bg_map = (float *)malloc(npixels_frame * sizeof(float));

    // Mesh dimensions configuration setups
    int mesh_size = 32; // Each local tile block size represents a 32x32 pixel section
    int tiles_x = width / mesh_size;  // 256 / 32 = 8 anchors along X axis
    int tiles_y = height / mesh_size; // 256 / 32 = 8 anchors along Y axis
    int npixels_tile = mesh_size * mesh_size;
    float *tile_buffer = (float *)malloc(npixels_tile * sizeof(float));

    // Allocate 2D matrix structure to hold the core sparse anchor grid values
    float mesh_grid[8][8];

    printf("========================================================================\n");
    printf("        STAGE 33: 2D SPATIAL BACKGROUND MESH ESTIMATOR ENGINE           \n");
    printf("========================================================================\n");
    printf(" Image Geometry : %ld x %ld pixels | Grid Resolution: %d x %d tiles\n", width, height, tiles_x, tiles_y);

    float nulval = 0.0f;
    int anynul = 0;

    /* 5. Process each frame sequentially over the time-series dimension cube */
    for (int f = 0; f < num_frames; f++) {
        long firstelem_cube[] = {1, 1, f + 1};
        fits_read_pix(f_in, TFLOAT, firstelem_cube, npixels_frame, &nulval, pixels, &anynul, &status);

        /* Step A: Calculate local median for each tile grid block */
        for (int ty = 0; ty < tiles_y; ty++) {
            for (int tx = 0; tx < tiles_x; tx++) {
                int tile_pixel_count = 0;

                // Load all raw pixel elements for the localized tile window
                for (int y = 0; y < mesh_size; y++) {
                    for (int x = 0; x < mesh_size; x++) {
                        long px = (tx * mesh_size) + x;
                        long py = (ty * mesh_size) + y;
                        tile_buffer[tile_pixel_count++] = pixels[(py * width) + px];
                    }
                }

                // Sort the localized buffer to extract a robust median score
                qsort(tile_buffer, tile_pixel_count, sizeof(float), compare_floats);
                mesh_grid[ty][tx] = tile_buffer[tile_pixel_count / 2];
            }
        }

        /* Step B: Interpolate the sparse anchor points to recreate a smooth full-size 2D map */
        for (long y = 0; y < height; y++) {
            for (long x = 0; x < width; x++) {
                // Find nearest tile coordinates fractions
                float fx = (float)x / mesh_size - 0.5f;
                float fy = (float)y / mesh_size - 0.5f;

                int tx1 = (int)floorf(fx);
                int ty1 = (int)floorf(fy);
                int tx2 = tx1 + 1;
                int ty2 = ty1 + 1;

                // Protect bounds checking shifts near edges
                if (tx1 < 0) { tx1 = 0; tx2 = 0; }
                if (ty1 < 0) { ty1 = 0; ty2 = 0; }
                if (tx2 >= tiles_x) { tx2 = tiles_x - 1; tx1 = tx2; }
                if (ty2 >= tiles_y) { ty2 = tiles_y - 1; ty1 = ty2; }

                float wx = fx - floorf(fx);
                float wy = fy - floorf(fy);

                // Bilinear interpolation mapping math calculation
                float val_interp = (1.0f - wx) * (1.0f - wy) * mesh_grid[ty1][tx1] +
                                   wx * (1.0f - wy) * mesh_grid[ty1][tx2] +
                                   (1.0f - wx) * wy * mesh_grid[ty2][tx1] +
                                   wx * wy * mesh_grid[ty2][tx2];

                bg_map[(y * width) + x] = val_interp;
            }
        }

        /* 6. Write the completed background map frame to disk */
        long firstelem_out[] = {1, 1, f + 1};
        fits_write_pix(f_out, TFLOAT, firstelem_out, npixels_frame, bg_map, &status);
        printf(" -> Completed Bilinear Interpolation Background Mesh Map for Frame %d / %ld\n", f + 1, num_frames);
    }

    printf("------------------------------------------------------------------------\n");
    printf(" SUCCESS: Global 2D background maps dataset generated completely!        \n");
    printf(" Saved File: stage_33_master_background_maps.fits                                \n");
    printf("========================================================================\n");

    /* 7. Free allocations and shutdown cleanly */
    free(pixels); free(bg_map); free(tile_buffer);
    fits_close_file(f_in, &status);
    fits_close_file(f_out, &status);
    return status;
}
