#include <stdio.h>
#include "fitsio.h"

int main() {
    fitsfile *fptr;   /* Pointer to the FITS file */
    int status = 0;   /* MUST initialize status to 0 */

    /* 1. Open the FITS file in READONLY mode */
    if (fits_open_file(&fptr, "stage_07_create_starry_sky_header.fits", READONLY, &status)) {
        fits_report_error(stderr, status);
        return status;
    }

    /* 2. Move to the specific image extension named "STARRY_SKY"
       The third parameter (0) matches any extension version type */
    int hdu_type;
    if (fits_movnam_hdu(fptr, IMAGE_HDU, "STARRY_SKY", 0, &status)) {
        printf("Error: Could not find the 'STARRY_SKY' extension layer!\n");
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return status;
    }

    /* 3. Prepare variables to hold the extracted header values */
    int nstars = 0;
    double exptime = 0.0;
    char filter[20] = {0};      /* Ensure buffer is large enough for string data */
    char telescope[40] = {0};
    char comment_buffer[80] = {0};

    /* 4. Extract variables using matching types (TINT, TDOUBLE, TSTRING)
       The last parameter can take a buffer to store the header card comment string (or NULL if ignored) */
    fits_read_key(fptr, TINT, "NSTARS", &nstars, comment_buffer, &status);
    fits_read_key(fptr, TDOUBLE, "EXPTIME", &exptime, NULL, &status);
    fits_read_key(fptr, TSTRING, "FILTER", filter, NULL, &status);
    fits_read_key(fptr, TSTRING, "TELESCOP", telescope, NULL, &status);

    /* 5. Check if any keys were missing or failed to parse */
    if (status) {
        printf("An error occurred while reading header keywords.\n");
        fits_report_error(stderr, status);
        // Reset status to 0 so we can safely close the file hook handle
        status = 0;
    } else {
        /* 6. Print out our successfully extracted data fields */
        printf("===========================================\n");
        printf("   FITS METADATA EXTRACTION SUCCESSFUL    \n");
        printf("===========================================\n");
        printf(" Telescope Facility : %s\n", telescope);
        printf(" Optical Band Filter: %s\n", filter);
        printf(" Exposure Duration  : %.1f seconds\n", exptime);
        printf(" Injected Star Count: %d stars\n", nstars);
        printf("===========================================\n");
    }

    /* 7. Close the file handle cleanly */
    fits_close_file(fptr, &status);
    if (status) {
        fits_report_error(stderr, status);
    }

    return status;
}
