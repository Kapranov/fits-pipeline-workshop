import numpy as np
import matplotlib.pyplot as plt
from astropy.io import fits

# 1. Configuration parameters matching your C engine setup
filename = "stage_17_time_cube.fits"

print("====================================================")
print("     AUTOMATED APERTURE PHOTOMETRY LIGHT CURVE      ")
print("====================================================")

# Request star coordinates interactively
target_x = float(input("Enter target star center X (e.g. 28): "))
target_y = float(input("Enter target star center Y (e.g. 48): "))

r_aperture = 4.0   # Radius of inner star circle
r_sky_in = 7.0     # Inner background ring radius
r_sky_out = 12.0   # Outer background ring radius

# Convert 1-indexed user coordinates to 0-indexed Python array indices
c_x = target_x - 1.0
c_y = target_y - 1.0

# 2. Open the FITS data cube
with fits.open(filename) as hdul:
    # Target the STARRY_SKY_CUBE extension layer
    # FITS shape in Python/NumPy is structured as: (Frames, Height, Width)
    cube_data = hdul["STARRY_SKY_CUBE"].data

num_frames, height, width = cube_data.shape

# 3. Create a coordinate grid mapping matrices to calculate distances
y_grid, x_grid = np.ogrid[:height, :width]
distances = np.sqrt((x_grid - c_x)**2 + (y_grid - c_y)**2)

# Generate geometric boolean masks for the aperture and the sky background ring
aperture_mask = distances <= r_aperture
sky_mask = (distances >= r_sky_in) & (distances <= r_sky_out)

ap_pixel_count = np.sum(aperture_mask)

# 4. Perform Aperture Photometry loop across all frames sequentially
net_flux_profile = []
frames = np.arange(1, num_frames + 1)

for f in range(num_frames):
    frame_pixels = cube_data[f]

    # Extract calculations using our geometric masks
    raw_ap_flux = np.sum(frame_pixels[aperture_mask])
    sky_values = frame_pixels[sky_mask]

    # Compute median background sky noise to avoid outliers/cosmic rays
    sky_background_level = np.median(sky_values) if len(sky_values) > 0 else 0.0

    # Subtract background glow from raw data footprint
    net_star_flux = raw_ap_flux - (ap_pixel_count * sky_background_level)
    net_flux_profile.append(net_star_flux)

    print(f" Frame {f+1} -> Integrated Net Star Flux: {net_star_flux:.4f} Jansky")

# 5. Render the Stellar Light Curve Chart Plot
plt.figure(figsize=(10, 5))
plt.plot(frames, net_flux_profile, color='crimson', marker='o', linestyle='-', linewidth=2, markersize=8, label=f'Star at ({target_x}, {target_y})')

# Emphasize individual points with filled scatter plot anchors
plt.scatter(frames, net_flux_profile, color='darkred', zorder=3)

# 6. Customize axes styling and formatting parameters
plt.title(f"Astronomical Light Curve: Atmospheric Twinkling Time-Series Variance", fontsize=13, pad=15)
plt.xlabel("Time Frame Sequence Index (Observational Steps)", fontsize=11, labelpad=10)
plt.ylabel("Integrated Net Flux Intensity (Jansky)", fontsize=11, labelpad=10)

plt.xticks(frames)
plt.grid(color='gray', linestyle=':', alpha=0.6)
plt.legend(loc="upper right", frameon=True, shadow=True)

# 7. Save vector summary output onto disk storage layout
output_plot = "stellar_light_curve.png"
plt.savefig(output_plot, dpi=300, bbox_inches="tight")
plt.show()

print(f"\nSuccess! Light curve chart visualization saved to {output_plot}")
