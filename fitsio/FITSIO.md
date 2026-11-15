# FITSIO for create FITS file

Basic Structure of a Fortran FITSIO Program
Every FITSIO operation follows a structured routine:

The CFITSIO/FITSIO library  is the  standard,  high-performance  subroutine
package used in astronomy to read and write Flexible Image Transport System
(FITS) files using  Fortran.  Modern  CFITSIO  provides  a  native  Fortran
90/95/2003+  interface  (as  well as legacy Fortran 77 subroutines prefixed
with FT) that insulates  developers from low-level FITS formatting rules.

1. Initialize an integer status variable to 0.
2. Open or Create a FITS file.
3. Write/Read headers (keywords) or data arrays (Images, ASCII/Binary tables).
4. Close the file and check for errors.

**Creating and Writing a FITS Image**

Below is a standard Fortran example using the modern CFITSIO wrapper to create
a new FITS file, write a header keyword, and save a 2D primary image array.

```
program write_image
    implicit none

    integer:: status, unit, bitpix, naxis
    integer, dimension(2):: naxes
    character(len = 80):: filename
    real, dimension(100, 100):: image_data
    integer:: i, j

    ! 1. Initialize variables
    status = 0
    filename = 'output_image.fits'
    bitpix = -32   ! Data type (32-bit floating point)
    naxis = 2            ! 2D Image
    naxes(1) = 100       ! Width
    naxes(2) = 100       ! Height

    ! Generate dummy pixel data (gradient)
    do j = 1, 100
        do i = 1, 100
            image_data(i, j) = real(i+j)
        end do
    end do

    ! 2. Delete the file if it already exists (convenience syntax: prefix with !)
    call ftinit(unit, '!' // trim(filename), 2880, status)

    ! 3. Write the required primary header keywords
    call ftphpr(unit, .true., bitpix, naxis, naxes, 0, 1, .true., status)

    ! 4. Write an additional user header keyword (Keyword, Value, Comment)
    call ftpkyj(unit, 'EXPTIME', 120, 'Exposure time in seconds', status)
    call ftpkys(unit, 'TELESCOP', 'Hubble', 'Name of the telescope', status)
    call ftpkyj(unit, 'EXPOSURE', 1500, 'Total Exposure Duration', status)

    ! 5. Write the 2D data array to the primary HDU
    ! ftppre arguments: unit, group, first_element, total_pixels, array, status
    call ftppre(unit, 1, 1, 100*100, image_data, status)

    ! 6. Close the FITS file
    call ftclos(unit, status)

    ! 7. Error reporting
    if (status .ne. 0) then
        print *, 'An error occurred during FITS operations. Status code: ', status
        call ftrprt(status)  ! Prints out the error message stack
    else
        print *, 'FITS file created successfully: ', trim(filename)
    end if

end program write_image
```

output:

```
bash> gfortran write_image.f90 -o write_image -lcfitsio
bash> ./write_image
```

Reading a FITS Image (`read_image.f90`), to extract data and headers
from an existing FITS file, mirror the logic using ftopen and ftgpve.

```
#include <stdio.h>
#include "fitsio.h"

// Dynamically fetches the image dimensions from a specific HDU
void c_get_image_dims(const char* filename, int hdu_num, int* nx, int* ny, int* status) {
    fitsfile *fptr;
    int bitpix, naxis;
    long naxes[2] = {0, 0};
    int hdutype;

    if (fits_open_file(&fptr, filename, READONLY, status)) return;

    // Move to the requested HDU (1-indexed: 1 = Primary, 2 = First Extension)
    if (fits_movabs_hdu(fptr, hdu_num, &hdutype, status)) {
        fits_close_file(fptr, status);
        return;
    }

    // Verify it is an image extension before parsing dimensions
    if (hdutype != IMAGE_HDU) {
        *status = NOT_IMAGE; // CFITSIO error code for non-image HDUs
        fits_close_file(fptr, status);
        return;
    }

    if (!fits_get_img_param(fptr, 2, &bitpix, &naxis, naxes, status)) {
        *nx = (int)naxes[0];
        *ny = (int)naxes[1];
    }
    fits_close_file(fptr, status);
}

// Reads the raw 2D floating-point matrix from a specific HDU
void c_read_image_float(const char* filename, int hdu_num, float* data, int nx, int ny, int* status) {
    fitsfile *fptr;
    long fpixel[] = {1, 1};
    int anynul, hdutype;

    if (fits_open_file(&fptr, filename, READONLY, status)) return;

    // Move to the requested HDU
    if (fits_movabs_hdu(fptr, hdu_num, &hdutype, status)) {
        fits_close_file(fptr, status);
        return;
    }

    if (fits_read_pix(fptr, TFLOAT, fpixel, (long)nx * (long)ny, NULL, data, &anynul, status)) {
        fits_close_file(fptr, status);
        return;
    }

    fits_close_file(fptr, status);
}

// Reads an Integer keyword value from a specific HDU
void c_read_int_key(const char* filename, int hdu_num, const char* keyname, int* value, int* status) {
    fitsfile *fptr;
    int intval, hdutype;
    if (fits_open_file(&fptr, filename, READONLY, status)) return;

    // Move to the requested HDU
    if (fits_movabs_hdu(fptr, hdu_num, &hdutype, status)) {
        fits_close_file(fptr, status);
        return;
    }

    fits_read_key(fptr, TINT, keyname, &intval, NULL, status);
    *value = intval;
    fits_close_file(fptr, status);
}
```

```
program read_demo
    use iso_c_binding
    implicit none

    interface
        subroutine c_get_image_dims(filename, hdu_num, nx, ny, status) bind(c, name="c_get_image_dims")
            import:: c_char, c_int
            character(kind = c_char), intent(in):: filename(*)
            integer(c_int), value:: hdu_num
            integer(c_int), intent(out):: nx, ny
            integer(c_int):: status
        end subroutine c_get_image_dims

        subroutine c_read_image_float(filename, hdu_num, data, nx, ny, status) bind(c, name="c_read_image_float")
            import:: c_char, c_float, c_int
            character(kind = c_char), intent(in):: filename(*)
            integer(c_int), value:: hdu_num
            real(c_float), intent(out):: data(*)
            integer(c_int), value:: nx, ny
            integer(c_int):: status
        end subroutine c_read_image_float

        subroutine c_read_int_key(filename, hdu_num, keyname, value, status) bind(c, name="c_read_int_key")
            import:: c_char, c_int
            character(kind = c_char), intent(in):: filename(*)
            integer(c_int), value:: hdu_num
            character(kind = c_char), intent(in):: keyname(*)
            integer(c_int), intent(out):: value
            integer(c_int):: status
        end subroutine c_read_int_key
    end interface

    character(kind = c_char, len=*), parameter:: FITS_FILE = "output_image.fits" // c_null_char
    integer(c_int):: nx, ny
    integer(c_int):: status, exposure_val
    integer(c_int):: hdu_target
    real(c_float), allocatable, dimension(:,:):: array

    status = 0_c_int
    exposure_val = 0_c_int

    ! TARGET SELECTION: 1 = Primary HDU, 2 = Extension 1, etc.
    hdu_target = 1_c_int

    print *, "Analyzing FITS HDU target:", hdu_target

    ! 1. Automatically retrieve size dimensions directly from target HDU
    call c_get_image_dims(FITS_FILE, hdu_target, nx, ny, status)

    if (status .ne. 0_c_int) then
        print *, "CRITICAL: HDU extension non-existent or contains no image metadata  ! Status:", status
        stop
    end if

    print *, "Detected layout dimensions:", nx, "x", ny

    ! 2. Dynamically allocate matrix space matching target HDU layout
    allocate(array(nx, ny))

    ! 3. Pull integer metadata keywords safely from target HDU header cards
    call c_read_int_key(FITS_FILE, hdu_target, "EXPOSURE" // c_null_char, exposure_val, status)
    if (status .eq. 0_c_int) then
        print *, "Parsed EXPOSURE keyword value:", exposure_val
    else
        print *, "Warning: Error reading EXPOSURE key in this HDU. Code:", status
        status = 0_c_int
    end if

    ! 4. Parse matrix pixels from target HDU
    call c_read_image_float(FITS_FILE, hdu_target, array, nx, ny, status)

    if (status .eq. 0_c_int) then
        print *, "SUCCESS: Float pixel array parsed from requested HDU completely!"
        print *, "--------------------------------------------------------"
        print *, "First element in target HDU: ", array(1, 1)
        print *, "Last element in target HDU:  ", array(nx, ny)
    else
        print *, "CRITICAL ERROR: HDU data matrix reading failed. Status code:", status
    end if

    deallocate(array)

end program read_demo
```

output:

```
bash> gcc -c bridge.c -o bridge.o -lcfitsio
bash> gfortran read_image.f90 bridge.o -o read_image -lcfitsio
bash> ./read_image

 Analyzing FITS HDU target:           1
 Detected layout dimensions:         100 x         100
 Parsed EXPOSURE keyword value:        1500
 SUCCESS: Float pixel array parsed from requested HDU completely!
 --------------------------------------------------------
 First element in target HDU:    2.00000000
 Last element in target HDU:     200.000000
```

**Create Demo Image(`demo_image.f90`)**

Create FITS file where HDU only simaple image of gradient.

The reason you are hitting compilation errors with that specific command is a
structural mismatch: **The classic CFITSIO  Fortran wrappers do not work with
use fits2nd or modern F90 modules by default**.  They  are written as classic
external subroutines that rely on an integer logical  unit number (similar to
native Fortran I/O), rather than the internal pointer fptr used in C.

To make your code compile and function  flawlessly  using  standard compilers
like gfortran, we must use the exact signatures that the `libcfitsio`  binary
expects.

```
program demo_image
    implicit none

    ! CFITSIO Fortran uses standard logical units instead of C pointers
    integer:: unit, status
    integer:: ii, jj

    ! Dimensions and parameters required by CFITSIO Fortran routines
    integer:: bitpix, naxis
    integer:: naxes(2)
    integer:: blocksize

    ! Pixel coordinates, elements, and keyword parameters
    integer:: fpixel, nelements, exposure

    ! Image array: 16-bit signed integer pixels
    integer*2:: array(300, 200)

    ! Initialize parameters
    status = 0
    fpixel = 1
    naxis = 2
    naxes(1) = 300
    naxes(2) = 200
    blocksize = 1

    ! Get an unused Logical Unit Number to use for the FITS file
    call ftgiou(unit, status)

    ! Create new empty FITS file (Deletes existing file if prefixed with '!')
    call ftinit(unit, '!demo_image.fits', blocksize, status)

    ! Define the image properties (16-bit short integer pixels)
    bitpix = 16  ! 16 maps to SHORT_IMG

    ! Write the primary header keywords defining the image matrix structure
    call ftphpr(unit, .true., bitpix, naxis, naxes, 0, 1, .true., status)

    ! Write a specific keyword to the header
    exposure = 1500
    call ftpkyl(unit, 'EXPOSURE', exposure, 'Total Exposure Time', status)

    ! Initialize the values in the image matrix with a linear ramp function
    ! Fortran utilizes Column-Major alignment (Columns vary fastest in memory)
    do jj = 1, naxes(2)
        do ii = 1, naxes(1)
            array(ii, jj) = (ii-1) + (jj-1)
        end do
    end do

    ! Total count of pixels to transfer
    nelements = naxes(1) * naxes(2)

    ! Write the array of integers directly to the primary image matrix
    call ftppri(unit, 1, fpixel, nelements, array, status)

    ! Close the file allocation handle safely
    call ftclos(unit, status)

    ! Free the logical unit back to the system pool
    call ftfiou(unit, status)

    ! Check and report any runtime error messages
    if (status .ne. 0) then
        call ftrprt('stderr', status)
    end if

end program demo_image
```

output:

Now you can compile it directly with `gfortran`. You do not need any `-I`
paths because we dropped the uninstalled module requirement:

```
bash> gfortran demo_image.f90 -o demo_image -lcfitsio
bash> ./demo_image

bash> fitsverify -e demo_image.fits

              fitsverify 4.22 (CFITSIO V4.7.0)
              --------------------------------


File: demo_image.fits

1 Header-Data Units in this file.

=================== HDU 1: Primary Array ===================

 10 header keywords

 16-bit integer pixels,  2 axes (300 x 200),

++++++++++++++++++++++ Error Summary  ++++++++++++++++++++++

 HDU#  Name (version)       Type             Warnings  Errors
 1                          Primary Array    0         0

**** Verification found 0 warning(s) and 0 error(s). ****
```

1. **Removed use fits2nd**: Replaced it with native, implicit external routine
   mapping so you do not have to generate a custom `.mod`  configuration file.
2. **ftgiou & ftfiou**: Added these routines to safely request and release  an
   internal Fortran runtime file handle system link (`unit`).
3. **ftphpr**: Rewrote the allocation header to explicitly initiate the primary
   image  matrix  parameters  safely (`simple`,  `bitpix`,  `naxes`)   directly
   matching the behavior in `demo_image.c`.

**Create Demo Table(`demo_table.f90`)**

Create FITS file where HDU only table.

```
program demo_table
    implicit none

    ! CFITSIO logical unit number and status flags
    integer:: unit, status
    integer:: colnum, tfields
    integer:: nrows, firstrow, firstelem, nelements, pcount

    ! Header properties
    character(len = 20):: extname

    ! Table configurations-lengths must match exactly
    character(len = 16):: ttype(3)
    character(len = 16):: tform(3)
    character(len = 16):: tunit(3)

    ! Array variable tracking the primary axis dimension size
    integer:: naxes(1)

    ! Mock data arrays (matching the unsigned arrays from the C code)
    integer*2:: uarray(100)
    integer*4:: varray(100)

    ! Initialize data arrays with mock data
    uarray = 5
    varray = 50000

    ! Table configuration parameters
    extname = 'Test_table'
    ttype(1) = 'Col_1'; tform(1) = '1U'; tunit(1) = ' '
    ttype(2) = 'Col_2'; tform(2) = '1V'; tunit(2) = ' '
    ttype(3) = 'Col_3'; tform(3) = '1W'; tunit(3) = ' '

    status = 0
    nrows = 1
    tfields = 3
    pcount = 0
    naxes(1) = 0

    ! Get an unused Logical Unit Number
    call ftgiou(unit, status)

    ! 1. Initialize status and create the new file first
    call ftinit(unit, '!demo_table.fits', 1, status)
    if (status .ne. 0) then
        call ftrprt('stderr', status)
        stop
    end if

    ! Write the primary header matrix keyword configurations
    call ftphpr(unit, .true., 8, 0, naxes, 0, 1, .true., status)

    ! 3. Create the binary table extension inside the created file
    call ftibin(unit, nrows, tfields, ttype, tform, tunit, extname, pcount, status)

    ! Write the 16-bit integers to the 1st column
    colnum    = 1
    firstrow  = 1
    firstelem = 1
    nelements = 100
    ! FIX: 'ftpcli' is the true standard routine for 16-bit integer array chunks
    call ftpcli(unit, colnum, firstrow, firstelem, nelements, uarray, status)

    ! Now write the 32-bit integers to the 2nd column
    colnum    = 2
    ! FIX: 'ftpclj' is the true standard routine for 32-bit integer array chunks
    call ftpclj(unit, colnum, firstrow, firstelem, nelements, varray, status)

    ! Close the file allocation handle safely
    call ftclos(unit, status)

    ! Free the logical unit back to the system pool
    call ftfiou(unit, status)

    ! Print out any error messages
    if (status .ne. 0) then
        call ftrprt('stderr', status)
    end if

end program demo_table

```

output:

```
bash> gfortran demo_table.f90 -o demo_table -lcfitsio

bash> fitsverify -e demo_table.fits

              fitsverify 4.22 (CFITSIO V4.7.0)
              --------------------------------


File: demo_table.fits

2 Header-Data Units in this file.

=================== HDU 1: Primary Array ===================

 7 header keywords

 Null data array; NAXIS = 0

=================== HDU 2: BINARY Table ====================

 22 header keywords

 Test_table  (3 columns x 100 rows)

 Col# Name (Units)       Format
   1 Col_1                1I
   2 Col_2                1J
   3 Col_3                1K

++++++++++++++++++++++ Error Summary  ++++++++++++++++++++++

 HDU#  Name (version)       Type             Warnings  Errors
 1                          Primary Array    0         0
 2     Test_table           Binary Table     0         0

**** Verification found 0 warning(s) and 0 error(s). ****
```

**Create Demo Image/Table(`demo_combined.f90`)**

```
program demo_combined
    implicit none

    ! CFITSIO logical unit number and status flags
    integer:: unit, status
    integer:: ii, jj

    ! ==========================================
    ! Image Data Setup (Primary HDU)
    ! ==========================================
    integer:: naxis
    integer:: naxes(2)
    integer:: blocksize
    integer*8:: fpixel, nelements, exposure
    integer*2:: img_array(300, 200)

    ! ==========================================
    ! Table Data Setup (Extension HDU)
    ! ==========================================
    integer:: colnum, tfields
    integer:: nrows, firstrow, firstelem, tbl_nelements, pcount
    character(len = 20):: extname

    ! Table configurations-ALL array string lengths must match exactly
    character(len = 16):: ttype(4)
    character(len = 16):: tform(4)
    character(len = 16):: tunit(4)

    ! Mock data arrays (matching the unsigned arrays from the C code)
    integer*2:: uarray(100)
    integer*4:: varray(100)

    ! ==========================================
    ! Parameter & Data Initialization
    ! ==========================================
    status = 0
    fpixel = 1
    naxis = 2
    naxes(1) = 300
    naxes(2) = 200
    blocksize = 1

    ! Initialize test array data for the table
    do ii = 1, 100
        uarray(ii) = int(ii-1, 2)
        varray(ii) = int((ii-1) * 10, 4)
    end do

    ! Initialize table structural metadata
    extname = 'Test_table'
    ttype(1) = 'Col_1'; tform(1) = '1U'; tunit(1) = ' '
    ttype(2) = 'Col_2'; tform(2) = '1V'; tunit(2) = ' '
    ttype(3) = 'Col_3'; tform(3) = '1W'; tunit(3) = ' '
    ttype(4) = 'Col_4'; tform(4) = '1S'; tunit(4) = ' '

    nrows = 1
    tfields = 4
    pcount = 0

    ! Get an unused Logical Unit Number
    call ftgiou(unit, status)

    ! ==========================================
    ! STEP 1: Create the new combined FITS file
    ! ==========================================
    call ftinit(unit, '!demo_combined.fits', blocksize, status)
    if (status .ne. 0) then
        call ftrprt('stderr', status)
        stop
    end if

    ! ==========================================
    ! STEP 2: Create and Write the Image (Primary HDU)
    ! ==========================================
    ! Define image header tracking properties (16-bit short integer pixels)
    call ftphpr(unit, .true., 16, naxis, naxes, 0, 1, .true., status)

    ! Write the exposure keyword to the header
    exposure = 1500
    call ftpkyl(unit, 'EXPOSURE', exposure, 'Total Exposure Time', status)

    ! Initialize the values in the image matrix with a linear ramp function
    ! Fortran utilizes Column-Major alignment (Columns vary fastest in memory)
    do jj = 1, naxes(2)
        do ii = 1, naxes(1)
            img_array(ii, jj) = int((ii-1) + (jj-1), 2)
        end do
    end do

    ! Number of pixels to write
    nelements = int(naxes(1), 8) * int(naxes(2), 8)

    ! Write the array of integers directly to the primary image matrix
    call ftppri(unit, 1, fpixel, nelements, img_array, status)

    ! ==========================================
    ! STEP 3: Append the Binary Table Extension
    ! ==========================================
    ! ftibin automatically appends a new HDU extension directly after the image
    call ftibin(unit, nrows, tfields, ttype, tform, tunit, extname, pcount, status)

    ! Write the unsigned shorts to the 1st column using standard 16-bit writer
    colnum    = 1
    firstrow  = 1
    firstelem = 1
    tbl_nelements = 100
    call ftpcli(unit, colnum, firstrow, firstelem, tbl_nelements, uarray, status)

    ! Write the unsigned longs to the 2nd column using standard 32-bit writer
    colnum    = 2
    call ftpclj(unit, colnum, firstrow, firstelem, tbl_nelements, varray, status)

    ! ==========================================
    ! STEP 4: Close the file and handle errors
    ! ==========================================
    call ftclos(unit, status)
    call ftfiou(unit, status)

    ! Print out any error messages
    if (status .ne. 0) then
        call ftrprt('stderr', status)
    end if

end program demo_combined
```

output:

```
bash> gfortran demo_combined.f90 -o demo_combined -lcfitsio

bash> fitsverify -e demo_combined.fits

              fitsverify 4.22 (CFITSIO V4.7.0)
              --------------------------------


File: demo_combined.fits

2 Header-Data Units in this file.

=================== HDU 1: Primary Array ===================

 10 header keywords

 16-bit integer pixels,  2 axes (300 x 200),

=================== HDU 2: BINARY Table ====================

 26 header keywords

 Test_table  (4 columns x 100 rows)

 Col# Name (Units)       Format
   1 Col_1                1I
   2 Col_2                1J
   3 Col_3                1K
   4 Col_4                1B

++++++++++++++++++++++ Error Summary  ++++++++++++++++++++++

 HDU#  Name (version)       Type             Warnings  Errors
 1                          Primary Array    0         0
 2     Test_table           Binary Table     0         0

**** Verification found 0 warning(s) and 0 error(s). ****
```

**Create Demo Image/Table/Image/Image/Image(`demo_multi.f90`)**

```
program demo_multi
    implicit none

    ! CFITSIO logical unit number and status flags
    integer:: unit, status
    integer:: ii, jj, ext

    ! ==========================================
    ! Image Data Setup (Primary & Extensions)
    ! ==========================================
    integer:: naxis
    integer:: naxes(2)
    integer:: blocksize
    integer*8:: fpixel, nelements, exposure
    integer*2:: img_array(300, 200)
    character(len = 20):: extimg_name

    ! ==========================================
    ! Table Data Setup
    ! ==========================================
    integer:: colnum, tfields
    integer:: nrows, firstrow, firstelem, tbl_nelements, pcount
    character(len = 20):: extname

    ! Table configurations-ALL array string lengths must match exactly
    character(len = 16):: ttype(4)
    character(len = 16):: tform(4)
    character(len = 16):: tunit(4)

    ! Mock data arrays (matching the unsigned arrays from the C code)
    integer*2:: uarray(100)
    integer*4:: varray(100)

    ! ==========================================
    ! Parameter & Data Initialization
    ! ==========================================
    status = 0
    fpixel = 1
    naxis = 2
    naxes(1) = 300
    naxes(2) = 200
    blocksize = 1

    ! Initialize test array data for the table
    do ii = 1, 100
        uarray(ii) = int(ii-1, 2)
        varray(ii) = int((ii-1) * 10, 4)
    end do

    ! Initialize table structural metadata
    extname = 'Test_table'
    ttype(1) = 'Col_1'; tform(1) = '1U'; tunit(1) = ' '
    ttype(2) = 'Col_2'; tform(2) = '1V'; tunit(2) = ' '
    ttype(3) = 'Col_3'; tform(3) = '1W'; tunit(3) = ' '
    ttype(4) = 'Col_4'; tform(4) = '1S'; tunit(4) = ' '

    nrows = 1
    tfields = 4
    pcount = 0

    ! Get an unused Logical Unit Number
    call ftgiou(unit, status)

    ! ==========================================
    ! STEP 1: Create the new combined FITS file
    ! ==========================================
    call ftinit(unit, '!demo_multi.fits', blocksize, status)
    if (status .ne. 0) then
        call ftrprt('stderr', status)
        stop
    end if

    ! ==========================================
    ! STEP 2: Create and Write the Primary Image
    ! ==========================================
    call ftphpr(unit, .true., 16, naxis, naxes, 0, 1, .true., status)

    exposure = 1500
    call ftpkyl(unit, 'EXPOSURE', exposure, 'Total Exposure Time', status)

    ! Primary Image data (Ramp)
    do jj = 1, naxes(2)
        do ii = 1, naxes(1)
            img_array(ii, jj) = int((ii-1) + (jj-1), 2)
        end do
    end do

    nelements = int(naxes(1), 8) * int(naxes(2), 8)
    call ftppri(unit, 1, fpixel, nelements, img_array, status)

    ! ==========================================
    ! STEP 3: Append the Binary Table Extension
    ! ==========================================
    call ftibin(unit, nrows, tfields, ttype, tform, tunit, extname, pcount, status)

    colnum    = 1
    firstrow  = 1
    firstelem = 1
    tbl_nelements = 100
    call ftpcli(unit, colnum, firstrow, firstelem, tbl_nelements, uarray, status)

    colnum    = 2
    call ftpclj(unit, colnum, firstrow, firstelem, tbl_nelements, varray, status)

    ! ==========================================
    ! STEP 4: Append Three Image Extensions
    ! ==========================================
    do ext = 1, 3
        ! call ftcrim (Put Image Extension) automatically creates a new image HDU
        call ftcrim(unit, 16, naxis, naxes, status)

        ! Format name string: "IMAGE_EXT_1", "IMAGE_EXT_2", etc.
        write(extimg_name, '(A, I1)') 'IMAGE_EXT_', ext

        ! Write the extension name keyword using ftpkys (Put Keyword String)
        call ftpkys(unit, 'EXTNAME', extimg_name, 'Extension name', status)

        ! Generate pixel variance across extension loops
        do jj = 1, naxes(2)
            do ii = 1, naxes(1)
                img_array(ii, jj) = int(((ii-1) + (jj-1)) * ext, 2)
            end do
        end do

        ! Write current matrix block layer directly out to disk
        call ftppri(unit, 1, fpixel, nelements, img_array, status)
    end do

    ! ==========================================
    ! STEP 5: Close the file and handle errors
    ! ==========================================
    call ftclos(unit, status)
    call ftfiou(unit, status)

    if (status .ne. 0) then
        call ftrprt('stderr', status)
    end if

end program demo_multi
```

output:

```
bash> gfortran demo_multi.f90 -o demo_multi -lcfitsio

bash> fitsverify -e demo_multi.fits

              fitsverify 4.22 (CFITSIO V4.7.0)
              --------------------------------


File: demo_multi.fits

5 Header-Data Units in this file.

=================== HDU 1: Primary Array ===================

 10 header keywords

 16-bit integer pixels,  2 axes (300 x 200),

=================== HDU 2: BINARY Table ====================

 26 header keywords

 Test_table  (4 columns x 100 rows)

 Col# Name (Units)       Format
   1 Col_1                1I
   2 Col_2                1J
   3 Col_3                1K
   4 Col_4                1B

=================== HDU 3: Image Exten. ====================

 9 header keywords

IMAGE_EXT_1 16-bit integer pixels,  2 axes (300 x 200),

=================== HDU 4: Image Exten. ====================

 9 header keywords

IMAGE_EXT_2 16-bit integer pixels,  2 axes (300 x 200),

=================== HDU 5: Image Exten. ====================

 9 header keywords

IMAGE_EXT_3 16-bit integer pixels,  2 axes (300 x 200),

++++++++++++++++++++++ Error Summary  ++++++++++++++++++++++

 HDU#  Name (version)       Type             Warnings  Errors
 1                          Primary Array    0         0
 2     Test_table           Binary Table     0         0
 3     IMAGE_EXT_1          Image Array      0         0
 4     IMAGE_EXT_2          Image Array      0         0
 5     IMAGE_EXT_3          Image Array      0         0

**** Verification found 0 warning(s) and 0 error(s). ****
```

**Create Demo Image/Image/Table/Image(`demo_stack.f90`)**

```
program demo_stack
    implicit none

    ! CFITSIO logical unit number and status flags
    integer:: unit, status
    integer:: ii, jj

    ! ==========================================
    ! Shared Image Parameters (300 width x 500 height)
    ! ==========================================
    integer:: naxis
    integer:: naxes(2)
    integer:: blocksize
    integer*8:: fpixel, nelements

    ! Native Fortran heap-allocated array to mimic C's malloc()
    integer*2, allocatable:: img_array(:,:)

    ! Random number helper variables
    real:: rand_val
    integer:: noise, star_chance, star_intensity

    ! ==========================================
    ! Table Data Setup
    ! ==========================================
    integer:: colnum, tfields
    integer:: nrows, firstrow, firstelem, tbl_nelements, pcount
    character(len = 20):: extname

    ! Table configurations-lengths must match exactly
    character(len = 16):: ttype(4)
    character(len = 16):: tform(4)
    character(len = 16):: tunit(4)

    ! Mock data arrays (matching the unsigned arrays from the C code)
    integer*2:: uarray(100)
    integer*4:: varray(100)

    ! ==========================================
    ! Parameter & Data Initialization
    ! ==========================================
    status = 0
    fpixel = 1
    naxis = 2
    naxes(1) = 300
    naxes(2) = 500
    blocksize = 1

    ! Dynamically allocate image space on the heap
    allocate(img_array(naxes(1), naxes(2)))

    ! Seed the random number generator using the system clock
    call random_seed()

    ! Initialize test array data for the table
    do ii = 1, 100
        uarray(ii) = int(ii-1, 2)
        varray(ii) = int((ii-1) * 10, 4)
    end do

    ! Initialize table structural metadata
    extname = 'Test_table'
    ttype(1) = 'Col_1'; tform(1) = '1U'; tunit(1) = ' '
    ttype(2) = 'Col_2'; tform(2) = '1V'; tunit(2) = ' '
    ttype(3) = 'Col_3'; tform(3) = '1W'; tunit(3) = ' '
    ttype(4) = 'Col_4'; tform(4) = '1S'; tunit(4) = ' '

    nrows = 1
    tfields = 4
    pcount = 0

    ! Get an unused Logical Unit Number
    call ftgiou(unit, status)

    ! ==========================================
    ! STEP 1: Create the new FITS file
    ! ==========================================
    call ftinit(unit, '!demo_stack.fits', blocksize, status)
    if (status .ne. 0) then
        call ftrprt('stderr', status)
        deallocate(img_array)
        stop
    end if

    ! Create an empty primary HDU array (standard FITS convention, naxis = 0)
    call ftphpr(unit, .true., 16, 0, naxes, 0, 1, .true., status)

    ! ==========================================
    ! STEP 2: Extension 1-Gradient Image
    ! ==========================================
    call ftcrim(unit, 16, naxis, naxes, status)
    call ftpkys(unit, 'EXTNAME', 'GRADIENT', 'Extension name', status)

    ! Generate a linear gradient pattern (Column-Major optimized order)
    do jj = 1, naxes(2)
        do ii = 1, naxes(1)
            img_array(ii, jj) = int((ii-1) + (jj-1), 2)
        end do
    end do

    nelements = int(naxes(1), 8) * int(naxes(2), 8)
    call ftppri(unit, 1, fpixel, nelements, img_array, status)

    ! ==========================================
    ! STEP 3: Extension 2-Binary Table
    ! ==========================================
    call ftibin(unit, nrows, tfields, ttype, tform, tunit, extname, pcount, status)

    colnum    = 1
    firstrow  = 1
    firstelem = 1
    tbl_nelements = 100
    call ftpcli(unit, colnum, firstrow, firstelem, tbl_nelements, uarray, status)

    colnum    = 2
    call ftpclj(unit, colnum, firstrow, firstelem, tbl_nelements, varray, status)

    ! ==========================================
    ! STEP 4: Extension 3-Simulated Sky Image
    ! ==========================================
    call ftcrim(unit, 16, naxis, naxes, status)
    call ftpkys(unit, 'EXTNAME', 'SIMULATED_SKY', 'Extension name', status)

    ! Generate background noise+random stars simulation
    do jj = 1, naxes(2)
        do ii = 1, naxes(1)
            ! Native random call outputs a float between 0.0 and 1.0
            call random_number(rand_val)

            ! Base background sky noise (scaled between 10 and 20)
            noise = 10+int(rand_val*11.0)
            img_array(ii, jj) = int(noise, 2)

            ! Inject random bright stars (0.5% chance)
            call random_number(rand_val)
            if (rand_val < 0.005) then
                call random_number(rand_val)
                ! Star intensity scaled between 500 and 2500
                star_intensity = 500+int(rand_val*2001.0)
                img_array(ii, jj) = img_array(ii, jj) + int(star_intensity, 2)
            end if
        end do
    end do

    call ftppri(unit, 1, fpixel, nelements, img_array, status)

    ! ==========================================
    ! STEP 5: Close the file and clean up
    ! ==========================================
    call ftclos(unit, status)
    call ftfiou(unit, status)

    if (status .ne. 0) then
        call ftrprt('stderr', status)
    end if

    ! Deallocate heap array safely
    deallocate(img_array)

end program demo_stack
```

output:

```
bash> gfortran demo_stack.f90 -o demo_stack -lcfitsio

bash> fitsverify -e demo_stack.fits

              fitsverify 4.22 (CFITSIO V4.7.0)
              --------------------------------


File: demo_stack.fits

4 Header-Data Units in this file.

=================== HDU 1: Primary Array ===================

 7 header keywords

 Null data array; NAXIS = 0

=================== HDU 2: Image Exten. ====================

 9 header keywords

GRADIENT 16-bit integer pixels,  2 axes (300 x 500),

=================== HDU 3: BINARY Table ====================

 26 header keywords

 Test_table  (4 columns x 100 rows)

 Col# Name (Units)       Format
   1 Col_1                1I
   2 Col_2                1J
   3 Col_3                1K
   4 Col_4                1B

=================== HDU 4: Image Exten. ====================

 9 header keywords

SIMULATED_SKY 16-bit integer pixels,  2 axes (300 x 500),

++++++++++++++++++++++ Error Summary  ++++++++++++++++++++++

 HDU#  Name (version)       Type             Warnings  Errors
 1                          Primary Array    0         0
 2     GRADIENT             Image Array      0         0
 3     Test_table           Binary Table     0         0
 4     SIMULATED_SKY        Image Array      0         0

**** Verification found 0 warning(s) and 0 error(s). ****
```

**Create Demo Image/Table (`demo_stars.f90`)**

```
program demo_stars
    implicit none

    ! CFITSIO logical unit number and status flags
    integer:: unit, status
    integer:: ii, jj

    ! --- HDU 1: Image Gradient Parameters---
    integer:: naxis
    integer:: naxes(2)
    integer:: blocksize
    integer*8:: fpixel, nelements
    integer*2, allocatable:: img_array(:,:)

    ! --- HDU 2: Binary Table Parameters & Parsed Storage Arrays---
    integer, parameter:: MAX_STARS = 1000
    integer*4:: star_hips(MAX_STARS)

    ! CRUCIAL FIX: Character arrays must be mapped contiguously to match C pointers
    character(len = 64):: star_names(MAX_STARS)

    real*4    :: star_v_mags(MAX_STARS)
    real*4    :: star_mv_mags(MAX_STARS)
    real*4    :: star_dists(MAX_STARS)
    integer*4:: star_charts(MAX_STARS)

    ! Table size and counters
    integer   :: star_count, tbl_tfields, pcount

    ! File-reading and tokenization buffers
    integer:: io_unit, io_status, line_len
    character(len = 256):: raw_line
    character(len = 64)  :: tokens(6)
    integer:: t_start, t_end, col_idx
    character:: TAB

    ! Final Binary Table FITS configuration
    character(len = 70):: tbl_ttype(6)
    character(len = 70):: tbl_tform(6)
    character(len = 70):: tbl_tunit(6)

    ! ==========================================
    ! Parameter & Variable Initialization
    ! ==========================================
    status = 0
    fpixel = 1
    naxis = 2
    naxes(1) = 300
    naxes(2) = 200
    blocksize = 1
    star_count = 0
    TAB = char(9)

    allocate(img_array(naxes(1), naxes(2)))

    ! Populating HDU 1 Gradient loop
    do jj = 1, naxes(2)
        do ii = 1, naxes(1)
            img_array(ii, jj) = int((ii-1) + (jj-1), 2)
        end do
    end do

    ! Populating HDU 2 Table Field Meta
    tbl_tfields = 6
    pcount = 0
    tbl_ttype(1) = 'HIP';      tbl_tform(1) = '1J';  tbl_tunit(1) = ' '
    tbl_ttype(2) = 'Name';     tbl_tform(2) = '64A'; tbl_tunit(2) = ' '
    tbl_ttype(3) = 'V';        tbl_tform(3) = '1E';  tbl_tunit(3) = 'mag'
    tbl_ttype(4) = 'M_V';      tbl_tform(4) = '1E';  tbl_tunit(4) = 'mag'
    tbl_ttype(5) = 'Distance'; tbl_tform(5) = '1E';  tbl_tunit(5) = 'ly'
    tbl_ttype(6) = 'Chart';    tbl_tform(6) = '1J';  tbl_tunit(6) = ' '

    ! ==========================================
    ! STEP 1: Parse the local tab-separated .dat file
    ! ==========================================
    io_unit = 42
    open(unit = io_unit, file='closest_stars.dat', status='old', iostat = io_status)
    if (io_status .ne. 0) then
        write(*,*) 'Error: closest_stars.dat could not be opened.'
        deallocate(img_array)
        stop
    end if

    ! Skip the header row entirely
    read(io_unit, '(A)', iostat = io_status) raw_line

    ! Parse data line by line
    do while (star_count < MAX_STARS)
        read(io_unit, '(A)', iostat = io_status) raw_line
        if (io_status .ne. 0) exit

        line_len = len_trim(raw_line)
        if (line_len == 0) cycle

        ! Clear token registers completely before tracking row slices
        tokens = ''
        col_idx = 1
        t_start = 1

        ! Tab Tokenizer with Strict Index Validation Checks
        do ii = 1, line_len
            if (raw_line(ii:ii) == TAB) then
                t_end = ii-1
                if (t_end >= t_start .and. col_idx <= 6) then
                    tokens(col_idx) = adjustl(raw_line(t_start:t_end))
                end if
                col_idx = col_idx+1
                t_start = ii+1
                if (col_idx > 6) exit
            end if
        end do

        ! Capture final column slice safely
        if (col_idx <= 6 .and. t_start <= line_len) then
            tokens(col_idx) = adjustl(raw_line(t_start:line_len))
        end if

        ! Process parsed line entries with rigorous safety checks to prevent crashes
        if (len_trim(tokens(1)) > 0) then
            star_count = star_count+1

            ! 1. HIP Column
            read(tokens(1), *) star_hips(star_count)

            ! 2. Name Column (Handles empty values cleanly)
            if (len_trim(tokens(2)) == 0) then
                star_names(star_count) = 'Unnamed'
            else
                star_names(star_count) = trim(tokens(2))
            end if

            ! 3. V Magnitude
            if (len_trim(tokens(3)) == 0) then
                star_v_mags(star_count) = 0.0
            else
                read(tokens(3), *) star_v_mags(star_count)
            end if

            ! 4. M_V Magnitude
            if (len_trim(tokens(4)) == 0) then
                star_mv_mags(star_count) = 0.0
            else
                read(tokens(4), *) star_mv_mags(star_count)
            end if

            ! 5. Distance Column
            if (len_trim(tokens(5)) == 0) then
                star_dists(star_count) = 0.0
            else
                read(tokens(5), *) star_dists(star_count)
            end if

            ! 6. Chart Column
            if (len_trim(tokens(6)) == 0) then
                star_charts(star_count) = 0
            else
                read(tokens(6), *) star_charts(star_count)
            end if
        end if
    end do
    close(io_unit)

    ! ==========================================
    ! EXECUTION STEP 1: Main Container & HDU 1
    ! ==========================================
    call ftgiou(unit, status)
    call ftinit(unit, '!demo_stars.fits', blocksize, status)
    if (status .ne. 0) then
        call ftrprt('stderr', status)
        deallocate(img_array)
        stop
    end if

    call ftphpr(unit, .true., 16, naxis, naxes, 0, 1, .true., status)
    nelements = int(naxes(1), 8) * int(naxes(2), 8)
    call ftppri(unit, 1, fpixel, nelements, img_array, status)
    deallocate(img_array)

    ! ==========================================
    ! EXECUTION STEP 2: Create and Write Binary Table
    ! ==========================================
    call ftibin(unit, star_count, tbl_tfields, tbl_ttype, tbl_tform, tbl_tunit, 'CLOSEST_STARS', pcount, status)

    if (star_count > 0 .and. status == 0) then
        call ftpclj(unit, 1, 1, 1, star_count, star_hips, status)

        ! FIX: Writing strings line-by-line eliminates the compiler array copy stack fault completely
        do ii = 1, star_count
            call ftpcls(unit, 2, ii, 1, 1, star_names(ii), status)
        end do

        call ftpcle(unit, 3, 1, 1, star_count, star_v_mags, status)
        call ftpcle(unit, 4, 1, 1, star_count, star_mv_mags, status)
        call ftpcle(unit, 5, 1, 1, star_count, star_dists, status)
        call ftpclj(unit, 6, 1, 1, star_count, star_charts, status)
    end if

    ! ==========================================
    ! EXECUTION STEP 3: Close File Handler
    ! ==========================================
    call ftclos(unit, status)
    call ftfiou(unit, status)

    if (status .ne. 0) then
        call ftrprt('stderr', status)
    else
        write(*, '(A, I4, A)') 'Success  ! Loaded ', star_count, ' rows into the FITS binary table.'
    end if

end program demo_stars
```

output:

```
bash> gfortran demo_stars.f90 -o demo_stars -lcfitsio
bash> fitsverify -e demo_stars.fits

              fitsverify 4.22 (CFITSIO V4.7.0)
              --------------------------------


File: demo_stars.fits

2 Header-Data Units in this file.

=================== HDU 1: Primary Array ===================

 9 header keywords

 16-bit integer pixels,  2 axes (300 x 200),

=================== HDU 2: BINARY Table ====================

 25 header keywords

 CLOSEST_STARS  (6 columns x 20 rows)

 Col# Name (Units)       Format
   1 HIP                  1J
   2 Name                 64A
   3 V (mag)              1E
   4 M_V (mag)            1E
   5 Distance (ly)        1E
   6 Chart                1J

++++++++++++++++++++++ Error Summary  ++++++++++++++++++++++

 HDU#  Name (version)       Type             Warnings  Errors
 1                          Primary Array    0         0
 2     CLOSEST_STARS        Binary Table     0         0

**** Verification found 0 warning(s) and 0 error(s). ****
```

**Create Demo Image/Table/Table (`demo_wcs.f90`)**

```
program demo_wcs
    implicit none

    ! CFITSIO logical unit number and status flags
    integer:: unit, status
    integer:: ii, jj

    ! --- HDU 1: Image Gradient Parameters---
    integer:: naxis
    integer:: naxes(2)
    integer:: blocksize
    integer*8:: fpixel, nelements
    integer*2, allocatable:: img_array(:,:)

    ! --- WCS Precision Floating Variables---
    double precision:: crpix1, crpix2
    double precision:: crval1, crval2
    double precision:: cdelt1, cdelt2
    double precision:: equinox

    ! --- HDU 2: ASCII Table (Text Payload) Parameters---
    integer:: txt_rowlen, txt_nrows, txt_tfields
    character(len = 70):: txt_ttype(1)
    character(len = 70):: txt_tform(1)
    character(len = 70):: txt_tunit(1)
    character(len = 40):: text_payload  ! Scaled to standard scalar string format

    ! --- HDU 3: Binary Table Parameters & Parsed Storage Arrays---
    integer, parameter:: MAX_STARS = 1000
    integer*4, allocatable:: star_hips(:)
    real*4, allocatable    :: star_v_mags(:)
    real*4, allocatable    :: star_mv_mags(:)
    real*4, allocatable    :: star_dists(:)
    integer*4, allocatable:: star_charts(:)

    ! CRUCIAL FIX: Pack all star names into one continuous string matrix block
    ! This guarantees exact layout alignment down to the low-level C API layer
    character(len = 64), allocatable:: star_names(:)

    ! Table size and counters
    integer   :: star_count, tbl_tfields, pcount

    ! File-reading and tokenization buffers
    integer:: io_unit, io_status, line_len
    character(len = 256):: raw_line
    character(len = 64)  :: tokens(6)
    integer:: t_start, t_end, col_idx
    character:: TAB

    ! Final Binary Table FITS configuration
    character(len = 70):: tbl_ttype(6)
    character(len = 70):: tbl_tform(6)
    character(len = 70):: tbl_tunit(6)

    ! ==========================================
    ! Parameter & Variable Initialization
    ! ==========================================
    status = 0
    fpixel = 1
    naxis = 2
    naxes(1) = 300
    naxes(2) = 200
    blocksize = 1
    star_count = 0
    TAB = char(9)

    ! Allocate heap memory arrays safely
    allocate(img_array(naxes(1), naxes(2)))
    allocate(star_hips(MAX_STARS))
    allocate(star_names(MAX_STARS))
    allocate(star_v_mags(MAX_STARS))
    allocate(star_mv_mags(MAX_STARS))
    allocate(star_dists(MAX_STARS))
    allocate(star_charts(MAX_STARS))

    ! Populating HDU 1 Gradient matrix
    do jj = 1, naxes(2)
        do ii = 1, naxes(1)
            img_array(ii, jj) = int((ii-1) + (jj-1), 2)
        end do
    end do

    ! Populating HDU 2 Data Config (ASCII Table Layout)
    txt_rowlen = 0
    txt_nrows = 1
    txt_tfields = 1
    txt_ttype(1) = 'TEXT_LINE'
    txt_tform(1) = 'A40'
    txt_tunit(1) = ' '
    text_payload = 'Hello world'

    ! Populating HDU 3 Table Field Meta
    tbl_tfields = 6
    pcount = 0
    tbl_ttype(1) = 'HIP';      tbl_tform(1) = '1J';  tbl_tunit(1) = ' '
    tbl_ttype(2) = 'Name';     tbl_tform(2) = '64A'; tbl_tunit(2) = ' '
    tbl_ttype(3) = 'V';        tbl_tform(3) = '1E';  tbl_tunit(3) = 'mag'
    tbl_ttype(4) = 'M_V';      tbl_tform(4) = '1E';  tbl_tunit(4) = 'mag'
    tbl_ttype(5) = 'Distance'; tbl_tform(5) = '1E';  tbl_tunit(5) = 'ly'
    tbl_ttype(6) = 'Chart';    tbl_tform(6) = '1J';  tbl_tunit(6) = ' '

    ! ==========================================
    ! STEP 1: Parse the local tab-separated .dat file
    ! ==========================================
    io_unit = 42
    open(unit = io_unit, file='closest_stars.dat', status='old', iostat = io_status)
    if (io_status .ne. 0) then
        write(*,*) 'Error: closest_stars.dat could not be opened.'
        goto 999
    end if

    ! Skip the header row entirely
    read(io_unit, '(A)', iostat = io_status) raw_line

    ! Parse data line by line
    do while (star_count < MAX_STARS)
        read(io_unit, '(A)', iostat = io_status) raw_line
        if (io_status .ne. 0) exit

        line_len = len_trim(raw_line)
        if (line_len == 0) cycle

        tokens = ''
        col_idx = 1
        t_start = 1

        ! Tab Tokenizer
        do ii = 1, line_len
            if (raw_line(ii:ii) == TAB) then
                t_end = ii-1
                if (t_end >= t_start .and. col_idx <= 6) then
                    tokens(col_idx) = adjustl(raw_line(t_start:t_end))
                end if
                col_idx = col_idx+1
                t_start = ii+1
                if (col_idx > 6) exit
            end if
        end do

        if (col_idx <= 6 .and. t_start <= line_len) then
            tokens(col_idx) = adjustl(raw_line(t_start:line_len))
        end if

        if (len_trim(tokens(1)) > 0) then
            star_count = star_count+1
            read(tokens(1), *) star_hips(star_count)

            if (len_trim(tokens(2)) == 0) then
                star_names(star_count) = 'Unnamed'
            else
                star_names(star_count) = trim(tokens(2))
            end if

            read(tokens(3), *) star_v_mags(star_count)
            read(tokens(4), *) star_mv_mags(star_count)
            read(tokens(5), *) star_dists(star_count)
            read(tokens(6), *) star_charts(star_count)
        end if
    end do
    close(io_unit)

    ! ==========================================
    ! EXECUTION STEP 1: Main Container & HDU 1
    ! ==========================================
    call ftgiou(unit, status)
    call ftinit(unit, '!demo_wcs.fits', blocksize, status)
    if (status .ne. 0) then
        call ftrprt('stderr', status)
        goto 999
    end if

    call ftphpr(unit, .true., 16, naxis, naxes, 0, 1, .true., status)
    nelements = int(naxes(1), 8) * int(naxes(2), 8)
    call ftppri(unit, 1, fpixel, nelements, img_array, status)

    ! -------------------------------------------------------------
    ! AUTOMATED WCS MAPPING INJECTION (For Primary Gradient Image)
    ! -------------------------------------------------------------
    call ftpkys(unit, 'CTYPE1', 'RA---TAN', 'Coordinate type for Axis 1', status)
    call ftpkys(unit, 'CTYPE2', 'DEC--TAN', 'Coordinate type for Axis 2', status)

    crpix1 = 150.0d0
    crpix2 = 100.0d0
    call ftpkyd(unit, 'CRPIX1', crpix1, 7, 'Reference pixel on Axis 1', status)
    call ftpkyd(unit, 'CRPIX2', crpix2, 7, 'Reference pixel on Axis 2', status)

    crval1 = 219.9014d0
    crval2 = -60.8339d0
    call ftpkyd(unit, 'CRVAL1', crval1, 7, 'RA at reference pixel (deg)', status)
    call ftpkyd(unit, 'CRVAL2', crval2, 7, 'DEC at reference pixel (deg)', status)

    cdelt1 = -0.00027778d0
    cdelt2 =  0.00027778d0
    call ftpkyd(unit, 'CDELT1', cdelt1, 8, 'Pixel scale Axis 1 (deg/pixel)', status)
    call ftpkyd(unit, 'CDELT2', cdelt2, 8, 'Pixel scale Axis 2 (deg/pixel)', status)

    call ftpkys(unit, 'RADECSYS', 'FK5', 'Coordinate system framework', status)
    equinox = 2000.0d0
    call ftpkyd(unit, 'EQUINOX', equinox, 1, 'Equinox of coordinates', status)
    ! -------------------------------------------------------------

    ! ==========================================
    ! EXECUTION STEP 2: HDU 2 (ASCII Text Table)
    ! ==========================================
    call ftibin(unit, txt_nrows, txt_tfields, txt_ttype, txt_tform, txt_tunit, 'TEXT_MESSAGE', pcount, status)

    ! FIX: Passing the scalar string variable cleanly prevents hidden pointer mismatches
    call ftpcls(unit, 1, 1, 1, 1, text_payload, status)

    ! ==========================================
    ! EXECUTION STEP 3: HDU 3 (The Parsed Binary Table)
    ! ==========================================
    call ftibin(unit, star_count, tbl_tfields, tbl_ttype, tbl_tform, tbl_tunit, 'CLOSEST_STARS', pcount, status)

    if (star_count > 0 .and. status == 0) then
        call ftpclj(unit, 1, 1, 1, star_count, star_hips, status)

        ! FIX: Passing the complete contiguous 1D array reference directly block-writes
        ! all text values at once, matching the C-wrapper layout structure exactly
        call ftpcls(unit, 2, 1, 1, star_count, star_names, status)

        call ftpcle(unit, 3, 1, 1, star_count, star_v_mags, status)
        call ftpcle(unit, 4, 1, 1, star_count, star_mv_mags, status)
        call ftpcle(unit, 5, 1, 1, star_count, star_dists, status)
        call ftpclj(unit, 6, 1, 1, star_count, star_charts, status)
    end if

    ! ==========================================
    ! EXECUTION STEP 4: Close File Handler
    ! ==========================================
    call ftclos(unit, status)
    call ftfiou(unit, status)

    if (status .ne. 0) then
        call ftrprt('stderr', status)
    else
        write(*, '(A, I4, A)') 'Success  ! Created WCS mapped file containing ', star_count, ' star rows.'
    end if

999 continue
    ! Safe memory resource cleanup
    if (allocated(img_array)) deallocate(img_array)
    if (allocated(star_hips)) deallocate(star_hips)
    if (allocated(star_names)) deallocate(star_names)
    if (allocated(star_v_mags)) deallocate(star_v_mags)
    if (allocated(star_mv_mags)) deallocate(star_mv_mags)
    if (allocated(star_dists)) deallocate(star_dists)
    if (allocated(star_charts)) deallocate(star_charts)

end program demo_wcs
```

output:

```
bash> gfortran demo_wcs.f90 -o demo_wcs -lcfitsio
bash> fitsverify -e demo_wcs.fits

              fitsverify 4.22 (CFITSIO V4.7.0)
              --------------------------------


File: demo_wcs.fits

3 Header-Data Units in this file.

=================== HDU 1: Primary Array ===================

 19 header keywords

 16-bit integer pixels,  2 axes (300 x 200),

=================== HDU 2: BINARY Table ====================

 12 header keywords

 TEXT_MESSAGE  (1 columns x 1 rows)

 Col# Name (Units)       Format
   1 TEXT_LINE            A40

=================== HDU 3: BINARY Table ====================

 25 header keywords

 CLOSEST_STARS  (6 columns x 20 rows)

 Col# Name (Units)       Format
   1 HIP                  1J
   2 Name                 64A
   3 V (mag)              1E
   4 M_V (mag)            1E
   5 Distance (ly)        1E
   6 Chart                1J

++++++++++++++++++++++ Error Summary  ++++++++++++++++++++++

 HDU#  Name (version)       Type             Warnings  Errors
 1                          Primary Array    0         0
 2     TEXT_MESSAGE         Binary Table     0         0
 3     CLOSEST_STARS        Binary Table     0         0

**** Verification found 0 warning(s) and 0 error(s). ****
```

### 25 Oct 2026 by Oleg G.Kapranov
