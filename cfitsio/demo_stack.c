#include "fitsio.h"
#include <stdlib.h>
#include <time.h>

int main() {
  fitsfile *fptr = NULL;
  int status = 0;
  int ii, jj;

  /* Shared Image Parameters (300 width x 500 height) */
  long fpixel = 1, naxis = 2, nelements;
  long naxes[2] = { 300, 500 };

  /* Use a dynamic or flat array to prevent stack overflow with large dimensions */
  /* 300 * 500 = 150,000 elements */
  short *img_array = (short *)malloc(naxes[0] * naxes[1] * sizeof(short));
  if (img_array == NULL) {
    fprintf(stderr, "Memory allocation failed for image array\n");
    return 1;
  }

  /* Table Data Setup */
  unsigned short uarray[100];
  unsigned int  varray[100];
  int colnum, tfields;
  long nrows, firstrow, firstelem;
  char extname[] = "Test_table";
  char *ttype[] = { "Col_1", "Col_2", "Col_3", "Col_4" };
  char *tform[] = { "1U",      "1V",    "1W",    "1S"};
  char *tunit[] = { " ",        " ",    " ",    " " };

  /* Seed the random number generator for the simulated sky */
  srand((unsigned int)time(NULL));

  /* Initialize test array data for the table */
  for (ii = 0; ii < 100; ii++) {
    uarray[ii] = (unsigned short)ii;
    varray[ii] = (unsigned int)(ii * 10);
  }

  /* ========================================== */
  /* STEP 1: Create the new FITS file           */
  /* ========================================== */
  if (fits_create_file(&fptr, "!demo_stack.fits", &status)) {
    fits_report_error(stderr, status);
    free(img_array);
    return status;
  }

  /* Create an empty primary HDU array (standard FITS convention) */
  fits_create_img(fptr, SHORT_IMG, 0, naxes, &status);

  /* ========================================== */
  /* STEP 2: Extension 1 - Gradient Image       */
  /* ========================================== */
  /* Create the first image extension */
  fits_create_img(fptr, SHORT_IMG, naxis, naxes, &status);
  fits_update_key(fptr, TSTRING, "EXTNAME", "GRADIENT", "Extension name", &status);

  /* Generate a linear gradient pattern */
  for (jj = 0; jj < naxes[1]; jj++) {
    for (ii = 0; ii < naxes[0]; ii++) {
      img_array[jj * naxes[0] + ii] = ii + jj;
    }
  }
  nelements = naxes[0] * naxes[1];
  fits_write_img(fptr, TSHORT, fpixel, nelements, img_array, &status);

  /* ========================================== */
  /* STEP 3: Extension 2 - Binary Table         */
  /* ========================================== */
  nrows   = 1;
  tfields = 4;
  fits_create_tbl(fptr, BINARY_TBL, nrows, tfields, ttype, tform, tunit, extname, &status);

  colnum    = 1;
  firstrow  = 1;
  firstelem = 1;
  nelements = 100;
  fits_write_col(fptr, TUSHORT, colnum, firstrow, firstelem, nelements, uarray, &status);

  colnum    = 2;
  fits_write_col(fptr, TUINT, colnum, firstrow, firstelem, nelements, varray, &status);

  /* ========================================== */
  /* STEP 4: Extension 3 - Simulated Sky Image  */
  /* ========================================== */
  /* Create the second image extension */
  fits_create_img(fptr, SHORT_IMG, naxis, naxes, &status);
  fits_update_key(fptr, TSTRING, "EXTNAME", "SIMULATED_SKY", "Extension name", &status);

  /* Generate a background noise + random stars simulation */
  for (jj = 0; jj < naxes[1]; jj++) {
    for (ii = 0; ii < naxes[0]; ii++) {
      int idx = jj * naxes[0] + ii;

      /* Base background sky noise (values between 10 and 20) */
      img_array[idx] = 10 + (rand() % 11);

      /* Inject random bright stars (0.5% chance per pixel) */
      if ((rand() % 200) == 0) {
        /* Star intensity between 500 and 2500 */
        img_array[idx] += 500 + (rand() % 2001);
      }
    }
  }

  nelements = naxes[0] * naxes[1];
  fits_write_img(fptr, TSHORT, fpixel, nelements, img_array, &status);

  /* ========================================== */
  /* STEP 5: Close the file and clean up        */
  /* ========================================== */
  fits_close_file(fptr, &status);

  if (status) {
    fits_report_error(stderr, status);
  }

  free(img_array);
  return status;
}
