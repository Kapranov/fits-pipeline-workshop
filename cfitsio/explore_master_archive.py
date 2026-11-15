import numpy as np
import matplotlib.pyplot as plt
from astropy.io import fits
from astropy.wcs import WCS

# Make sure this matches your exact master filename!
filename = "stage_25_master_observatory_dataset.fits"

print("==================================================================")
print("     MASTER OBSERVATORY ARCHIVE EXTENSION EXPLORER INITIALIZED   ")
print("==================================================================")

try:
    # 1. Open the multi-extension master archive container file
    with fits.open(filename) as hdul:
        # Display the complete structural layout summary of the file
        print("\n[INFO] Detecting Internal Multi-Extension HDU Structure:")
        hdul.info()
        print("-" * 66)

        # 2. LOAD BOTH EXTENSION LAYERS INSIDE THE OPEN FILE CONTEXT
        image_hdu = hdul["STARRY_SKY_CUBE"]
        table_data = hdul["CLEAN_CATALOG"].data

        # 3. Read image metrics and extract native WCS-TIME axis parameters
        cube_data = image_hdu.data
        header = image_hdu.header
        num_frames, height, width = cube_data.shape

        crval3 = header.get("CRVAL3", 0.0)
        crpix3 = header.get("CRPIX3", 1.0)
        cdelt3 = header.get("CDELT3", 2.0)
        time_unit = header.get("CUNIT3", "s")

        # Generate the physical time step tracker array using WCS header definitions
        time_vector = crval3 + cdelt3 * (np.arange(1, num_frames + 1) - crpix3)

        # Read the spatial WCS metadata mapping profile to anchor RA/DEC axes coordinates
        spatial_wcs = WCS(header, naxis=2) # Map the first 2 spatial dimensions

        print(f"[DATA] Image Cube Grid Slices Found : {num_frames} frames ({width}x{height} pixels)")
        print(f"[DATA] WCS adaptive cadence rhythm : Each frame segment = {cdelt3} {time_unit} steps")
        print(f"[DATA] Verified catalog table size  : {len(table_data)} rows loaded simultaneously.")
        print("-" * 66)

        # 4. Generate a Multi-Panel Dashboard Visualizer Plot Frame
        fig = plt.figure(figsize=(14, 10))
        plt.subplots_adjust(wspace=0.25, hspace=0.35)

        # PANEL 1 (Left side): Render Spatial Plane Frame 1 using coordinate projection mappings
        ax1 = fig.add_subplot(2, 2, 1, projection=spatial_wcs)
        im = ax1.imshow(cube_data[0], cmap="inferno", origin="lower", vmin=0.0, vmax=1.5)
        ax1.set_title("Panel A: Frame 1 Spatial Canvas Grid\n(STARRY_SKY_CUBE Layer)", fontsize=11, weight="bold")
        ax1.set_xlabel("Right Ascension (J2000)")
        ax1.set_ylabel("Declination (J2000)")
        ax1.grid(color="white", linestyle="--", alpha=0.25)
        cbar = plt.colorbar(im, ax=ax1, pad=0.02, aspect=15)
        cbar.set_label("Flux Intensity Units", rotation=270, labelpad=12)

        # Isolate unique stars that survived the C engine filtering check in frame 1
        f1_table = table_data[table_data["FRAME"] == 1]
        ax1.scatter(
            f1_table["X_CENTER"], f1_table["Y_CENTER"],
            s=f1_table["NET_FLUX"] * 25, edgecolors="cyan",
            facecolors="none", linewidths=1.2, label="Verified Stars (SNR >= 5)"
        )
        ax1.legend(loc="upper right", framealpha=0.8)

        # PANEL 2 (Right side): Extract the Light Curve of the Absolute Brightest Star Core
        brightest_idx = np.argmax(table_data["NET_FLUX"])
        target_star_id = table_data[brightest_idx]["STAR_ID"]
        star_profile = table_data[table_data["STAR_ID"] == target_star_id]

        # Sort chronological frame indices to align lines accurately
        sort_order = np.argsort(star_profile["FRAME"])
        plot_times = time_vector[star_profile["FRAME"][sort_order] - 1]
        plot_fluxes = star_profile["NET_FLUX"][sort_order]
        plot_snrs = star_profile["SNR"][sort_order]

        ax2 = fig.add_subplot(2, 1, 2)

        # FIXED: Swapped 'tab:crimson' for universal hex code '#d32f2f'
        color_flux = "#d32f2f"
        ax2.set_xlabel(f"Observational WCS Elapsed Timeline Scale ({time_unit})", fontsize=11, labelpad=8)
        ax2.set_ylabel("Integrated Net Flux (Jansky)", color=color_flux, fontsize=11)
        ax2.plot(plot_times, plot_fluxes, color=color_flux, marker="o", linewidth=2.5, label="Net Stellar Signal Flux")
        ax2.tick_params(axis="y", labelcolor=color_flux)
        ax2.set_xticks(time_vector)
        ax2.grid(color="gray", linestyle=":", alpha=0.5)

        # Build a matching shared twin axis mapping to mirror SNR shifts alongside light curves
        ax3 = ax2.twinx()

        # FIXED: Swapped 'tab:blue' for universal hex code '#1976d2'
        color_snr = "#1976d2"
        ax3.set_ylabel("Calculated Signal-to-Noise Ratio (SNR)", color=color_snr, fontsize=11)
        ax3.plot(plot_times, plot_snrs, color=color_snr, marker="s", linestyle="--", linewidth=2.0, label="Dynamic SNR Performance")
        ax3.tick_params(axis="y", labelcolor=color_snr)

        ax2.set_title(f"Panel B: Simultaneous Cross-Layer Analysis Loop (Star ID {target_star_id:02d} Profile Tracking)", 
                      fontsize=12, weight="bold", pad=12)

        # Merge unified legend cards into a single visualization chart anchor box
        lines = ax2.get_lines() + ax3.get_lines()
        labels = [line.get_label() for line in lines]
        ax2.legend(lines, labels, loc="upper right", frameon=True, shadow=True)

        # 5. Save the final multi-panel dashboard graphic file onto disk
        output_filename = "master_archive_dashboard.png"
        plt.savefig(output_filename, dpi=300, bbox_inches="tight")
        plt.show()

    print(f"\n[SUCCESS] Visual overview dashboard successfully saved to: {output_filename}\n")

except FileNotFoundError:
    print(f"\n[ERROR] File '{filename}' not found. Please verify the filename on line 7.")
except Exception as e:
    print(f"\n[CRITICAL ERROR] Failed to parse extensions concurrently: {str(e)}")
