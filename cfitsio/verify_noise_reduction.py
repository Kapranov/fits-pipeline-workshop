import numpy as np
from astropy.io import fits

print("==================================================================")
print("     CALCULATING EXACT BACKGROUND NOISE FLOOR REDUCTION           ")
print("==================================================================")

try:
    # 1. Load the original raw 3D data cube (using the first frame)
    with fits.open("stage_17_time_cube.fits") as hdul_cube:
        # Assuming the data is in the primary extension [0] or name it if needed
        raw_frame = hdul_cube["STARRY_SKY_CUBE"].data[0] # Grab Frame 1 out of the 3D cube


    # 2. Load the collapsed final 2D stacked frame
    with fits.open("stage_43_stacked_master_image.fits") as hdul_master:
        stacked_frame = hdul_master[0].data

    # 3. Define a clean, star-free corner region of the sky background
    # Using a 40x40 pixel window in the bottom-left corner [0:40, 0:40]
    y_min, y_max = 0, 40
    x_min, x_max = 0, 40

    raw_bg_box = raw_frame[y_min:y_max, x_min:x_max]
    stacked_bg_box = stacked_frame[y_min:y_max, x_min:x_max]

    # 4. Compute variance (Noise Power Floor) across both files
    var_raw = np.var(raw_bg_box)
    var_stacked = np.var(stacked_bg_box)

    # 5. Calculate the exact percentage drop in background noise power
    pct_drop = (1.0 - (var_stacked / var_raw)) * 100.0

    # 6. Output the analysis breakdown
    print(f"Analysis Region Bounds : Y[{y_min}:{y_max}], X[{x_min}:{x_max}]")
    print(f"Raw Background Variance: {var_raw:.6e}")
    print(f"Final Background Variance: {var_stacked:.6e}")
    print("------------------------------------------------------------------")
    print(f"[SUCCESS] Background Noise Power Floor Crushed by {pct_drop:.2f}%")
    print("==================================================================")

except FileNotFoundError as e:
    print(f"\n[ERROR] Missing dataset component to process variance: {e.filename}")
except IndexError:
    print("\n[ERROR] Dimension mismatch. Ensure 'stage_17_time_cube.fits' is a 3D array.")
