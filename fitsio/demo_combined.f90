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
