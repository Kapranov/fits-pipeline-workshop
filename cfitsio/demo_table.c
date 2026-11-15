#include "fitsio.h"

int main() {
  /* required by every program that uses CFITSIO  */
  fitsfile *fptr = NULL;

  unsigned short uarray[100];
  unsigned int  varray[100];

  int colnum, tfields, status;
  long nrows, firstrow, firstelem, nelements, pcount;

  /* extension name */
  char extname[] = "Test_table";

  /* define the name, data type, and physical units for 4 columns */
  char *ttype[] = { "Col_1", "Col_2", "Col_3", "Col_4" };
  char *tform[] = { "1U",      "1V",    "1W",    "1S"};
  char *tunit[] = { " ",        " ",    " ",    " " };

  /* 1. Initialize status and create the new file first */
  status  = 0;
  if (fits_create_file(&fptr, "!demo_table.fits", &status)) {
    fits_report_error(stderr, status);
    return status;
  }

  /* 2. Setup table parameters write the header keywords */
  nrows   = 1;
  tfields = 3;
  pcount  = 0;

  /* 3. Create the binary table extension inside the created file */
  fits_create_tbl(fptr, BINARY_TBL, nrows, tfields, ttype, tform, tunit, extname, &status);

  /* write the unsigned shorts to the 1st column */
  colnum    = 1;
  firstrow  = 1;
  firstelem = 1;
  nelements = 100;
  fits_write_col(fptr, TUSHORT, colnum, firstrow, firstelem, nelements, uarray, &status);

  /* now write the unsigned longs to the 2nd column */
  colnum    = 2;
  fits_write_col(fptr, TUINT, colnum, firstrow, firstelem, nelements, varray, &status);

  /* close the file */
  fits_close_file(fptr, &status);

  /* print out any error messages */
  if (status) {
      fits_report_error(stderr, status);
  }
  return status;
}
