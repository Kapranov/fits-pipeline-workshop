import numpy as np
import matplotlib.pyplot as plt
from astropy.io import fits

filename = "stage_37_subpixel_precision_catalog.fits"

print("==================================================================")
print("     SUB-PIXEL SPACE-TIME STELLAR DRIFT PATH TRACKER INITIALIZED ")
print("==================================================================")

try:
    # 1. Open the sub-pixel high-precision FITS catalog
    with fits.open(filename) as hdul:
        table_data = hdul["SUBPIXEL_CATALOG"].data

    print(f"[SUCCESS] Loaded {len(table_data)} high-precision records.")
    print("-" * 66)

    # 2. Group records by unique Star ID
    unique_stars = np.unique(table_data["STAR_ID"])

    # 3. Create a clean visual canvas for plotting the paths
    plt.figure(figsize=(10, 8))

    # Define a distinct color loop palette mapping for the tracks
    colors = plt.cm.jet(np.linspace(0, 1, min(len(unique_stars), 8)))

    print("Tracing paths for stars with complete 5-frame tracking logs...")
    tracked_count = 0

    # 4. Loop through the stars and trace their sub-pixel movements chronological order
    for star_id in unique_stars:
        # Extract row elements for this specific star
        star_rows = table_data[table_data["STAR_ID"] == star_id]

        # We look for stars with 5 full frames recorded to show the complete path
        if len(star_rows) == 5:
            sort_order = np.argsort(star_rows["FRAME"])
            x_drift = star_rows["X_CENTROID"][sort_order]
            y_drift = star_rows["Y_CENTROID"][sort_order]
            frames = star_rows["FRAME"][sort_order]

            # Select color and plot the continuous drift trajectory vector
            c = colors[tracked_count % len(colors)]
            plt.plot(x_drift, y_drift, linestyle="-", marker="o", color=c, alpha=0.7,
                     linewidth=1.5, markersize=5, label=f"Star ID {star_id:02d}")

            # Highlight the starting position (Frame 1) with an open circle ring anchor marker
            plt.scatter(x_drift[0], y_drift[0], facecolors='none', edgecolors=c,
                        s=130, linewidths=1.5, zorder=3)

            # Label the chronological progression order numbers directly on the plot graph nodes
            for idx, f in enumerate(frames):
                plt.text(x_drift[idx], y_drift[idx], f" F{f}", color=c, fontsize=9, weight="bold")

            tracked_count += 1
            if tracked_count >= 6: # Limit overlay paths to keep chart legible and clean
                break

    # 5. Apply advanced layout scaling and custom grid configurations
    plt.title("Astronomical Sub-Pixel Tracking: Time-Series Space Drift Path\n(Atmospheric Jitter Simulation Model)",
              fontsize=13, weight="bold", pad=15)
    plt.xlabel("Fractional X Center Coordinate (pixels)", fontsize=11, labelpad=8)
    plt.ylabel("Fractional Y Center Coordinate (pixels)", fontsize=11, labelpad=8)

    # Enable fine minor grids to visualize micro-pixel changes clearly
    plt.grid(color="gray", linestyle=":", alpha=0.5, which="both")
    plt.minorticks_on()

    plt.legend(loc="upper right", frameon=True, shadow=True, title="Stellar Target IDs")

    # 6. Save vector image layout onto disk storage sector
    output_filename = "subpixel_drift_trajectories.png"
    plt.savefig(output_filename, dpi=300, bbox_inches="tight")
    plt.show()

    print(f"\n[SUCCESS] Sub-pixel tracking drift paths saved to: {output_filename}\n")

except FileNotFoundError:
    print(f"\n[ERROR] File '{filename}' not found. Please verify your Stage 29 C script has executed successfully.")
except Exception as e:
    print(f"\n[CRITICAL ERROR] Failed to trace sub-pixel coordinate vectors: {str(e)}")
