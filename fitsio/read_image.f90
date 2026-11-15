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
