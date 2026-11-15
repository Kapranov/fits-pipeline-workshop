#include "fitsio.h"

int main() {
  /* required by every program that uses CFITSIO  */
  fitsfile *fptr = NULL;
  int status = 0;
  int ii, jj;

  /* Image Data Setup (File 1) */
  long fpixel = 1, naxis = 2, nelements, exposure;
  long naxes[2] = { 300, 200 };
  short array[200][300];

  /* Table Data Setup (File 2) */
  unsigned short uarray[100];
  unsigned int  varray[100];
  int colnum, tfields;
  long nrows, firstrow, firstelem;
  char extname[] = "Test_table";
  char *ttype[] = { "Col_1", "Col_2", "Col_3", "Col_4" };
  char *tform[] = { "1U",      "1V",    "1W",    "1S"};
  char *tunit[] = { " ",        " ",    " ",    " " };

  /* Initialize test array data for the table */
  for (ii = 0; ii < 100; ii++) {
    uarray[ii] = (unsigned short)ii;
    varray[ii] = (unsigned int)(ii * 10);
  }

  /* ========================================== */
  /* STEP 1: Create the new combined FITS file  */
  /* ========================================== */
  if (fits_create_file(&fptr, "!demo_combined.fits", &status)) {
    fits_report_error(stderr, status);
    return status;
  }

  /* ========================================== */
  /* STEP 2: Create and Write the Image (Primary HDU) */
  /* ========================================== */
  /* Create the primary array image */
  fits_create_img(fptr, SHORT_IMG, naxis, naxes, &status);

  /* Write exposure keyword */
  exposure = 1500;
  fits_update_key(fptr, TLONG, "EXPOSURE", &exposure, "Total Exposure Time", &status);

  /* Initialize the values in the image with a linear ramp function */
  for (jj = 0; jj < naxes[1]; jj++) {
    for (ii = 0; ii < naxes[0]; ii++) {
      array[jj][ii] = ii + jj;
    }
  }

  /* Number of pixels to write and execution */
  nelements = naxes[0] * naxes[1];
  fits_write_img(fptr, TSHORT, fpixel, nelements, array[0], &status);

  /* ========================================== */
  /* STEP 3: Append the Binary Table Extension */
  /* ========================================== */
  /* tfields is updated to 4 to match the 4 array columns provided */
  nrows   = 1;
  tfields = 4;

  /* This automatically creates a new HDU extension directly after the image */
  fits_create_tbl(fptr, BINARY_TBL, nrows, tfields, ttype, tform, tunit, extname, &status);

  /* Write the unsigned shorts to the 1st column */
  colnum    = 1;
  firstrow  = 1;
  firstelem = 1;
  nelements = 100;
  fits_write_col(fptr, TUSHORT, colnum, firstrow, firstelem, nelements, uarray, &status);

  /* Write the unsigned longs to the 2nd column */
  colnum    = 2;
  fits_write_col(fptr, TUINT, colnum, firstrow, firstelem, nelements, varray, &status);

  /* ========================================== */
  /* STEP 4: Close the file and handle errors   */
  /* ========================================== */
  fits_close_file(fptr, &status);

  if (status) {
    fits_report_error(stderr, status);
  }

  return status;
}
