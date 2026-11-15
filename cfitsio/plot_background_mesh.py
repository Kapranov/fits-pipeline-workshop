import numpy as np
import matplotlib.pyplot as plt
from astropy.io import fits

# 1. Configuration filenames matching your C pipeline paths
file_science = "stage_25_master_observatory_dataset.fits"
file_background = "stage_33_master_background_maps.fits"

print("==================================================================")
print("     2D BACKGROUND MESH SURFACE MAP VISUALIZER INITIALIZED        ")
print("==================================================================")

try:
    # 2. Open both FITS datasets concurrently
    with fits.open(file_science) as hdul_sci, fits.open(file_background) as hdul_bg:
        # Load Frame 1 (Index 0 in Python/NumPy format) from both cubes
        science_frame = hdul_sci["STARRY_SKY_CUBE"].data[0]
        background_frame = hdul_bg["BACKGROUND_MESH"].data[0]

    print("[SUCCESS] Successfully loaded both 3D layers concurrently.")
    print(f" Science Canvas Resolution   : {science_frame.shape} pixels")
    print(f" Background Mesh Resolution  : {background_frame.shape} pixels")
    print("-" * 66)

    # 3. Create a dual-panel layout figure frame
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    plt.subplots_adjust(wspace=0.15)

    # Establish uniform color scaling tracking bounds across both charts
    vmin_val = 0.00
    vmax_val = 0.15 # Clipped lower to clearly showcase the ambient diffuse background map

    # PANEL 1: Raw Science Frame (Stars + Noise Background)
    im1 = ax1.imshow(science_frame, cmap="inferno", origin="lower", vmin=vmin_val, vmax=vmax_val)
    ax1.set_title("Panel A: Raw Science Image\n(Stars + Ambient Sky Glow Noise)", fontsize=11, weight="bold")
    ax1.set_xlabel("X Vector Index (pixels)")
    ax1.set_ylabel("Y Vector Index (pixels)")
    ax1.grid(color="white", linestyle=":", alpha=0.2)

    # PANEL 2: Isolated Smooth Estimated 2D Background Mesh Map
    im2 = ax2.imshow(background_frame, cmap="inferno", origin="lower", vmin=vmin_val, vmax=vmax_val)
    ax2.set_title("Panel B: Estimated 2D Background Surface\n(32x32 Median Mesh Map)", fontsize=11, weight="bold")
    ax2.set_xlabel("X Vector Index (pixels)")
    ax2.get_yaxis().set_visible(False) # Hide Y axis ticks on right side panel for clean spacing
    ax2.grid(color="white", linestyle=":", alpha=0.2)

    # 4. Append a shared single color bar tracking indicator mapping scale to the right side
    cbar = fig.colorbar(im2, ax=[ax1, ax2], pad=0.02, aspect=20)
    cbar.set_label("Flux Intensity Units (Jansky / Pixel Scale)", rotation=270, labelpad=15)

    # 5. Save chart image summary profile to disk drive sector layout
    output_filename = "background_mesh_comparison.png"
    plt.savefig(output_filename, dpi=300, bbox_inches="tight")
    plt.show()

    print(f"[SUCCESS] Comparative side-by-side surface plot saved to: {output_filename}\n")

except FileNotFoundError as e:
    print(f"\n[ERROR] Missing required FITS files. Ensure both pipelines have run successfully:\n{str(e)}")
except Exception as e:
    print(f"\n[CRITICAL ERROR] Failed to complete imshow rendering loop: {str(e)}")
