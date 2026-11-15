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
