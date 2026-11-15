import matplotlib.pyplot as plt
from astropy.io import fits

# 1. Open your generated summary FITS file container
with fits.open("stage_18_cube_statistics_summary.fits") as hdul:
    # Target the FRAME_STATS extension layer and extract the row data
    table_data = hdul["FRAME_STATS"].data

# 2. Extract specific data vectors for graphing
frames = table_data["FRAME"]
bg_mean = table_data["BG_MEAN"]
max_flux = table_data["MAX_FLUX"]

# 3. Initialize a clean canvas for charting
fig, ax1 = plt.subplots(figsize=(10, 6))

# 4. Plot the Max Peak Flux metrics on the primary Y-axis (Left)
color = 'tab:orange'
ax1.set_xlabel('Time Frame Sequence Index', fontsize=12, labelpad=10)
ax1.set_ylabel('Peak Flux Intensity (Jansky)', color=color, fontsize=12)
line1 = ax1.plot(frames, max_flux, color=color, marker='o', linewidth=2, markersize=8, label='Max Star Core Flux')
ax1.tick_params(axis='y', labelcolor=color)
ax1.set_xticks(frames) # Ensure distinct ticks for frames 1-5

# 5. Create a shared secondary Y-axis (Right) to track subtle Background deviations
ax2 = ax1.twinx()
color = 'tab:blue'
ax2.set_ylabel('Global Average Background Glow', color=color, fontsize=12)
line2 = ax2.plot(frames, bg_mean, color=color, marker='s', linestyle='--', linewidth=2, markersize=8, label='BG Mean Noise')
ax2.tick_params(axis='y', labelcolor=color)

# Adjust right-axis range boundaries dynamically to keep subtle noise changes visible
ax2.set_ylim(min(bg_mean) * 0.99, max(bg_mean) * 1.01)

# 6. Build a unified legend chart anchor box
lines = line1 + line2
labels = [l.get_label() for l in lines]
ax1.legend(lines, labels, loc='upper right', frameon=True, shadow=True)

# 7. Add grids and professional chart labels
plt.title('3D Time-Series Data Cube: Stellar Twinkling Fluctuations Analysis', fontsize=14, pad=15)
ax1.grid(color='gray', linestyle=':', alpha=0.5)

# 8. Save the chart as a vector graphic and render to display layer
plt.savefig("cube_fluctuations_analysis.png", dpi=300, bbox_inches="tight")
plt.show()

print("Success! Summary visualization saved to cube_fluctuations_analysis.png")
