import os
import matplotlib.pyplot as plt
from astropy.io import fits

filename = "stage_41_stacked_star_trails.fits"

print("==================================================================")
print("     2D CO-ADDITION INTEGRATED STAR TRAIL VISUALIZER              ")
print("==================================================================")

# 1. Verify file presence before initializing plotting canvas
if not os.path.exists(filename):
    print(f"[ERROR] File '{filename}' not found. Please compile and run stage_41_stacking.c first.")
    exit(1)

try:
    # 2. Open the 2D stacked FITS container safely
    with fits.open(filename) as hdul:
        # Load the flat 2D science image array matrix
        stacked_image = hdul["STAR_TRAILS"].data

    print("[SUCCESS] Successfully loaded the integrated 2D extension layer.")
    print(f" Science Canvas Resolution: {stacked_image.shape} pixels")
    print("-" * 66)

    # 3. Create a dark canvas for plotting the star trails
    fig, ax = plt.subplots(figsize=(9, 9))
    fig.patch.set_facecolor('black')
    ax.set_facecolor('black')

    # Enforce color tracking boundaries.
    # vmin right at 0.0, vmax set to 1.0 to isolate trailing wings clearly.
    im = ax.imshow(stacked_image, cmap="inferno", origin="lower", vmin=0.00, vmax=1.00)

    # 4. Apply professional layout formatting and labeling
    ax.set_title("Astronomical Co-Addition Stacking: Field Rotation Star Trails\n(Unguided Alt-Azimuth Long Exposure Simulation)",
                 color="white", fontsize=12, weight="bold", pad=15)
    ax.set_xlabel("X Axis Pixel Coordinate Reference", color="white", fontsize=10, labelpad=8)
    ax.set_ylabel("Y Axis Pixel Coordinate Reference", color="white", fontsize=10, labelpad=8)

    # Style ticks to look clean on the dark interface
    ax.tick_params(colors='white', which='both')
    ax.grid(color="white", linestyle=":", alpha=0.15)

    # Append a single color bar tracking indicator mapping scale to the right edge
    cbar = fig.colorbar(im, ax=ax, pad=0.02, aspect=20)
    cbar.set_label("Mean Time-Integrated Flux Density (Jansky)", color="white", rotation=270, labelpad=18)
    cbar.ax.tick_params(labelcolor='white')

    # 5. Save chart image summary profile to disk
    output_filename = "coaddition_star_trails.png"
    plt.savefig(output_filename, dpi=300, bbox_inches="tight", facecolor='black')
    plt.show()

    print(f"[SUCCESS] Final star trails tracking map saved to: {output_filename}\n")

except Exception as e:
    print(f"\n[CRITICAL ERROR] Failed to complete imshow rendering loop: {str(e)}")
