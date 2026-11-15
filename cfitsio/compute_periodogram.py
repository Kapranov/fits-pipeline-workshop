import numpy as np
import matplotlib.pyplot as plt
from astropy.io import fits
from astropy.timeseries import LombScargle

# 1. Load the batch data table from disk storage
filename = "stage_20_all_stars_light_curves.fits"
with fits.open(filename) as hdul:
    table_data = hdul["LIGHT_CURVES"].data

# 2. Automatically find the brightest star ID to test (Star ID 47 from the previous step)
unique_star_ids = np.unique(table_data["STAR_ID"])
best_star_id = None
max_mean_flux = -1.0

for star_id in unique_star_ids:
    star_rows = table_data[table_data["STAR_ID"] == star_id]
    mean_flux = np.mean(star_rows["NET_FLUX"])
    if mean_flux > max_mean_flux:
        max_mean_flux = mean_flux
        best_star_id = star_id

print("====================================================")
print("        CELESTIAL PERIODOGRAM COMPUTATION ENGINE     ")
print("====================================================")
print(f" Target Object Isolated : Star ID {best_star_id:02d}")
print(f" Baseline Average Flux  : {max_mean_flux:.4f} Jansky")

# 3. Extract the time-domain metrics for this target star
star_data = table_data[table_data["STAR_ID"] == best_star_id]
sort_idx = np.argsort(star_data["FRAME"])

# Assume an observational cadency grid (e.g., 1 frame exposure taken every 2.0 seconds)
time_steps = star_data["FRAME"][sort_idx] * 2.0  # Time vector in seconds
flux_signal = star_data["NET_FLUX"][sort_idx]    # Flux signal array

# 4. Compute the Lomb-Scargle Periodogram spectrum
# We evaluate test frequencies ranging from 0.05 Hz up to a maximum Nyquist frequency of 0.25 Hz
frequency_grid = np.linspace(0.05, 0.25, 500)
power_distribution = LombScargle(time_steps, flux_signal).power(frequency_grid)

# Locate the absolute peak frequency component
peak_idx = np.argmax(power_distribution)
dominant_frequency = frequency_grid[peak_idx]
dominant_period = 1.0 / dominant_frequency

print(f" Dominant Twinkle Freq : {dominant_frequency:.4f} Hz")
print(f" Equivalent Peak Period: {dominant_period:.2f} seconds")
print("----------------------------------------------------")

# 5. Render a professional dual-panel analysis dashboard chart
fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(11, 8))
plt.subplots_adjust(hspace=0.35)

# Panel 1: Time-Domain Light Curve Track
ax1.plot(time_steps, flux_signal, color='darkviolet', marker='o', linestyle='-', linewidth=2, label="Observed Signal")
ax1.set_title(f"Panel A: Time-Domain Light Curve (Star ID {best_star_id:02d})", fontsize=12, weight='bold')
ax1.set_xlabel("Observational Time Vector (seconds)", fontsize=10)
ax1.set_ylabel("Net Flux Intensity (Jy)", fontsize=10)
ax1.grid(color='gray', linestyle=':', alpha=0.5)
ax1.legend(loc="upper right")

# Panel 2: Frequency-Domain Periodogram Spectrum
ax2.plot(frequency_grid, power_distribution, color='teal', linewidth=2, label="Lomb-Scargle Power Spectrum")
ax2.axvline(dominant_frequency, color='crimson', linestyle='--', alpha=0.7,
            label=f"Peak Frequency ({dominant_frequency:.3f} Hz)")
ax2.set_title("Panel B: Frequency-Domain Periodogram Spectrum Analysis", fontsize=12, weight='bold')
ax2.set_xlabel("Frequency Vector (Hz / Cycles per Second)", fontsize=10)
ax2.set_ylabel("Spectral Power Intensity", fontsize=10)
ax2.grid(color='gray', linestyle=':', alpha=0.5)
ax2.legend(loc="upper right")

# 6. Finalize layout, save vector image, and display canvas
output_plot = "star_periodogram_analysis.png"
plt.savefig(output_plot, dpi=300, bbox_inches="tight")
plt.show()

print(f"Success! Periodogram dashboard graphic saved to {output_plot}\n")
