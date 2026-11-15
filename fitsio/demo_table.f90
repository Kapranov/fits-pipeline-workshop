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
