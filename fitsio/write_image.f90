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
