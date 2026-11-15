import numpy as np
import matplotlib.pyplot as plt
from astropy.io import fits

# 1. Open the generated SNR catalog extension layer
with fits.open("stage_22_all_stars_snr_catalog.fits") as hdul:
    table_data = hdul["SNR_CATALOG"].data

# 2. Print out a quick data summary sorted by the highest SNR score in Frame 1
frame1_data = table_data[table_data["FRAME"] == 1]
sorted_indices = np.argsort(frame1_data["SNR"])[::-1]

print("=========================================================")
print("      EXTRACTED FITS SNR ANALYSIS BATCH REPORT           ")
print("=========================================================")
print(" Rank | Star ID |  X (pix)  |  Y (pix)  |   Flux   |    SNR   ")
print("---------------------------------------------------------")
for rank, idx in enumerate(sorted_indices[:10], 1):
    row = frame1_data[idx]
    print(f"  {rank:02d}  |   {row['STAR_ID']:02d}    |  {row['X_CENTER']:7.2f}  |  {row['Y_CENTER']:7.2f}  |  {row['NET_FLUX']:6.4f}  |  {row['SNR']:7.2f}")
print("=========================================================\n")

# 3. Choose the absolute highest-SNR star to inspect across all frames
best_star_id = frame1_data[sorted_indices[0]]["STAR_ID"]
star_time_series = table_data[table_data["STAR_ID"] == best_star_id]
sort_order = np.argsort(star_time_series["FRAME"])

frames = star_time_series["FRAME"][sort_order]
fluxes = star_time_series["NET_FLUX"][sort_order]
snrs = star_time_series["SNR"][sort_order]

# 4. Generate a comparative dual-axis chart tracking Flux vs SNR
fig, ax1 = plt.subplots(figsize=(10, 5))

color = 'tab:red'
ax1.set_xlabel('Time Frame Sequence Index', labelpad=10)
ax1.set_ylabel('Net Flux (Jansky)', color=color)
ax1.plot(frames, fluxes, color=color, marker='o', linewidth=2, label="Net Star Flux")
ax1.tick_params(axis='y', labelcolor=color)
ax1.set_xticks(frames)
ax1.grid(color='gray', linestyle=':', alpha=0.5)

ax2 = ax1.twinx()
color = 'tab:blue'
ax2.set_ylabel('Calculated Signal-to-Noise Ratio (SNR)', color=color)
ax2.plot(frames, snrs, color=color, marker='s', linestyle='--', linewidth=2, label="SNR Score")
ax2.tick_params(axis='y', labelcolor=color)

plt.title(f"Performance Tracking Dashboard: Net Flux vs SNR (Star ID {best_star_id:02d})", fontsize=13, pad=15)
fig.tight_layout()
plt.savefig("star_snr_performance.png", dpi=300)
plt.show()

print("Success! Performance analysis graphic saved to star_snr_performance.png")
