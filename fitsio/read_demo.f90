program read_demo
    use iso_c_binding
    implicit none

    interface
        subroutine c_read_image(filename, data, nx, ny, status) bind(c, name="c_read_image")
            import:: c_char, c_short, c_int
            character(kind = c_char), intent(in):: filename(*)
            integer(c_short), intent(out):: data(*)
            integer(c_int), value:: nx, ny
            integer(c_int):: status
        end subroutine c_read_image

        subroutine c_read_int_key(filename, keyname, value, status) bind(c, name="c_read_int_key")
            import:: c_char, c_int
            character(kind = c_char), intent(in):: filename(*)
            character(kind = c_char), intent(in):: keyname(*)
            integer(c_int), intent(out):: value
            integer(c_int):: status
        end subroutine c_read_int_key

        subroutine c_dump_all_headers(filename, status) bind(c, name="c_dump_all_headers")
            import:: c_char, c_int
            character(kind = c_char), intent(in):: filename(*)
            integer(c_int):: status
        end subroutine c_dump_all_headers
    end interface

    integer(c_int):: nx = 100_c_int
    integer(c_int):: ny = 100_c_int
    integer(c_int):: status, exposure_val
    integer(c_short), allocatable, dimension(:,:):: array

    status = 0_c_int
    exposure_val = 0_c_int
    allocate(array(nx, ny))

    ! 1. Call the new function to dump all info first
    call c_dump_all_headers("output_image.fits" // c_null_char, status)

    if (status .ne. 0_c_int) then
        print *, "Error trying to parse headers  ! Status:", status
        status = 0_c_int
    end if

    print *, "Reading structural pixels..."
    call c_read_image("output_image.fits" // c_null_char, array, nx, ny, status)

    if (status .eq. 0_c_int) then
        print *, "SUCCESS: Pixel matrix parsed completely!"
        print *, "First grid element:", array(1, 1)
        print *, "Last grid element: ", array(nx, ny)
    else
        print *, "CRITICAL ERROR: Matrix reading failed. Status code:", status
    end if

    deallocate(array)
end program read_demo
