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
