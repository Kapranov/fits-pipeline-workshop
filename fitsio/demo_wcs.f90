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
