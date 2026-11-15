import numpy as np
import matplotlib.pyplot as plt
from astropy.io import fits

# 1. Paths matching your completed C engineering architecture
file_science = "stage_25_master_observatory_dataset.fits"
file_background = "stage_33_master_background_maps.fits"
file_reduced = "stage_34_master_reduced_science.fits"

print("==================================================================")
print("     3-PANEL SCIENCE DATA REDUCTION DASHBOARD INITIALIZED          ")
print("==================================================================")

try:
    # 2. Concurrently load all 3 dataset files using safe context managers
    with fits.open(file_science) as h_sci, fits.open(file_background) as h_bg, fits.open(file_reduced) as h_red:
        # Pull Frame 1 (Index 0) from each 3D time-series data cube
        raw_frame = h_sci["STARRY_SKY_CUBE"].data[0]
        bg_frame  = h_bg["BACKGROUND_MESH"].data[0]
        red_frame = h_red["REDUCED_SCIENCE"].data[0]

    print("[SUCCESS] All 3 data extension layers loaded simultaneously.")
    print(f" Matrix Resolutions: {raw_frame.shape} pixels | 3 Channels Active.")
    print("-" * 72)

    # 3. Create a clean 3-panel side-by-side canvas layout frame
    fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(18, 6))
    plt.subplots_adjust(wspace=0.08)

    # Enforce standard uniform color scaling maps to track structural differences
    vmin_val = 0.00
    vmax_val = 0.15 # Clipped lower to clearly highlight the dark ambient fog profile

    # PANEL 1: Raw Science Image (Stars drowned inside ambient background glow)
    im1 = ax1.imshow(raw_frame, cmap="inferno", origin="lower", vmin=vmin_val, vmax=vmax_val)
    ax1.set_title("Panel A: Raw Input Science\n(Stars + Ambient Sky Noise)", fontsize=11, weight="bold")
    ax1.set_xlabel("X (pixels)")
    ax1.set_ylabel("Y (pixels)")
    ax1.grid(color="white", linestyle=":", alpha=0.15)

    # PANEL 2: Isolated Background Mesh Map (Ignored stars, pure isolated diffuse noise)
    im2 = ax2.imshow(bg_frame, cmap="inferno", origin="lower", vmin=vmin_val, vmax=vmax_val)
    ax2.set_title("Panel B: Estimated 2D Background\n(Bilinear 32x32 Median Mesh)", fontsize=11, weight="bold")
    ax2.set_xlabel("X (pixels)")
    ax2.get_yaxis().set_visible(False) # Hide Y labels to prevent overlap text cramping
    ax2.grid(color="white", linestyle=":", alpha=0.15)

    # PANEL 3: Flattened Reduced Science Image (Background completely scrubbed flat to 0)
    im3 = ax3.imshow(red_frame, cmap="inferno", origin="lower", vmin=vmin_val, vmax=vmax_val)
    ax3.set_title("Panel C: Reduced Flat Science\n(Background Subtracted Array)", fontsize=11, weight="bold")
    ax3.set_xlabel("X (pixels)")
    ax3.get_yaxis().set_visible(False)
    ax3.grid(color="white", linestyle=":", alpha=0.15)

    # 4. Append a shared centralized color mapping scale guide indicator on the right edge
    cbar = fig.colorbar(im3, ax=[ax1, ax2, ax3], pad=0.01, aspect=22)
    cbar.set_label("Flux Density Scale Intensity (Jansky / Pixel Matrix)", rotation=270, labelpad=15)

    # 5. Save dashboard graphic to your Arch Linux storage drive sector
    output_plot = "science_reduction_dashboard.png"
    plt.savefig(output_plot, dpi=300, bbox_inches="tight")
    plt.show()

    print(f"[SUCCESS] Science data reduction dashboard saved to: {output_plot}\n")

except FileNotFoundError as e:
    print(f"\n[ERROR] Missing required file tracks. Ensure all C binaries ran successfully:\n{str(e)}")
except Exception as e:
    print(f"\n[CRITICAL ERROR] Visual matrix rendering loop crashed: {str(e)}")
