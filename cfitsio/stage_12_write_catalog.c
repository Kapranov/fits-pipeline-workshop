#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;
    int status = 0;

    /* 1. Open the file in READWRITE mode to populate the table extension */
    if (fits_open_file(&fptr, "stage_07_create_starry_sky_header.fits", READWRITE, &status)) {
        printf("Error: Could not open file.\n");
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Target the STARRY_SKY image extension layer to pull the pixels */
    if (fits_movnam_hdu(fptr, IMAGE_HDU, "STARRY_SKY", 0, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    int bitpix, naxis;
    long naxes[2] = {0, 0};
    fits_get_img_param(fptr, 2, &bitpix, &naxis, naxes, &status);
    long width = naxes[0];
    long height = naxes[1];
    long npixels = width * height;

    float *pixels = (float *)malloc(npixels * sizeof(float));
    long firstelem[2] = {1, 1};
    float nulval = 0.0f;
    int anynul = 0;
    fits_read_pix(fptr, TFLOAT, firstelem, npixels, &nulval, pixels, &anynul, &status);

    /* 3. Move over to your "STAR_DATA" binary table extension layer */
    if (fits_movnam_hdu(fptr, BINARY_TBL, "STAR_DATA", 0, &status)) {
        printf("Error: Could not locate 'STAR_DATA' table layer!\n");
        fits_report_error(stderr, status);
        free(pixels);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 4. Scan again and write matches straight to the extension rows */
    float star_threshold = 0.5f;
    int star_count = 0;

    for (long y = 1; y < height - 1; y++) {
        for (long x = 1; x < width - 1; x++) {
            long idx = (y * width) + x;
            float center_val = pixels[idx];

            if (center_val >= star_threshold) {
                if (center_val >= pixels[idx - 1] && center_val >= pixels[idx + 1] &&
                    center_val >= pixels[idx - width] && center_val >= pixels[idx + width] &&
                    center_val >= pixels[idx - width - 1] && center_val >= pixels[idx - width + 1] &&
                    center_val >= pixels[idx + width - 1] && center_val >= pixels[idx + width + 1]) {

                    star_count++;

                    /* Prepare data rows matching table layout schema columns */
                    int col_id = star_count;
                    float col_flux = center_val;

                    char col_name[20];
                    sprintf(col_name, "SIM_STAR_%02d", star_count);

                    /* Write fields row-by-row into columns 1, 2, and 3
                       Arguments: fptr, datatype, colnum, firstrow, firstelem, nelements, src_data_ptr, status */
                    fits_write_col(fptr, TINT,   1, star_count, 1, 1, &col_id,   &status);

                    /* Strings are passed directly as pointers */
                    char *name_ptr[1];
                    name_ptr[0] = col_name;
                    fits_write_col(fptr, TSTRING, 2, star_count, 1, 1, name_ptr,  &status);

                    fits_write_col(fptr, TFLOAT,  3, star_count, 1, 1, &col_flux, &status);
                }
            }
        }
    }

    if (status) {
        fits_report_error(stderr, status);
    } else {
        printf("==================================================\n");
        printf(" SUCCESS: Wrote %d records into 'STAR_DATA' Table! \n");
        printf("==================================================\n");
    }

    /* 5. Free and close file descriptor updates */
    free(pixels);
    fits_close_file(fptr, &status);
    return status;
}
