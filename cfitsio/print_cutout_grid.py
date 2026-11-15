import os
import numpy as np
from astropy.io import fits

filename = "star_cutout_cube.fits"

print("==================================================================")
print("       3D SUB-CUBE SECTOR SCANNER: NUMERICAL MATRIX GRID          ")
print("==================================================================")

if not os.path.exists(filename):
    print(f"[ERROR] File '{filename}' not found.")
    print("Please compile and execute your 'stage_30_write_subset' C program first!")
    print("==================================================================")
    exit(1)

try:
    # 1. Open the mini sub-cube FITS container file
    with fits.open(filename) as hdul:
        # Pull metadata tracking lineage keys directly from the primary header card block
        header = hdul[0].header
        orig_x = header.get("ORIG_X", "UNKNOWN")
        orig_y = header.get("ORIG_Y", "UNKNOWN")

        # Load the 3D sub-cube pixel data array
        # Shape structure in Python/NumPy: (Frames, Height, Width) -> (5, 5, 5)
        cube_data = hdul[0].data

    num_frames, height, width = cube_data.shape
    print(f"[INFO] Loaded Standalone Mini-Cube: {width}x{height} pixels across {num_frames} frames.")
    print(f"[INFO] Original Master Lower-Left Anchor Coordinates: (X={orig_x}, Y={orig_y})")
    print("==================================================================")

    # 2. Sequential loop through time frame dimensions to output numerical matrix tables
    for f in range(num_frames):
        print(f"\n[TIME STEP FRAME #{f + 1}]")
        print("---------------------------------------")
        print("         X=1      X=2      X=3      X=4      X=5")
        print("---------------------------------------")

        # Pull the isolated 5x5 matrix frame slice
        frame_matrix = cube_data[f]

        # Loop through rows backwards (height-1 down to 0) so the top row prints at the top
        # to match the standard astronomical "origin='lower'" visual telescope layout orientation
        for y in range(height - 1, -1, -1):
            row_str = f"Y={y + 1} | "
            for x in range(width):
                pixel_value = frame_matrix[y, x]
                # Format to 4 decimal places inside an 8-character column space cushion
                row_str += f"{pixel_value:8.4f} "
            print(row_str)

        print("---------------------------------------")

    print("\n==================================================================")
    print(" SUCCESS: Deep space micro-canvas matrix print complete.         ")
    print("==================================================================")

except Exception as e:
    print(f"\n[CRITICAL ERROR] Failed to parse mini-cube dataset: {str(e)}")
