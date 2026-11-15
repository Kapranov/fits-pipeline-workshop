#include "fitsio.h"

int main() {
  /* required by every program that uses CFITSIO  */
  fitsfile *fptr = NULL;
  int status = 0;
  int ii, jj, ext;

  /* Image Data Setup (For Primary and Extensions) */
  long fpixel = 1, naxis = 2, nelements, exposure;
  long naxes[2] = { 300, 200 };
  short array[200][300];

  /* Table Data Setup */
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
  if (fits_create_file(&fptr, "!demo_multi.fits", &status)) {
    fits_report_error(stderr, status);
    return status;
  }

  /* ========================================== */
  /* STEP 2: Create and Write the Primary Image */
  /* ========================================== */
  fits_create_img(fptr, SHORT_IMG, naxis, naxes, &status);

  exposure = 1500;
  fits_update_key(fptr, TLONG, "EXPOSURE", &exposure, "Total Exposure Time", &status);

  /* Linear ramp function for Primary Image */
  for (jj = 0; jj < naxes[1]; jj++) {
    for (ii = 0; ii < naxes[0]; ii++) {
      array[jj][ii] = ii + jj;
    }
  }
  nelements = naxes[0] * naxes[1];
  fits_write_img(fptr, TSHORT, fpixel, nelements, array[0], &status);

  /* ========================================== */
  /* STEP 3: Append the Binary Table Extension */
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
  /* STEP 4: Append Three Image Extensions     */
  /* ========================================== */
  for (ext = 1; ext <= 3; ext++) {
    /* Create a new image extension */
    fits_create_img(fptr, SHORT_IMG, naxis, naxes, &status);

    /* (Optional) Name each image extension so you can find them easily later */
    char extimg_name[20];
    sprintf(extimg_name, "IMAGE_EXT_%d", ext);
    fits_update_key(fptr, TSTRING, "EXTNAME", extimg_name, "Extension name", &status);

    /* Generate different pixel data for each extension layer */
    for (jj = 0; jj < naxes[1]; jj++) {
      for (ii = 0; ii < naxes[0]; ii++) {
        /* Multiplying by ext gives each image different pixel values */
        array[jj][ii] = (ii + jj) * ext;
      }
    }

    /* Write the pixel array to the current image extension */
    nelements = naxes[0] * naxes[1];
    fits_write_img(fptr, TSHORT, fpixel, nelements, array[0], &status);
  }

  /* ========================================== */
  /* STEP 5: Close the file and handle errors   */
  /* ========================================== */
  fits_close_file(fptr, &status);

  if (status) {
    fits_report_error(stderr, status);
  }

  return status;
}
