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

    long global_row_counter = 1; // Start auto-increment ID counter at 1

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

    // 3. Inject the New Schema Columns (ID and SOURCE_FILE)
    int existing_fields = 0;
    fits_get_num_cols(outfptr, &existing_fields, &status);

    int id_col_num = existing_fields + 1;         // Position for ID column
    int tracker_col_num = existing_fields + 2;    // Position for SOURCE_FILE column

    // Inject ID column ("1J" = 32-bit Integer)
    printf("Injecting schema column 'ID' at position %d...\n", id_col_num);
    fits_insert_col(outfptr, id_col_num, "ROW_ID", "1J", &status);

    // Inject SOURCE_FILE column ("30A" = 30-char string)
    printf("Injecting schema column 'SOURCE_FILE' at position %d...\n", tracker_col_num);
    fits_insert_col(outfptr, tracker_col_num, "SOURCE_FILE", "30A", &status);

    if (status) {
        fits_report_error(stderr, status);
        fits_close_file(outfptr, &status);
        return status;
    }

    // 4. Fill ID and Source tracking arrays for rows brought over from the first file
    long master_rows = 0;
    fits_get_num_rows(outfptr, &master_rows, &status);

    for (long r = 1; r <= master_rows; r++) {
        // Write dynamic auto-incrementing ID
        fits_write_col(outfptr, TINT, id_col_num, r, 1, 1, &global_row_counter, &status);

        // Write the filename string
        char *filename_ptr = source_files[0];
        fits_write_col(outfptr, TSTRING, tracker_col_num, r, 1, 1, &filename_ptr, &status);

        global_row_counter++; // Increment key index
    }

    // 5. Loop through remaining source files to extract rows, append them, and apply trackers
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
        fits_get_num_rows(outfptr, &master_rows, &status); // Recalculate target structural ceiling

        if (source_rows > 0) {
            // Allocate blank staging slots at the end of the file container
            if (fits_insert_rows(outfptr, master_rows, source_rows, &status)) {
                fits_report_error(stderr, status);
                fits_close_file(infptr, &status);
                break;
            }

            // Copy each old data column over to the newly expanded row slots
            for (int col = 1; col <= existing_fields; col++) {
                int datatype;
                long repeat, width;
                fits_get_coltype(infptr, col, &datatype, &repeat, &width, &status);

                if (datatype == TSTRING) {
                    char **str_buffer = malloc(source_rows * sizeof(char*));
                    for(int r=0; r<source_rows; r++) str_buffer[r] = malloc(width + 1);

                    fits_read_col(infptr, TSTRING, col, 1, 1, source_rows, NULL, str_buffer, NULL, &status);
                    fits_write_col(outfptr, TSTRING, col, master_rows + 1, 1, source_rows, str_buffer, &status);

                    for(int r=0; r<source_rows; r++) free(str_buffer[r]);
                    free(str_buffer);
                } 
                else if (datatype == TFLOAT) {
                    float *flt_buffer = malloc(source_rows * sizeof(float));
                    fits_read_col(infptr, TFLOAT, col, 1, 1, source_rows, NULL, flt_buffer, NULL, &status);
                    fits_write_col(outfptr, TFLOAT, col, master_rows + 1, 1, source_rows, flt_buffer, &status);
                    free(flt_buffer);
                }
                else if (datatype == TINT) {
                    int *int_buffer = malloc(source_rows * sizeof(int));
                    fits_read_col(infptr, TINT, col, 1, 1, source_rows, NULL, int_buffer, NULL, &status);
                    fits_write_col(outfptr, TINT, col, master_rows + 1, 1, source_rows, int_buffer, &status);
                    free(int_buffer);
                }
            }

            // Fill the ID and SOURCE_FILE data loops for these newly appended rows
            for (long r = 1; r <= source_rows; r++) {
                long target_row = master_rows + r;

                // Write unique sequence index
                fits_write_col(outfptr, TINT, id_col_num, target_row, 1, 1, &global_row_counter, &status);

                // Write the tracking string
                char *filename_ptr = source_files[i];
                fits_write_col(outfptr, TSTRING, tracker_col_num, target_row, 1, 1, &filename_ptr, &status);

                global_row_counter++; // Safely advance loop increment
            }
        }

        fits_close_file(infptr, &status);
    }

    // 6. Complete and save layout updates
    fits_close_file(outfptr, &status);

    if (status) {
        fits_report_error(stderr, status);
    } else {
        printf("\nUnified rows compiled! Wrote %ld rows total with dynamic IDs to 'master_tracked.fits'.\n", global_row_counter - 1);
    }

    return status;
}
