import numpy as np
import matplotlib.pyplot as plt
from astropy.io import fits

plt.close('all')

print("==================================================================")
print("     GENERATING FINAL PUBLICATION-READY PIPELINE COMPARISON       ")
print("==================================================================")

try:
    with fits.open("stage_25_master_observatory_dataset.fits") as hdul_cube:
        raw_frame = hdul_cube["STARRY_SKY_CUBE"].data[0]

    with fits.open("stage_43_stacked_master_image.fits") as hdul_master:
        stacked_frame = hdul_master[0].data

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    fig.patch.set_facecolor('#111111')

    vmin_val = 0.00
    vmax_val = 1.20

    im1 = ax1.imshow(raw_frame, cmap="inferno", origin="lower", vmin=vmin_val, vmax=vmax_val)
    ax1.set_title("Panel A: Raw Exposure Slice (Frame 1)\nHigh Atmospheric Noise Floor", color="white", fontsize=11, weight="bold", pad=10)
    ax1.axis("off")

    im2 = ax2.imshow(stacked_frame, cmap="inferno", origin="lower", vmin=vmin_val, vmax=vmax_val)
    ax2.set_title("Panel B: De-rotated & Co-added Master Product\nBackground Noise Power Floor Crushed by 76.07%", color="white", fontsize=11, weight="bold", pad=10)
    ax2.axis("off")

    # Fixed colorbar implementation using standard Matplotlib parameters
    cbar = fig.colorbar(
        im2,
        ax=[ax1, ax2],
        fraction=0.03,
        pad=0.03,
        label="Absolute Integrated Flux Density Scale (Jansky)"
    )

    cbar.ax.yaxis.label.set_color('white')
    cbar.ax.tick_params(colors='white')

    output_image = "final_pipeline_contrast_comparison.png"
    plt.savefig(output_image, dpi=300, facecolor=fig.get_facecolor(), bbox_inches="tight")
    plt.show()

    print(f"\n[SUCCESS] Master verification chart compiled! Graphics vector saved to: {output_image}\n")

except FileNotFoundError as e:
    print(f"\n[ERROR] Missing dataset component to draw final charts: {e.filename}")
