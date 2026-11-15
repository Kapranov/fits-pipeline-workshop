import matplotlib.pyplot as plt
from astropy.io import fits
from astropy.wcs import WCS

# 1. Open your generated FITS file container
hdul = fits.open("stage_07_create_starry_sky_header.fits")

# 2. Extract components: The 2D image array and the catalog table metrics
image_hdu = hdul["STARRY_SKY"]
table_data = hdul["STAR_DATA"].data

# 3. Read the WCS profile from the image header block
wcs = WCS(image_hdu.header)

# 4. Set up an advanced celestial projection canvas plotting area
fig = plt.figure(figsize=(10, 8))
ax = fig.add_subplot(1, 1, 1, projection=wcs)

# 5. Render the synthetic sky background layout
im = ax.imshow(image_hdu.data, cmap="inferno", origin="lower")
cbar = plt.colorbar(im, ax=ax, pad=0.01)
cbar.set_label("Intensity (Arbitrary Flux Units)", rotation=270, labelpad=15)

# 6. Scatter-plot the 50 table rows using pixel indices
# Scaling circle sizes (s) dynamically relative to their recorded flux strength
ax.scatter(
    table_data["X_POS"],
    table_data["Y_POS"],
    s=table_data["FLUX"] * 40,
    edgecolors="cyan",
    facecolors="none",
    linewidths=1.2,
    label="Cataloged Stars"
)

# 7. Add professional astronomical labels
ax.set_title("FITS Multi-Extension Simulation & Catalog Overlap", fontsize=14, pad=15)
ax.set_xlabel("Right Ascension (J2000)")
ax.set_ylabel("Declination (J2000)")
ax.grid(color="white", ls="--", alpha=0.3)
ax.legend(loc="upper right")

# 8. Save the chart as an image vector file and display it
plt.savefig("starry_sky_plot.png", dpi=300, bbox_inches="tight")
plt.show()

hdul.close()
print("Success! Saved visualization to starry_sky_plot.png")
