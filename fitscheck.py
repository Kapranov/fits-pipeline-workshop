from pathlib import Path
import astropy.io.fits as fits

# 1. Define the directory path containing your FITS files
fits_directory = Path(".")

# Find all FITS files matching extensions: .fits, .fit, or .fts
fits_files = list(fits_directory.glob("*.fit*"))

print(f"Found {len(fits_files)} FITS files to process.\n")

# 2. Loop through each file and update headers
for file_path in fits_files:
    try:
        # Open the file in update mode ('update' allows direct modification)
        with fits.open(file_path, mode="update") as hdul:
            print(f"Processing: {file_path.name}")

            # Loop through every HDU (extension) inside the file
            for i, hdu in enumerate(hdul):
                # FIXED: verify_checksum() returns a single integer status flag
                # 1 = valid/matches, 0 = missing keys, -1 = data corrupted
                checksum_status = hdu.verify_checksum()

                # Check if tags are missing or out of sync
                if checksum_status != 1:
                    status_msg = "Missing" if checksum_status == 0 else "CORRUPTED"
                    print(f"  -> HDU {i}: {status_msg} checksum. Recalculating...")

                    # This computes and writes both CHECKSUM and DATASUM keys
                    hdu.add_checksum()
                else:
                    print(f"  -> HDU {i}: Checksum verified OK.")

        # File is automatically saved and closed properly here via context manager

    except Exception as e:
        print(f"  [ERROR] Failed to process {file_path.name}: {e}")

print("\nPipeline execution complete! All files verified and updated.")
