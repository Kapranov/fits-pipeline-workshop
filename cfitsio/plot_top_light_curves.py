import numpy as np
import matplotlib.pyplot as plt
from astropy.io import fits

# 1. Open the batch light curves FITS table
filename = "stage_20_all_stars_light_curves.fits"

print("====================================================")
print("     EXTRACTING TOP 5 BRIGHTEST LIGHT CURVES        ")
print("====================================================")

with fits.open(filename) as hdul:
    table_data = hdul["LIGHT_CURVES"].data

# 2. Group the data by Star ID to compute their average brightness
unique_star_ids = np.unique(table_data["STAR_ID"])
star_averages = []

for star_id in unique_star_ids:
    # Filter rows matching this specific star
    star_rows = table_data[table_data["STAR_ID"] == star_id]
    # Calculate the mean flux value across all available frames
    mean_flux = np.mean(star_rows["NET_FLUX"])
    star_averages.append((star_id, mean_flux))

# 3. Sort stars by flux in descending order and isolate the top 5
# lambda x: x[1] sorts by the mean_flux value computed above
star_averages.sort(key=lambda x: x[1], reverse=True)
top_5_stars = star_averages[:5]

print("\nIdentified Top 5 Brightest Stars:")
for rank, (star_id, avg_flux) in enumerate(top_5_stars, 1):
    print(f" Rank {rank}: Star ID {star_id:02d} | Average Flux: {avg_flux:.4f} Jansky")

# 4. Set up the plotting canvas
plt.figure(figsize=(12, 6))

# A list of distinct styles so lines are visually clean and easy to separate
styles = [
    {'color': '#d32f2f', 'marker': 'o', 'ls': '-'},   # Red
    {'color': '#1976d2', 'marker': 's', 'ls': '--'},  # Blue
    {'color': '#388e3c', 'marker': 'D', 'ls': '-.'},  # Green
    {'color': '#fbc02d', 'marker': '^', 'ls': ':'},   # Yellow/Gold
    {'color': '#7b1fa2', 'marker': 'v', 'ls': '-'}    # Purple
]

# 5. Loop through the top 5 entries and overlay them onto the graph canvas
for idx, (star_id, _) in enumerate(top_5_stars):
    # Extract the subset data for this specific star rank
    star_data = table_data[table_data["STAR_ID"] == star_id]

    # Sort by frame index to ensure the line vectors connect in chronological order
    sort_idx = np.argsort(star_data["FRAME"])
    frames = star_data["FRAME"][sort_idx]
    fluxes = star_data["NET_FLUX"][sort_idx]

    # Grab the location coordinates from the first entry to add context to the legend label
    x_pos = star_data["X_CENTER"][0]
    y_pos = star_data["Y_CENTER"][0]

    # Plot line overlay
    plt.plot(
        frames, fluxes,
        label=f"Star ID {star_id:02d} ({x_pos:.1f}, {y_pos:.1f})",
        color=styles[idx]['color'],
        marker=styles[idx]['marker'],
        linestyle=styles[idx]['ls'],
        linewidth=2.5,
        markersize=7
    )

# 6. Apply professional layout formatting
plt.title("Astronomical Batch Photometry: Top 5 Brightest Injected Light Curves Overlay", fontsize=14, pad=15)
plt.xlabel("Observational Time Frame Index", fontsize=12, labelpad=10)
plt.ylabel("Integrated Net Flux Density (Jansky)", fontsize=12, labelpad=10)

plt.xticks(np.arange(1, 6)) # Distinct ticks for frames 1 to 5
plt.grid(color='gray', linestyle=':', alpha=0.5)

# Place the legend in a clear floating frame layout next to the lines
plt.legend(loc="upper right", frameon=True, shadow=True, title="Target Stars Map")

# 7. Save vector summary output onto your Arch Linux drive storage sector
output_plot = "top_5_light_curves.png"
plt.savefig(output_plot, dpi=300, bbox_inches="tight")
plt.show()

print(f"\nSuccess! Comparative light curves chart saved to {output_plot}")

