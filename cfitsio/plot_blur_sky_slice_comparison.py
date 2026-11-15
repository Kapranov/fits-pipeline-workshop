import os
import numpy as np
import matplotlib.pyplot as plt
from astropy.io import fits

# 1. Verification of file names matching your pipeline configuration
file_master = "stage_25_master_observatory_dataset.fits"
file_blurred = "stage_38_telescope_blurred_cube.fits"

print("==================================================================")
print("     2D PIXEL CONVOLUTION BLUR PROFILE VISUALIZER INITIALIZED    ")
print("==================================================================")

# Verify paths before mounting context managers
if not os.path.exists(file_master) or not os.path.exists(file_blurred):
    print("[ERROR] Missing required FITS files. Ensure both pipelines executed successfully.")
    print(f" Checking path stability: master={os.path.exists(file_master)}, blurred={os.path.exists(file_blurred)}")
    exit(1)

try:
    # 2. Open both FITS datasets concurrently
    with fits.open(file_master) as hdul_mast, fits.open(file_blurred) as hdul_blur:
        # Load Frame 1 (Index 0 in Python/NumPy array slicing layout) from both cubes
        sharp_frame = hdul_mast["STARRY_SKY_CUBE"].data[0]
        blurred_frame = hdul_blur["BLURRED_SKY"].data[0]

    print("[SUCCESS] Successfully loaded both 3D layers concurrently.")
    print(f" Sharp Canvas Grid Resolution : {sharp_frame.shape} pixels")
    print(f" Blurred Canvas Grid Resolution: {blurred_frame.shape} pixels")
    print("-" * 66)

    # 3. Create a dual-panel layout figure frame
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    plt.subplots_adjust(wspace=0.12)

    # Establish uniform color tracking boundaries across both chart displays
    vmin_val = 0.00
    vmax_val = 1.20 # Clipped slightly to isolate core profile blurring paths clearly

    # PANEL 1: Raw Sharp Science Frame
    im1 = ax1.imshow(sharp_frame, cmap="inferno", origin="lower", vmin=vmin_val, vmax=vmax_val)
    ax1.set_title("Panel A: Sharp Original Science Image\n(STARRY_SKY_CUBE Layer)", fontsize=11, weight="bold")
    ax1.set_xlabel("X coordinate (pixels)")
    ax1.set_ylabel("Y coordinate (pixels)")
    ax1.grid(color="white", linestyle=":", alpha=0.15)

    # PANEL 2: Convolved Defocused Telescope Frame
    im2 = ax2.imshow(blurred_frame, cmap="inferno", origin="lower", vmin=vmin_val, vmax=vmax_val)
    ax2.set_title("Panel B: Defocused Telescope Simulation\n(5x5 Normalized Gaussian Convolution Blur)", fontsize=11, weight="bold")
    ax2.set_xlabel("X coordinate (pixels)")
    ax2.get_yaxis().set_visible(False) # Hide Y ticks on the right panel for clean spacing
    ax2.grid(color="white", linestyle=":", alpha=0.15)

    # 4. Append a shared single color bar tracking indicator mapping scale to the right side
    cbar = fig.colorbar(im2, ax=[ax1, ax2], pad=0.02, aspect=20)
    cbar.set_label("Integrated Flux Intensity Scale (Jansky / Pixel)", rotation=270, labelpad=15)

    # 5. Save chart image summary profile to disk drive sector layout
    output_filename = "telescope_blur_comparison.png"
    plt.savefig(output_filename, dpi=300, bbox_inches="tight")
    plt.show()

    print(f"[SUCCESS] Comparative side-by-side surface plot saved to: {output_filename}\n")

except Exception as e:
    print(f"\n[CRITICAL ERROR] Failed to complete imshow rendering loop: {str(e)}")
