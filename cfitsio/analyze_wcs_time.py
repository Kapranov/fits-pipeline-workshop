import numpy as np
import matplotlib.pyplot as plt
from astropy.io import fits
from astropy.timeseries import LombScargle

# 1. Open the 3D data cube file container
filename = "stage_17_time_cube.fits"

print("====================================================")
print("   WCS-TIME AUTOMATED ANALYSIS ENGINE INITIALIZED    ")
print("====================================================")

with fits.open(filename) as hdul:
    image_hdu = hdul["STARRY_SKY_CUBE"]
    cube_data = image_hdu.data
    header = image_hdu.header

num_frames, height, width = cube_data.shape

# 2. DYNAMICALLY EXTRACT WCS-TIME METADATA FROM HEADERS (The core upgrade!)
# If the keywords aren't present in the header, fall back to safe defaults (0.0s origin, 2.0s delta)
crval3 = header.get("CRVAL3", 0.0)   # Reference observation start time value
crpix3 = header.get("CRPIX3", 1.0)   # Reference pixel slice frame index
cdelt3 = header.get("CDELT3", 2.0)   # Time step cadency delta per slice segment
time_unit = header.get("CUNIT3", "s") # Unit of measurement label

print(f" Detected Axis 3 Type  : {header.get('CTYPE3', 'UNKNOWN')}")
print(f" Time Origin (CRVAL3)  : {crval3} {time_unit}")
print(f" Cadence Delta (CDELT3): {cdelt3} {time_unit} per frame layer slice")

# Calculate the physical elapsed time array dynamically using WCS metadata:
# Formula: Time = CRVAL3 + CDELT3 * (Frame_Index - CRPIX3)
frames = np.arange(1, num_frames + 1)
time_vector = crval3 + cdelt3 * (frames - crpix3)
print(f" Computed Timeline Map : {list(time_vector)} (in {time_unit})")
print("----------------------------------------------------")

# 3. Locate all star centers dynamically on Frame 1 using a baseline local maxima search
frame1 = cube_data[0]
star_threshold = 0.5
star_positions = []

# Skipping outer edges to protect loop array bounds checks
for y in range(1, height - 1):
    for x in range(1, width - 1):
        val = frame1[y, x]
        if val >= star_threshold:
            if (val >= frame1[y, x-1] and val >= frame1[y, x+1] and
                val >= frame1[y-1, x] and val >= frame1[y+1, x] and
                val >= frame1[y-1, x-1] and val >= frame1[y-1, x+1] and
                val >= frame1[y+1, x-1] and val >= frame1[y+1, x+1]):
                star_positions.append((x, y))

# 4. Perform Batch Aperture Photometry geometry math over all frames
r_aperture = 4.0
r_sky_in = 7.0
r_sky_out = 12.0

y_indices, x_indices = np.ogrid[:height, :width]
star_light_curves = {}

for star_idx, (star_x, star_y) in enumerate(star_positions):
    distances = np.sqrt((x_indices - star_x)**2 + (y_indices - star_y)**2)
    aperture_mask = distances <= r_aperture
    sky_mask = (distances >= r_sky_in) & (distances <= r_sky_out)
    ap_pixel_count = np.sum(aperture_mask)

    flux_profile = []
    for f in range(num_frames):
        frame_pixels = cube_data[f]
        raw_ap_flux = np.sum(frame_pixels[aperture_mask])
        sky_values = frame_pixels[sky_mask]
        sky_background_median = np.median(sky_values) if len(sky_values) > 0 else 0.0

        net_flux = raw_ap_flux - (ap_pixel_count * sky_background_median)
        flux_profile.append(net_flux)

    star_light_curves[star_idx] = {
        'coords': (star_x + 1, star_y + 1),
        'flux': np.array(flux_profile),
        'mean_flux': np.mean(flux_profile)
    }

# 5. Sort objects to identify the top 5 brightest profiles and locate the absolute brightest
sorted_stars = sorted(star_light_curves.items(), key=lambda item: item[1]['mean_flux'], reverse=True)
top_5_stars_keys = [item[0] for item in sorted_stars[:5]]
brightest_star_idx = top_5_stars_keys[0]

brightest_star_coords = star_light_curves[brightest_star_idx]['coords']
brightest_star_flux = star_light_curves[brightest_star_idx]['flux']

print(f" Brightest Object Located: Star ID {brightest_star_idx:02d} at Coordinates ({brightest_star_coords[0]}, {brightest_star_coords[1]})")

# 6. Compute Lomb-Scargle Periodogram frequency spectrum for the brightest star
# The maximum testable resolution frequency limit (Nyquist frequency) is 1.0 / (2 * cdelt3)
nyquist_frequency = 1.0 / (2.0 * cdelt3)
frequency_grid = np.linspace(0.01, nyquist_frequency, 1000)
power_distribution = LombScargle(time_vector, brightest_star_flux).power(frequency_grid)

peak_power_idx = np.argmax(power_distribution)
dominant_frequency = frequency_grid[peak_power_idx]
dominant_period = 1.0 / dominant_frequency

print(f" Dominant Period Frequency : {dominant_frequency:.4f} Hz (Period: {dominant_period:.2f} {time_unit})")
print("----------------------------------------------------")

# 7. Render the Final Multi-Panel Dashboard Visualization Chart
fig = plt.figure(figsize=(12, 10))
plt.subplots_adjust(hspace=0.4)

# Panel A: Top 5 Overlaid Light Curves Plot
ax1 = plt.subplot(2, 1, 1)
for idx in top_5_stars_keys:
    star_info = star_light_curves[idx]
    ax1.plot(time_vector, star_info['flux'], marker='o', linewidth=2,
             label=f"Star {idx:02d} @ ({star_info['coords'][0]}, {star_info['coords'][1]})")

ax1.set_title("Panel A: WCS-TIME Adaptive Light Curves Overlay (Top 5 Brightest)", fontsize=12, weight='bold', pad=10)
ax1.set_xlabel(f"Observational Time Vector Elapsed ({time_unit})", fontsize=10)
ax1.set_ylabel("Integrated Net Flux Intensity (Jansky)", fontsize=10)
ax1.set_xticks(time_vector)
ax1.grid(color='gray', linestyle=':', alpha=0.5)
ax1.legend(loc="upper right", title="Target Space Catalogs")

# Panel B: Frequency-Domain Periodogram Spectrum Plot
ax2 = plt.subplot(2, 1, 2)
ax2.plot(frequency_grid, power_distribution, color='teal', linewidth=2, label="Lomb-Scargle Power Spectrum")
ax2.axvline(dominant_frequency, color='crimson', linestyle='--', alpha=0.8,
            label=f"Peak Frequency Vector ({dominant_frequency:.3f} Hz)")
ax2.set_title(f"Panel B: Lomb-Scargle Periodogram Spectrum Analysis (Object ID {brightest_star_idx:02d})", fontsize=12, weight='bold', pad=10)
ax2.set_xlabel("Frequency Vector Resolution Scale (Hz / Cycles per Second)", fontsize=10)
ax2.set_ylabel("Spectral Power Signal Strength", fontsize=10)
ax2.grid(color='gray', linestyle=':', alpha=0.5)
ax2.legend(loc="upper right")

# 8. Save the final multi-panel dashboard graphic file onto disk
output_plot = "wcs_time_analysis_dashboard.png"
plt.savefig(output_plot, dpi=300, bbox_inches="tight")
plt.show()

print(f"SUCCESS! Dashboard compiled and graphics vector saved to {output_plot}\n")
