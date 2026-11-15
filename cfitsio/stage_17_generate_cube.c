#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;

    srand((unsigned int)time(NULL));

    /* 1. Create a BRAND NEW file specifically for the 3D cube series */
    if (fits_create_file(&fptr, "!stage_17_time_cube.fits", &status)) {
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Primary HDU Layout Baseline Marker */
    int primary_bitpix = 8;
    int primary_naxis = 0;
    long primary_naxes[] = {0};
    fits_create_img(fptr, primary_bitpix, primary_naxis, primary_naxes, &status);

    /* 3. Define a 3D Image Cube Extension (256 columns x 256 rows x 5 time frames) */
    int bitpix = FLOAT_IMG;
    int naxis = 3;
    long naxes[] = {256, 256, 5};
    long npixels_frame = naxes[0] * naxes[1];
    long total_pixels = npixels_frame * naxes[2];

    if (fits_create_img(fptr, bitpix, naxis, naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }
    fits_write_key(fptr, TSTRING, "EXTNAME", "STARRY_SKY_CUBE", "Time Series Simulation", &status);

    /* --- APPEND DESCRIPTIVE SCIENCE METADATA --- */
    double exptime = 300.0;
    char filter[] = "V";
    char telescope[] = "Virtual-Sim-Telescope";

    fits_write_key(fptr, TDOUBLE, "EXPTIME",  &exptime,   "Total exposure time in seconds", &status);
    fits_write_key(fptr, TSTRING, "FILTER",   filter,     "Optical filter band name", &status);
    fits_write_key(fptr, TSTRING, "TELESCOP", telescope,  "Observational facility source", &status);
    /* ------------------------------------------- */

    /* --- INJECT SPATIAL WCS METADATA (RA / DEC Axes 1 and 2) --- */
    double crval1 = 291.3750;
    double crval2 = 42.7833;
    double crpix1 = 128.5;
    double crpix2 = 128.5;
    double cdelt1 = -0.00006944;
    double cdelt2 =  0.00006944;

    fits_write_key(fptr, TSTRING, "CTYPE1", "RA---TAN", "Coordinate type / Tangent Projection", &status);
    fits_write_key(fptr, TSTRING, "CTYPE2", "DEC--TAN", "Coordinate type / Tangent Projection", &status);
    fits_write_key(fptr, TDOUBLE, "CRVAL1", &crval1, "Ref value of primary axis (deg)", &status);
    fits_write_key(fptr, TDOUBLE, "CRVAL2", &crval2, "Ref value of secondary axis (deg)", &status);
    fits_write_key(fptr, TDOUBLE, "CRPIX1", &crpix1, "Ref pixel location of primary axis", &status);
    fits_write_key(fptr, TDOUBLE, "CRPIX2", &crpix2, "Ref pixel location of secondary axis", &status);
    fits_write_key(fptr, TDOUBLE, "CDELT1", &cdelt1, "Spatial increment per pixel (deg)", &status);
    fits_write_key(fptr, TDOUBLE, "CDELT2", &cdelt2, "Spatial increment per pixel (deg)", &status);
    fits_write_key(fptr, TSTRING, "CUNIT1", "deg", "Physical units of primary axis", &status);
    fits_write_key(fptr, TSTRING, "CUNIT2", "deg", "Physical units of secondary axis", &status);

    /* --- NEW: INJECT WCS-TIME METADATA (Axis 3) --- */
    double crval3 = 0.0;          /* Observations begin at elapsed time 0.0s */
    double crpix3 = 1.0;          /* Reference point starts at frame slice 1 */
    double cdelt3 = 2.0;          /* Each time step frame slice represents a 2.0-second delta */

    fits_write_key(fptr, TSTRING, "CTYPE3", "TIME", "Coordinate axis type represents Time", &status);
    fits_write_key(fptr, TSTRING, "CUNIT3", "s",    "SI time measurement unit (seconds)", &status);
    fits_write_key(fptr, TDOUBLE, "CRVAL3", &crval3, "Time baseline reference value at origin", &status);
    fits_write_key(fptr, TDOUBLE, "CRPIX3", &crpix3, "Reference coordinate pixel index frame layout", &status);
    fits_write_key(fptr, TDOUBLE, "CDELT3", &cdelt3, "Temporal grid step duration interval size", &status);
    /* ---------------------------------------------- */

    /* 4. Allocate memory buffer for the entire 3D array cube */
    float *cube_pixels = (float *)malloc(total_pixels * sizeof(float));
    if (cube_pixels == NULL) {
        printf("Memory allocation failed!\n");
        fits_close_file(fptr, &status);
        return 1;
    }

    /* 5. Pre-generate stable baseline positions for 50 tracking targets */
    int nstars = 50;
    float base_x[50], base_y[50], base_flux[50], base_sigma[50];
    for (int s = 0; s < nstars; s++) {
        base_x[s] = (float)(rand() % naxes[0]);
        base_y[s] = (float)(rand() % naxes[1]);
        base_flux[s] = 0.5f + ((float)rand() / (float)RAND_MAX) * 2.5f;
        base_sigma[s] = 0.8f + ((float)rand() / (float)RAND_MAX) * 1.2f;
    }

    /* 6. Render the 3D data slices loop sequentially */
    for (int frame = 0; frame < naxes[2]; frame++) {
        long frame_offset = frame * npixels_frame;

        for (long i = 0; i < npixels_frame; i++) {
            cube_pixels[frame_offset + i] = ((float)rand() / (float)RAND_MAX) * 0.05f;
        }

        for (int s = 0; s < nstars; s++) {
            float twist_flux = base_flux[s] * (1.0f + (((float)rand() / (float)RAND_MAX) * 0.4f - 0.2f));
            float jitter_x = base_x[s] + (((float)rand() / (float)RAND_MAX) * 0.4f - 0.2f);
            float jitter_y = base_y[s] + (((float)rand() / (float)RAND_MAX) * 0.4f - 0.2f);

            int box_radius = (int)(base_sigma[s] * 4);
            int start_x = (int)jitter_x - box_radius;
            int end_x   = (int)jitter_x + box_radius;
            int start_y = (int)jitter_y - box_radius;
            int end_y   = (int)jitter_y + box_radius;

            if (start_x < 0) start_x = 0;
            if (end_x >= naxes[0]) end_x = naxes[0] - 1;
            if (start_y < 0) start_y = 0;
            if (end_y >= naxes[1]) end_y = naxes[1] - 1;

            for (int y = start_y; y <= end_y; y++) {
                for (int x = start_x; x <= end_x; x++) {
                    float dx = (float)x - jitter_x;
                    float dy = (float)y - jitter_y;
                    float dist_sq = (dx * dx) + (dy * dy);
                    float profile = twist_flux * expf(-dist_sq / (2.0f * base_sigma[s] * base_sigma[s]));

                    cube_pixels[frame_offset + (y * naxes[0]) + x] += profile;
                }
            }
        }
    }

    /* 7. Write the entire continuous 3D data cube to disk storage */
    long firstelem_cube[] = {1, 1, 1};
    if (fits_write_pix(fptr, TFLOAT, firstelem_cube, total_pixels, cube_pixels, &status)) {
        fits_report_error(stderr, status);
    } else {
        printf("====================================================================\n");
        printf(" SUCCESS: 3D Time-Series FITS Cube Created with Native WCS-TIME!  \n");
        printf(" File Saved: stage_17_time_cube.fits                                \n");
        printf(" Time Cadency Configured: Axis 3 delta scale set to %0.1fs intervals\n", cdelt3);
        printf("====================================================================\n");
    }

    /* 8. Safe shutdown processing */
    free(cube_pixels);
    fits_close_file(fptr, &status);
    return status;
}
