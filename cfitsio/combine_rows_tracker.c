#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fitsio.h"

int main() {
    fitsfile *infptr;   // Pointer to input source files
    fitsfile *outfptr;  // Pointer to the master tracking file
    int status = 0;

    char *source_files[] = {"table1.fits", "table2.fits", "table3.fits"};
    int num_files = 3;
    char extname[] = "STAR_KINEMATICS";

    // 1. Open the first file to initialize our master template base structure
    printf("Initializing master file from %s...\n", source_files[0]);
    if (fits_open_file(&infptr, source_files[0], READONLY, &status)) {
        fits_report_error(stderr, status);
        return status;
    }

    if (fits_create_file(&outfptr, "!master_tracked.fits", &status)) {
        fits_report_error(stderr, status);
        fits_close_file(infptr, &status);
        return status;
    }

    // Copy everything from file 1 to establish the layout base
    if (fits_copy_file(infptr, outfptr, 1, 1, 1, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(infptr, &status);
        fits_close_file(outfptr, &status);
        return status;
    }
    fits_close_file(infptr, &status);

    // 2. Point to our active table extension inside the master file
    if (fits_movnam_hdu(outfptr, BINARY_TBL, extname, 0, &status)) {
        fprintf(stderr, "Error: Master table '%s' not found.\n", extname);
        fits_close_file(outfptr, &status);
        return status;
    }

    // 3. Append the brand new "SOURCE_FILE" tracking column to the master table schema
    // We place it at the end of the table. Let's find out how many columns exist first:
    int existing_fields = 0;
    fits_get_num_cols(outfptr, &existing_fields, &status);

    int tracker_col_num = existing_fields + 1; // It becomes the last column
    char *tracker_col_name = "SOURCE_FILE";
    char *tracker_col_format = "30A";          // Accommodates file names up to 30 characters

    printf("Injecting tracking schema column '%s' at position %d...\n", tracker_col_name, tracker_col_num);
    if (fits_insert_col(outfptr, tracker_col_num, tracker_col_name, tracker_col_format, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(outfptr, &status);
        return status;
    }

    // 4. Fill in the tracker strings for rows brought over from the first file (which are already in the table)
    long master_rows = 0;
    fits_get_num_rows(outfptr, &master_rows, &status);
    for (long r = 1; r <= master_rows; r++) {
        char *filename_ptr = source_files[0];
        fits_write_col(outfptr, TSTRING, tracker_col_num, r, 1, 1, &filename_ptr, &status);
    }

    // 5. Loop through remaining source files to extract rows and inject data + tracker flags
    for (int i = 1; i < num_files; i++) {
        printf("Processing and tagging rows from: %s...\n", source_files[i]);

        if (fits_open_file(&infptr, source_files[i], READONLY, &status)) {
            fits_report_error(stderr, status);
            break;
        }

        if (fits_movnam_hdu(infptr, BINARY_TBL, extname, 0, &status)) {
            fprintf(stderr, "Warning: Table '%s' missing in %s. Skipping.\n", extname, source_files[i]);
            status = 0;
            fits_close_file(infptr, &status);
            continue;
        }

        long source_rows = 0;
        fits_get_num_rows(infptr, &source_rows, &status);
        fits_get_num_rows(outfptr, &master_rows, &status); // Recalculate target ceiling baseline

        if (source_rows > 0) {
            // Create blank staging slots at the end of the file container
            if (fits_insert_rows(outfptr, master_rows, source_rows, &status)) {
                fits_report_error(stderr, status);
                fits_close_file(infptr, &status);
                break;
            }

            // Copy each old column over to the new rows one column at a time
            for (int col = 1; col <= existing_fields; col++) {
                int datatype;
                long repeat, width;

                // Determine source metadata profile dynamically to prevent runtime format mismatches
                fits_get_coltype(infptr, col, &datatype, &repeat, &width, &status);

                if (datatype == TSTRING) {
                    // String array copy handling
                    char **str_buffer = malloc(source_rows * sizeof(char*));
                    for(int r=0; r<source_rows; r++) str_buffer[r] = malloc(width + 1);

                    fits_read_col(infptr, TSTRING, col, 1, 1, source_rows, NULL, str_buffer, NULL, &status);
                    fits_write_col(outfptr, TSTRING, col, master_rows + 1, 1, source_rows, str_buffer, &status);

                    for(int r=0; r<source_rows; r++) free(str_buffer[r]);
                    free(str_buffer);
                } 
                else if (datatype == TFLOAT) {
                    // Float copy handling
                    float *flt_buffer = malloc(source_rows * sizeof(float));
                    fits_read_col(infptr, TFLOAT, col, 1, 1, source_rows, NULL, flt_buffer, NULL, &status);
                    fits_write_col(outfptr, TFLOAT, col, master_rows + 1, 1, source_rows, flt_buffer, &status);
                    free(flt_buffer);
                }
                else if (datatype == TINT) {
                    // Integer copy handling
                    int *int_buffer = malloc(source_rows * sizeof(int));
                    fits_read_col(infptr, TINT, col, 1, 1, source_rows, NULL, int_buffer, NULL, &status);
                    fits_write_col(outfptr, TINT, col, master_rows + 1, 1, source_rows, int_buffer, &status);
                    free(int_buffer);
                }
                // (Note: Add TDOUBLE block here if your table contains 1D double formats)
            }

            // Write the current input filename into the SOURCE_FILE column for these new rows
            for (long r = 1; r <= source_rows; r++) {
                long target_row = master_rows + r;
                char *filename_ptr = source_files[i];
                fits_write_col(outfptr, TSTRING, tracker_col_num, target_row, 1, 1, &filename_ptr, &status);
            }
        }

        fits_close_file(infptr, &status);
    }

    // 6. Complete and save layout updates
    fits_close_file(outfptr, &status);

    if (status) {
        fits_report_error(stderr, status);
    } else {
        printf("\nSuccessfully consolidated rows with clean source logging attributes into 'master_tracked.fits'!\n");
    }

    return status;
}
