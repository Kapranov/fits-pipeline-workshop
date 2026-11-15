#include "fitsio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH  512
#define HEIGHT 512

#define MAX_ROWS 100
#define MAX_STARS 100

int append_dynamic_table(fitsfile *fptr, int *status) {
    if (*status) return *status;

    FILE *txt_file = fopen("highest_proper_motion_stars.data", "r");
    if (txt_file == NULL) {
        fprintf(stderr, "[Warning] Could not open highest_proper_motion_stars.data. Skipping HDU 4.\n");
        return 0;
    }

    char (*hip_data)[20]  = malloc(MAX_ROWS * sizeof(*hip_data));
    char (*name_data)[30] = malloc(MAX_ROWS * sizeof(*name_data));
    float *v_data         = malloc(MAX_ROWS * sizeof(float));
    float *pm_data        = malloc(MAX_ROWS * sizeof(float));
    float *pa_data        = malloc(MAX_ROWS * sizeof(float));
    int *chart_data       = malloc(MAX_ROWS * sizeof(int));

    if (!hip_data || !name_data || !v_data || !pm_data || !pa_data || !chart_data) {
        fclose(txt_file);
        return 1;
    }

    char line[256]; // FIXED: Allocated a robust string array instead of a single char
    long row_count = 0;

    while (fgets(line, sizeof(line), txt_file) && row_count < MAX_ROWS) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) < 10) continue;

        char hip_buf[20]  = {0};
        char name_buf[30] = {0};
        char v_buf[10]    = {0};
        char pm_buf[10]   = {0};
        char pa_buf[10]   = {0};
        char chart_buf[10]= {0};

        strncpy(hip_buf,   &line[0],  9);
        strncpy(name_buf,  &line[10], 19);
        strncpy(v_buf,     &line[30], 5);
        strncpy(pm_buf,    &line[35], 7);
        strncpy(pa_buf,    &line[43], 5);
        strncpy(chart_buf, &line[49], 5);

        int idx = (int)strlen(name_buf) - 1;
        while (idx >= 0 && (name_buf[idx] == ' ' || name_buf[idx] == '\t')) {
            name_buf[idx] = '\0';
            idx--;
        }
        idx = (int)strlen(hip_buf) - 1;
        while (idx >= 0 && (hip_buf[idx] == ' ' || hip_buf[idx] == '\t')) {
            hip_buf[idx] = '\0';
            idx--;
        }

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
        free(hip_data); free(name_data); free(v_data); free(pm_data); free(pa_data); free(chart_data);
        return 0;
    }

    int ttype_kin = BINARY_TBL;
    int tfields_kin = 6;
    char *ttype_names_kin[] = {"HIP", "Name", "V", "PM", "PA", "Chart"};
    char *tform_kin[]       = {"20A", "30A", "1E", "1E", "1E", "1J"};
    char *tunit_kin[]       = {"", "", "mag", "arcsec/yr", "deg", ""};

    if (fits_create_tbl(fptr, ttype_kin, row_count, tfields_kin, ttype_names_kin, tform_kin, tunit_kin, "DYNAMIC_STAR_CATALOG", status)) {
        free(hip_data); free(name_data); free(v_data); free(pm_data); free(pa_data); free(chart_data);
        return *status;
    }

    long f_row = 1;
    long f_elm = 1;
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

    free(hip_ptrs);  free(name_ptrs);
    free(hip_data);  free(name_data);
    free(v_data);    free(pm_data);   free(pa_data);  free(chart_data);

    return *status;
}

int append_variable_stars_table(fitsfile *fptr, int *status) {
    if (*status) return *status;

    FILE *txt_file = fopen("brightest_variable_stars.dat", "r");
    if (!txt_file) {
        fprintf(stderr, "[Warning] Could not open brightest_variable_stars.dat. Skipping HDU 5.\n");
        return 0;
    }

    char (*hip_data)[20]     = malloc(MAX_STARS * sizeof(*hip_data));
    char (*desig_data)[30]   = malloc(MAX_STARS * sizeof(*desig_data));
    float *v_data            = malloc(MAX_STARS * sizeof(float));
    float *ampl_data         = malloc(MAX_STARS * sizeof(float));
    float *period_data       = malloc(MAX_STARS * sizeof(float));
    char (*type_data)[15]    = malloc(MAX_STARS * sizeof(*type_data));
    char (*atlas_data)[10]   = malloc(MAX_STARS * sizeof(*atlas_data));
    int *chart_data          = malloc(MAX_STARS * sizeof(int));

    char line[512]; // FIXED: Allocated a robust string array instead of a single char
    long row_count = 0;

    if (fgets(line, sizeof(line), txt_file) == NULL) {
        fclose(txt_file);
        free(hip_data); free(desig_data); free(v_data); free(ampl_data);
        free(period_data); free(type_data); free(atlas_data); free(chart_data);
        return 0;
    }

    while (fgets(line, sizeof(line), txt_file) && row_count < MAX_STARS) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) < 5) continue;

        char *ptr = line;
        char *tokens[8] = {NULL};
        int col = 0;

        while (ptr && col < 8) {
            tokens[col++] = ptr;
            ptr = strchr(ptr, '\t');
            if (ptr) {
                *ptr = '\0';
                ptr++;
            }
        }

        strcpy(hip_data[row_count],     (col > 0 && tokens[0] && strlen(tokens[0]) > 0) ? tokens[0] : "");
        strcpy(desig_data[row_count],   (col > 1 && tokens[1] && strlen(tokens[1]) > 0) ? tokens[1] : "");
        v_data[row_count] =             (col > 2 && tokens[2] && strlen(tokens[2]) > 0) ? (float)atof(tokens[2]) : 0.0f;
        ampl_data[row_count] =          (col > 3 && tokens[3] && strlen(tokens[3]) > 0) ? (float)atof(tokens[3]) : 0.0f;
        period_data[row_count] =        (col > 4 && tokens[4] && strlen(tokens[4]) > 0) ? (float)atof(tokens[4]) : 0.0f;
        strcpy(type_data[row_count],    (col > 5 && tokens[5] && strlen(tokens[5]) > 0) ? tokens[5] : "");
        strcpy(atlas_data[row_count],   (col > 6 && tokens[6] && strlen(tokens[6]) > 0) ? tokens[6] : "");
        chart_data[row_count] =         (col > 7 && tokens[7] && strlen(tokens[7]) > 0) ? atoi(tokens[7]) : 0;

        row_count++;
    }
    fclose(txt_file);

    int ttype = BINARY_TBL;
    int tfields = 8;
    char *ttype_names[] = {"HIP", "Designation", "V", "Ampl", "Period", "Type", "Atlas_Class", "Chart"};
    char *tform[] = {"20A", "30A", "1E", "1E", "1E", "15A", "10A", "1J"};
    char *tunit[] = {"", "", "mag", "mag", "days", "", "", ""};

    if (fits_create_tbl(fptr, ttype, row_count, tfields, ttype_names, tform, tunit, "VARIABLE_STARS", status)) {
        free(hip_data); free(desig_data); free(v_data); free(ampl_data);
        free(period_data); free(type_data); free(atlas_data); free(chart_data);
        return *status;
    }

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

    long f_row = 1, f_elm = 1;
    fits_write_col(fptr, TSTRING, 1, f_row, f_elm, row_count, hip_ptrs, status);
    fits_write_col(fptr, TSTRING, 2, f_row, f_elm, row_count, desig_ptrs, status);
    fits_write_col(fptr, TFLOAT,  3, f_row, f_elm, row_count, v_data, status);
    fits_write_col(fptr, TFLOAT,  4, f_row, f_elm, row_count, ampl_data, status);
    fits_write_col(fptr, TFLOAT,  5, f_row, f_elm, row_count, period_data, status);
    fits_write_col(fptr, TSTRING, 6, f_row, f_elm, row_count, type_ptrs, status);
    fits_write_col(fptr, TSTRING, 7, f_row, f_elm, row_count, atlas_ptrs, status);
    fits_write_col(fptr, TINT,    8, f_row, f_elm, row_count, chart_data, status);

    free(hip_ptrs); free(desig_ptrs); free(type_ptrs); free(atlas_ptrs);
    free(hip_data); free(desig_data); free(v_data); free(ampl_data);
    free(period_data); free(type_data); free(atlas_data); free(chart_data);

    return *status;
}

int copy_hdu_between_files(fitsfile *dest_fptr, int *status) {
    fitsfile *infptr;
    int read_status = 0;
    int hdu_type;

    if (fits_open_file(&infptr, "test.fits", READONLY, &read_status)) {
        fprintf(stderr, "Error: Could not open test.fits for reading.\n");
        return read_status;
    }

    fits_movabs_hdu(infptr, 1, &hdu_type, &read_status);

    int naxis = 0;
    long naxes[2] = {0, 0};
    fits_get_img_dim(infptr, &naxis, &read_status);
    fits_get_img_size(infptr, 2, naxes, &read_status);

    if (read_status || naxis == 0) {
        fprintf(stderr, "Error: test.fits does not contain valid 2D image data.\n");
        fits_close_file(infptr, &read_status);
        return read_status;
    }

    long total_pixels = naxes[0] * naxes[1];
    float *buffer = (float *)malloc(total_pixels * sizeof(float));
    float nulval = 0.0f;
    int anynul;

    fits_read_img(infptr, TFLOAT, 1, total_pixels, &nulval, buffer, &anynul, &read_status);
    fits_close_file(infptr, &read_status);

    if (read_status) {
        fprintf(stderr, "Error reading pixel contents out of test.fits.\n");
        free(buffer);
        return read_status;
    }

    // ====================================================================
    // EXPLICIT WRITER ROUTING WITH HEADER RESERVATION
    // ====================================================================
    *status = 0;
    fits_create_img(dest_fptr, FLOAT_IMG, naxis, naxes, status);
    fits_set_hdrsize(dest_fptr, 24, status); // Reserves header storage blocks explicitly
    fits_update_key(dest_fptr, TSTRING, "EXTNAME", "WCS_IMAGE", "Name of this copied array", status);
    fits_update_key(dest_fptr, TSTRING, "CTYPE1", "RA---TAN", "Coordinate type Axis 1", status);
    fits_update_key(dest_fptr, TSTRING, "CTYPE2", "DEC--TAN", "Coordinate type Axis 2", status);

    double crpix1 = (double)naxes[0] / 2.0 + 0.5;
    double crpix2 = (double)naxes[1] / 2.0 + 0.5;

    fits_update_key(dest_fptr, TDOUBLE, "CRPIX1", &crpix1, "X reference pixel", status);
    fits_update_key(dest_fptr, TDOUBLE, "CRPIX2", &crpix2, "Y reference pixel", status);

    double crval1 = 10.6847;
    double crval2 = 41.2687;

    fits_update_key(dest_fptr, TDOUBLE, "CRVAL1", &crval1, "RA of reference pixel (deg)", status);
    fits_update_key(dest_fptr, TDOUBLE, "CRVAL2", &crval2, "DEC of reference pixel (deg)", status);

    double cdelt1 = -0.00006944;
    double cdelt2 =  0.00006944;

    fits_update_key(dest_fptr, TDOUBLE, "CDELT1", &cdelt1, "X pixel scale (deg/pixel)", status);
    fits_update_key(dest_fptr, TDOUBLE, "CDELT2", &cdelt2, "Y pixel scale (deg/pixel)", status);
    fits_write_img(dest_fptr, TFLOAT, 1, total_pixels, buffer, status);
    fits_flush_file(dest_fptr, status);
    free(buffer);

    if (*status) {
      fprintf(stderr, "WCS Keyword generation execution failed with code: %d\n", *status);
    } else {
      printf("Success! Created a clean image extension and successfully injected WCS_IMAGE keywords!\n");
    }
    return *status;
}

int main() {
  fitsfile *fptr;
  int status = 0;
  float *pixels = (float *)malloc(WIDTH * HEIGHT * sizeof(float));

  if (pixels == NULL) {
    fprintf(stderr, "Error: Memory allocation failed.\n");
    return 1;
  }

  for (int i = 0; i < WIDTH * HEIGHT; i++) pixels[i] = (float)i;

  if (fits_create_file(&fptr, "!wcs_output.fits", &status)) {
    fits_report_error(stderr, status);
    free(pixels);
    return status;
  }

  // HDU 1: PRIMARY HDU
  int bitpix = BYTE_IMG;
  int naxis = 0;
  long naxes[1] = {0};

  if (fits_create_img(fptr, bitpix, naxis, naxes, &status)) {
    fits_report_error(stderr, status);
    fits_close_file(fptr, &status);
    free(pixels);
    return status;
  }

  fits_update_key(fptr, TSTRING, "ORIGIN", "MyObservatory", "Global source", &status);

  // HDU 2: SCI_DATA IMAGE EXTENSION
  int ext_bitpix = FLOAT_IMG;
  int ext_naxis = 2;
  long ext_naxes[] = {WIDTH, HEIGHT};

  fits_create_img(fptr, ext_bitpix, ext_naxis, ext_naxes, &status);
  fits_update_key(fptr, TSTRING, "EXTNAME", "SCI_DATA", "Extension Name", &status);
  fits_update_key(fptr, TSTRING, "OBJECT", "M31", "Target object", &status);

  LONGLONG firstelem = 1;
  LONGLONG nelements = (LONGLONG)WIDTH * HEIGHT;

  fits_write_img(fptr, TFLOAT, firstelem, nelements, pixels, &status);

  // HDU 3: STARS_CATALOG TABLE
  int ttype = BINARY_TBL;
  long nrows = 3;
  int tfields = 2;
  char *ttype_names[] = {"STAR_ID", "MAGNITUDE"};
  char *tform[] = {"10A", "1D"};
  char *tunit[] = {"", "mag"};

  fits_create_tbl(fptr, ttype, nrows, tfields, ttype_names, tform, tunit, "STARS_CATALOG", &status);

  char *star_ids[] = {"Alpha", "Beta", "Gamma"};
  double magnitudes[] = {4.25, 1.12, 2.84};

  fits_write_col(fptr, TSTRING, 1, 1, 1, nrows, star_ids, &status);
  fits_write_col(fptr, TDOUBLE, 2, 1, 1, nrows, magnitudes, &status);

  // ====================================================================
  // RUN SEQUENTIAL PIPELINES WITH BUFFER RESET SEGMENT GUARDS
  // ====================================================================
  status = 0;
  append_dynamic_table(fptr, &status);
  status = 0;
  append_variable_stars_table(fptr, &status);
  status = 0;
  fits_flush_file(fptr, &status);
  status = 0;
  copy_hdu_between_files(fptr, &status);
  fits_close_file(fptr, &status);
  free(pixels);
  printf("Execution completely finished. File written to wcs_output.fits\n");
  return 0;
}
