#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int main() {
    fitsfile *infptr;   // Pointer to input source files
    fitsfile *outfptr;  // Pointer to the master row accumulator file
    int status = 0;
    int hdu_type;

    char *source_files[] = {"table1.fits", "table2.fits", "table3.fits"};
    int num_files = 3;

    // 1. Open the first file to initialize our master template base
    // We make a physical copy first so we have the layout ready
    printf("Initializing master file from %s...\n", source_files[0]);
    if (fits_open_file(&infptr, source_files[0], READONLY, &status)) {
        fits_report_error(stderr, status);
        return status;
    }

    if (fits_create_file(&outfptr, "!master_combined.fits", &status)) {
        fits_report_error(stderr, status);
        fits_close_file(infptr, &status);
        return status;
    }

    // Copy all current structures (Primary HDU + all extensions) from file 1
    if (fits_copy_file(infptr, outfptr, 1, 1, 1, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(infptr, &status);
        fits_close_file(outfptr, &status);
        return status;
    }
    fits_close_file(infptr, &status);

    // 2. Point the master file target to our active table extension (HDU 4 for example)
    // You can also target it by name using fits_movnam_hdu
    char extname[] = "STAR_KINEMATICS";
    if (fits_movnam_hdu(outfptr, BINARY_TBL, extname, 0, &status)) {
        fprintf(stderr, "Error: Master table '%s' not found.\n", extname);
        fits_close_file(outfptr, &status);
        return status;
    }

    // 3. Loop through subsequent files to append rows
    for (int i = 1; i < num_files; i++) {
        printf("Appending rows from: %s...\n", source_files[i]);

        if (fits_open_file(&infptr, source_files[i], READONLY, &status)) {
            fits_report_error(stderr, status);
            break;
        }

        // Navigate to the matching table extension in the input file
        if (fits_movnam_hdu(infptr, BINARY_TBL, extname, 0, &status)) {
            fprintf(stderr, "Warning: Table '%s' missing in %s. Skipping.\n", extname, source_files[i]);
            status = 0; // Reset error flag to continue loop safely
            fits_close_file(infptr, &status);
            continue;
        }

        // Find how many rows currently exist in both target and source tables
        long master_rows = 0;
        long source_rows = 0;
        fits_get_num_rows(outfptr, &master_rows, &status);
        fits_get_num_rows(infptr, &source_rows, &status);

        // Find how many bytes wide a single table row profile is
        long rowlen = 0;
        fits_get_rowsize(infptr, &rowlen, &status);

        if (source_rows > 0) {
            // Allocate blank row capacity at the very bottom of the master file table
            // master_rows = insertion point (appends to the end)
            // source_rows = number of blank rows to clear out space for
            if (fits_insert_rows(outfptr, master_rows, source_rows, &status)) {
                fits_report_error(stderr, status);
                fits_close_file(infptr, &status);
                break;
            }

            // Allocate a memory block to hold the raw raw-byte buffer payload
            unsigned char *row_buffer = (unsigned char *)malloc(source_rows * rowlen);
            if (row_buffer == NULL) {
                fprintf(stderr, "Memory allocation error.\n");
                status = 1;
                fits_close_file(infptr, &status);
                break;
            }

            // Read raw bytes block directly from source
            long firstrow = 1;
            long firstelem = 1;
            if (fits_read_tblbytes(infptr, firstrow, firstelem, source_rows * rowlen, row_buffer, &status)) {
                fits_report_error(stderr, status);
            }

            // Write raw bytes block directly into the newly padded space at the end of the master table
            long insert_start_row = master_rows + 1;
            if (fits_write_tblbytes(outfptr, insert_start_row, firstelem, source_rows * rowlen, row_buffer, &status)) {
                fits_report_error(stderr, status);
            }

            free(row_buffer);
        }

        fits_close_file(infptr, &status);
    }

    // 4. Save updates and finalize
    fits_close_file(outfptr, &status);

    if (status) {
        fits_report_error(stderr, status);
    } else {
        printf("\nAll rows consolidated successfully into a single master table table!\n");
    }

    return status;
}
