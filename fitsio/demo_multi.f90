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
