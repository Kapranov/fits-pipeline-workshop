#include "fitsio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH  512
#define HEIGHT 512

// Set maximum bounds for dynamic memory allocation
#define MAX_ROWS 100
#define MAX_STARS 100

int append_dynamic_table(fitsfile *fptr, int *status) {
    if (*status) return *status;

    // 1. Open the source text file
    FILE *txt_file = fopen("highest_proper_motion_stars.data", "r");
    if (txt_file == NULL) {
        fprintf(stderr, "Error: Could not open star_data.txt file.\n");
        return 1;
    }

    // 2. Dynamically allocate arrays to hold the parsed data rows
    char (*hip_data)[20]  = malloc(MAX_ROWS * sizeof(*hip_data));
    char (*name_data)[30] = malloc(MAX_ROWS * sizeof(*name_data));
    float *v_data         = malloc(MAX_ROWS * sizeof(float));
    float *pm_data        = malloc(MAX_ROWS * sizeof(float));
    float *pa_data        = malloc(MAX_ROWS * sizeof(float));
    int *chart_data       = malloc(MAX_ROWS * sizeof(int));

    if (!hip_data || !name_data || !v_data || !pm_data || !pa_data || !chart_data) {
        fprintf(stderr, "Error: Memory allocation failed for table arrays.\n");
        if (txt_file) fclose(txt_file);
        return 1;
    }

    // 3. Read and parse lines from the text file loop-by-loop
    char line[256];
    long row_count = 0;

    while (fgets(line, sizeof(line), txt_file) && row_count < MAX_ROWS) {
        // Strip trailing newline character
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) < 10) continue; // Skip empty or broken lines

        // Parse line safely based on your specific spacing:
        // HIP (0-9), Name (10-29), V (30-34), PM (35-42), PA (43-48), Chart (49+)
        char hip_buf[20]  = {0};
        char name_buf[30] = {0};
        char v_buf[10]    = {0};
        char pm_buf[10]   = {0};
        char pa_buf[10]   = {0};
        char chart_buf[10]= {0};

        // Extract substrings reliably using width lengths
        strncpy(hip_buf,   &line[0],  9);
        strncpy(name_buf,  &line[10], 19);
        strncpy(v_buf,     &line[30], 5);
        strncpy(pm_buf,    &line[35], 7);
        strncpy(pa_buf,    &line[43], 5);
        strncpy(chart_buf, &line[49], 5);

        // Trim trailing whitespace from text elements
        int idx = strlen(name_buf) - 1;
        while (idx >= 0 && (name_buf[idx] == ' ' || name_buf[idx] == '\t')) {
            name_buf[idx] = '\0';
            idx--;
        }
        idx = strlen(hip_buf) - 1;
        while (idx >= 0 && (hip_buf[idx] == ' ' || hip_buf[idx] == '\t')) {
            hip_buf[idx] = '\0';
            idx--;
        }

        // Copy clean outputs into structural storage vectors
        strcpy(hip_data[row_count], hip_buf);
        strcpy(name_data[row_count], name_buf);
        v_data[row_count]     = (float)atof(v_buf);
        pm_data[row_count]    = (float)atof(pm_buf);
        pa_data[row_count]    = (float)atof(pa_buf);
        chart_data[row_count] = atoi(chart_buf);

        row_count++;
    }
    fclose(txt_file);

    if (row_count == 0) {
        fprintf(stderr, "Error: No data rows were extracted from file.\n");
        return 1;
    }

    // 4. Set up the FITS Binary Table parameters dynamically based on row count
    int ttype_kin = BINARY_TBL;
    int tfields_kin = 6;

    char *ttype_names_kin[] = {"HIP", "Name", "V", "PM", "PA", "Chart"};
    char *tform_kin[]       = {"20A", "30A", "1E", "1E", "1E", "1J"};
    char *tunit_kin[]       = {"", "", "mag", "arcsec/yr", "deg", ""};
    char *extname_kin       = "DYNAMIC_STAR_CATALOG";

    // 5. Append the table header container extension
    if (fits_create_tbl(fptr, ttype_kin, row_count, tfields_kin, ttype_names_kin, tform_kin, tunit_kin, extname_kin, status)) {
        fits_report_error(stderr, *status);
        return *status;
    }

    // 6. Pack data into columns sequentially using pointer indexing
    long f_row = 1;
    long f_elm = 1;

    // Convert fixed length arrays to a flat array layout format for cfitsio compatibility
    char **hip_ptrs  = malloc(row_count * sizeof(char*));
    char **name_ptrs = malloc(row_count * sizeof(char*));
    for (int i = 0; i < row_count; i++) {
        hip_ptrs[i]  = hip_data[i];
        name_ptrs[i] = name_data[i];
    }

    fits_write_col(fptr, TSTRING, 1, f_row, f_elm, row_count, hip_ptrs, status);
    fits_write_col(fptr, TSTRING, 2, f_row, f_elm, row_count, name_ptrs, status);
    fits_write_col(fptr, TFLOAT,  3, f_row, f_elm, row_count, v_data, status);
    fits_write_col(fptr, TFLOAT,  4, f_row, f_elm, row_count, pm_data, status);
    fits_write_col(fptr, TFLOAT,  5, f_row, f_elm, row_count, pa_data, status);
    fits_write_col(fptr, TINT,    6, f_row, f_elm, row_count, chart_data, status);

    // Free all temporary scratch arrays
    free(hip_ptrs);  free(name_ptrs);
    free(hip_data);  free(name_data);
    free(v_data);    free(pm_data);   free(pa_data);  free(chart_data);

    if (*status) {
        fits_report_error(stderr, *status);
    } else {
        printf("Successfully created dynamic table using data from text file! Loaded %ld rows.\n", row_count);
    }

    return *status;
}


int append_variable_stars_table(fitsfile *fptr, int *status) {
    if (*status) return *status;

    // Open your specific raw text file
    FILE *txt_file = fopen("brightest_variable_stars.dat", "r");
    if (!txt_file) {
        fprintf(stderr, "Error: Could not open data file.\n");
        return 1;
    }

    // Allocate memory vectors matching our custom columns
    char (*hip_data)[20]     = malloc(MAX_STARS * sizeof(*hip_data));
    char (*desig_data)[30]   = malloc(MAX_STARS * sizeof(*desig_data));
    float *v_data            = malloc(MAX_STARS * sizeof(float));
    float *ampl_data         = malloc(MAX_STARS * sizeof(float));
    float *period_data       = malloc(MAX_STARS * sizeof(float));
    char (*type_data)[15]    = malloc(MAX_STARS * sizeof(*type_data));
    char (*atlas_data)[10]   = malloc(MAX_STARS * sizeof(*atlas_data));
    int *chart_data          = malloc(MAX_STARS * sizeof(int));

    char line[512];
    long row_count = 0;

    // Skip the first line containing the headers
    if (fgets(line, sizeof(line), txt_file) == NULL) {
        fclose(txt_file);
        return 1;
    }

    // Loop through the data lines
    while (fgets(line, sizeof(line), txt_file) && row_count < MAX_STARS) {
        // Strip out trailing newlines safely
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) < 5) continue;

        // Set up temporary target buffers
        char hip[20] = "", desig[30] = "", v[15] = "", ampl[15] = "";
        char period[15] = "", type[15] = "", atlas[10] = "", chart[15] = "";

        // Safe token parsing logic using sscanf
        // Handles collapsed whitespace, spaces inside strings require tab separation
        // Alternative if using tabs: use a custom loop with strsep() or strtok()
        int parsed = sscanf(line, "%19s\t%29[^\t]\t%14s\t%14s\t%14s\t%14s\t%9s\t%14s",
                            hip, desig, v, ampl, period, type, atlas, chart);

        // If sscanf struggles with double tabs, a manual tokenizer is used here:
        char *ptr = line;
        char *tokens[8] = {NULL};
        int col = 0;

        // Manual tab-split parsing to correctly preserve empty cells
        while (ptr && col < 8) {
            tokens[col++] = ptr;
            ptr = strchr(ptr, '\t');
            if (ptr) {
                *ptr = '\0';
                ptr++;
            }
        }

        // Fill storage buffers safely, checking for empty tokens
        strcpy(hip_data[row_count],     (col > 0 && strlen(tokens[0]) > 0) ? tokens[0] : "");
        strcpy(desig_data[row_count],   (col > 1 && strlen(tokens[1]) > 0) ? tokens[1] : "");
        v_data[row_count] =             (col > 2 && strlen(tokens[2]) > 0) ? (float)atof(tokens[2]) : 0.0f;
        ampl_data[row_count] =          (col > 3 && strlen(tokens[3]) > 0) ? (float)atof(tokens[3]) : 0.0f;
        period_data[row_count] =        (col > 4 && strlen(tokens[4]) > 0) ? (float)atof(tokens[4]) : 0.0f; // Handles blank periods
        strcpy(type_data[row_count],    (col > 5 && strlen(tokens[5]) > 0) ? tokens[5] : "");
        strcpy(atlas_data[row_count],   (col > 6 && strlen(tokens[6]) > 0) ? tokens[6] : "");
        chart_data[row_count] =         (col > 7 && strlen(tokens[7]) > 0) ? atoi(tokens[7]) : 0;

        row_count++;
    }
    fclose(txt_file);

    // Create the actual table structure inside your FITS container
    int ttype = BINARY_TBL;
    int tfields = 8;
    char *ttype_names[] = {"HIP", "Designation", "V", "Ampl", "Period", "Type", "Atlas_Class", "Chart"};
    char *tform[] = {"20A", "30A", "1E", "1E", "1E", "15A", "10A", "1J"};
    char *tunit[] = {"", "", "mag", "mag", "days", "", "", ""};

    if (fits_create_tbl(fptr, ttype, row_count, tfields, ttype_names, tform, tunit, "VARIABLE_STARS", status)) {
        fits_report_error(stderr, *status);
        return *status;
    }

    // Set string layout mapping arrays required by cfitsio
    char **hip_ptrs = malloc(row_count * sizeof(char*));
    char **desig_ptrs = malloc(row_count * sizeof(char*));
    char **type_ptrs = malloc(row_count * sizeof(char*));
    char **atlas_ptrs = malloc(row_count * sizeof(char*));

    for (int i = 0; i < row_count; i++) {
        hip_ptrs[i] = hip_data[i];
        desig_ptrs[i] = desig_data[i];
        type_ptrs[i] = type_data[i];
        atlas_ptrs[i] = atlas_data[i];
    }

    // Sequential column block writing commands
    long f_row = 1, f_elm = 1;
    fits_write_col(fptr, TSTRING, 1, f_row, f_elm, row_count, hip_ptrs, status);
    fits_write_col(fptr, TSTRING, 2, f_row, f_elm, row_count, desig_ptrs, status);
    fits_write_col(fptr, TFLOAT,  3, f_row, f_elm, row_count, v_data, status);
    fits_write_col(fptr, TFLOAT,  4, f_row, f_elm, row_count, ampl_data, status);
    fits_write_col(fptr, TFLOAT,  5, f_row, f_elm, row_count, period_data, status);
    fits_write_col(fptr, TSTRING, 6, f_row, f_elm, row_count, type_ptrs, status);
    fits_write_col(fptr, TSTRING, 7, f_row, f_elm, row_count, atlas_ptrs, status);
    fits_write_col(fptr, TINT,    8, f_row, f_elm, row_count, chart_data, status);

    // Free string pointers lists
    free(hip_ptrs); free(desig_ptrs); free(type_ptrs); free(atlas_ptrs);
    // Free numeric blocks and data layouts
    free(hip_data); free(desig_data); free(v_data); free(ampl_data);
    free(period_data); free(type_data); free(atlas_data); free(chart_data);

    return *status;
}

#include <stdio.h>
#include <stdlib.h>
#include "fitsio.h"

int copy_hdu_between_files() {
    fitsfile *infptr;   // Source FITS file pointer
    fitsfile *outfptr;  // Destination FITS file pointer
    int status = 0;
    int hdu_type;

    // 1. Open the source FITS file that contains the data you want to copy
    if (fits_open_file(&infptr, "test.fits", READONLY, &status)) {
        fits_report_error(stderr, status);
        return status;
    }

    // 2. Move to the specific HDU you want to copy (e.g., HDU 2, which could be an image or table)
    int hdu_num = 2;
    if (fits_movabs_hdu(infptr, hdu_num, &hdu_type, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(infptr, &status);
        return status;
    }

    // 3. Open or create the destination FITS file
    // The "!" prefix overwrites the file if it already exists
    if (fits_create_file(&outfptr, "!master_output.fits", &status)) {
        fits_report_error(stderr, status);
        fits_close_file(infptr, &status);
        return status;
    }

    // 4. Copy the entire current HDU from the source file into the destination file
    // This copies all data array structures and metadata header cards automatically.
    // The 'morekeys' parameter allows you to specify reserve space for extra headers (0 if none)
    int morekeys = 0;
    if (fits_copy_hdu(infptr, outfptr, morekeys, &status)) {
        fits_report_error(stderr, status);
    } else {
        printf("Successfully copied HDU %d from source into destination FITS file!\n", hdu_num);
    }

    // 5. Clean up and close both file handles safely
    fits_close_file(infptr, &status);
    fits_close_file(outfptr, &status);

    return status;
}

int main() {
    fitsfile *fptr;
    int status = 0;

    // Allocate memory for our image extension data
    float *pixels = (float *)malloc(WIDTH * HEIGHT * sizeof(float));
    if (pixels == NULL) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return 1;
    }
    for (int i = 0; i < WIDTH * HEIGHT; i++) pixels[i] = (float)i;

    // 1. Create the new physical FITS file container
    if (fits_create_file(&fptr, "!multi_ext.fits", &status)) {
        fits_report_error(stderr, status);
        free(pixels);
        return status;
    }

    // ====================================================================
    // HDU 1: PRIMARY HDU (Empty data, metadata only)
    // ====================================================================
    int bitpix = BYTE_IMG;
    int naxis = 0;
    long naxes[1] = {0};

    if (fits_create_img(fptr, bitpix, naxis, naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        free(pixels);
        return status;
    }
    // Add a global header keyword to the Primary HDU
    fits_update_key(fptr, TSTRING, "ORIGIN", "MyObservatory", "Global source", &status);

    // ====================================================================
    // HDU 2: IMAGE EXTENSION (Appending our 2D Matrix)
    // ====================================================================
    // Calling fits_create_img again automatically creates a new extension (XTENSION = 'IMAGE')
    int ext_bitpix = FLOAT_IMG;
    int ext_naxis = 2;
    long ext_naxes[] = {WIDTH, HEIGHT};

    if (fits_create_img(fptr, ext_bitpix, ext_naxis, ext_naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        free(pixels);
        return status;
    }

    // Name this specific extension so it can be easily found later
    fits_update_key(fptr, TSTRING, "EXTNAME", "SCI_DATA", "Extension Name", &status);
    fits_update_key(fptr, TSTRING, "OBJECT", "M31", "Target object", &status);

    // Write the pixel data into this newly appended extension
    LONGLONG firstelem = 1;
    LONGLONG nelements = (LONGLONG)WIDTH * HEIGHT;

    if (fits_write_img(fptr, TFLOAT, firstelem, nelements, pixels, &status)) {
        fits_report_error(stderr, status);
    } else {
        printf("Successfully created multi-extension FITS file with an image extension.\n");
    }

    // ====================================================================
    // HDU 3: BINARY TABLE EXTENSION
    // ====================================================================
    // 1. Define the table structure dimensions
    int ttype = BINARY_TBL;       // Type of table extension
    long nrows = 3;               // Initial number of rows
    int tfields = 2;              // Number of columns

    // 2. Define the columns (Names, Formats, and Units)
    char *ttype_names[] = {"STAR_ID", "MAGNITUDE"}; // Column names
    char *tform[] = {"10A", "1D"};                 // 10A = 10-char string, 1D = 1 double float
    char *tunit[] = {"", "mag"};                   // Physical units
    char *extname = "STARS_CATALOG";               // Extension name

    // 3. Create the empty table extension section
    if (fits_create_tbl(fptr, ttype, nrows, tfields, ttype_names, tform, tunit, extname, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        free(pixels);
        return status;
    }

    // 4. Prepare your data arrays
    char *star_ids[] = {"Alpha", "Beta", "Gamma"};
    double magnitudes[] = {4.25, 1.12, 2.84};

    // 5. Write the data into the columns (1-based index)
    long firstrow = 1;
    // Rename it to something distinct like tbl_firstelem
    long tbl_firstelem = 1;

    // Then update your writing functions to use the new name:
    fits_write_col(fptr, TSTRING, 1, firstrow, tbl_firstelem, nrows, star_ids, &status);
    fits_write_col(fptr, TDOUBLE, 2, firstrow, tbl_firstelem, nrows, magnitudes, &status);

    // Write column 2 (Doubles)
    if (fits_write_col(fptr, TDOUBLE, 2, firstrow, firstelem, nrows, magnitudes, &status)) {
        fits_report_error(stderr, status);
    } else {
        printf("Successfully appended Binary Table section.\n");
    }

    // ====================================================================
    // HDU 4: BINARY TABLE EXTENSION
    // ====================================================================
    // Call the dynamic loader function
    append_dynamic_table(fptr, &status);

    // ====================================================================
    // HDU 5: BINARY TABLE EXTENSION
    // ====================================================================
    // Call the dynamic loader function
    append_variable_stars_table(fptr, &status);

    // ====================================================================
    // HDU 6: IMAGE EXTENSION
    // ====================================================================
    // Call the dynamic loader function
    copy_hdu_between_files();

    // Clean up and close
    fits_close_file(fptr, &status);
    free(pixels);

    if (status) {
        fits_report_error(stderr, status);
    }
    return status;
}

