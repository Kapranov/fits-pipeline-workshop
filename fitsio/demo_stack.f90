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
