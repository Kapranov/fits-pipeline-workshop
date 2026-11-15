import os
import matplotlib.pyplot as plt
from astropy.io import fits

# Define file paths matching your architecture
master_file = "stage_25_master_observatory_dataset.fits"
blurred_file = "stage_31_blurred_sky_slice.fits"

print("==================================================================")
print("     ASTRONOMICAL IMAGE CONVOLUTION SIDE-BY-SIDE VISUALIZER      ")
print("==================================================================")

# Ensure both processing source assets exist on disk before plotting
for f in [master_file, blurred_file]:
    if not os.path.exists(f):
        print(f"[ERROR] Required file '{f}' not found.")
        print("Please ensure your prior C pipeline stages were executed!")
        exit(1)

# 1. Open the master container to extract the raw Frame 1 slice
with fits.open(master_file) as hdul:
    raw_frame_1 = hdul["STARRY_SKY_CUBE"].data[0] # NumPy shape: (Frame, Y, X)

# 2. Open the blurred standalone 2D canvas file
with fits.open(blurred_file) as hdul:
    blurred_frame = hdul["BLURRED_IMAGE"].data

# 3. Define the sub-section bounding box around your brightest star center (X=28, Y=48)
# We crop a 30x30 bounding box window to check the blurring detail cleanly
crop_radius = 15
center_x, center_y = 28, 48

# Convert 1-indexed FITS coordinates to 0-indexed slice array selections
x_start, x_end = (center_x - 1) - crop_radius, (center_x - 1) + crop_radius
y_start, y_end = (center_y - 1) - crop_radius, (center_y - 1) + crop_radius

raw_crop = raw_frame_1[y_start:y_end, x_start:x_end]
blurred_crop = blurred_frame[y_start:y_end, x_start:x_end]

# 4. Generate the comparative visual panel canvas
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 6))

# Standardize brightness min/max scaling across subplots for exact alignment checks
vmin_val, vmax_val = 0.0, 1.8

# Subplot panel A: Raw Simulation Star Footprint
im1 = ax1.imshow(raw_crop, cmap="inferno", origin="lower", vmin=vmin_val, vmax=vmax_val)
ax1.set_title("Panel A: Raw Unfiltered Star Core\n(Pixel-Isolated Core Focus)", fontsize=11, weight="bold")
ax1.set_xlabel("Relative X Coordinates Offset")
ax1.set_ylabel("Relative Y Coordinates Offset")
ax1.grid(color="white", linestyle=":", alpha=0.3)

# Subplot panel B: Convolved Smearing PSF Footprint
im2 = ax2.imshow(blurred_crop, cmap="inferno", origin="lower", vmin=vmin_val, vmax=vmax_val)
ax2.set_title("Panel B: Convolved Gaussian Blurred Profile\n(Simulated Optical PSF Scattering Halo)", fontsize=11, weight="bold")
ax2.set_xlabel("Relative X Coordinates Offset")
ax2.get_yaxis().set_visible(False) # Hide secondary Y text to clear up layout spacing
ax2.grid(color="white", linestyle=":", alpha=0.3)

# 5. Append a shared analytical intensity color bar layout scale
cbar_ax = fig.add_axes([0.92, 0.15, 0.02, 0.7])
cbar = fig.colorbar(im2, cax=cbar_ax)
cbar.set_label("Stellar Pixel Intensity Value (Arbitrary Flux Units)", rotation=270, labelpad=15)

plt.suptitle("Observatory Verification Metrics: 2D Spatial Kernel Convolution Comparison", fontsize=13, weight="bold", y=0.98)

# 6. Save vector snapshot and display canvas
output_plot = "convolution_blur_comparison.png"
plt.savefig(output_plot, dpi=300, bbox_inches="tight")
plt.show()

print(f"[SUCCESS] Side-by-side comparison chart saved to: {output_plot}\n")
