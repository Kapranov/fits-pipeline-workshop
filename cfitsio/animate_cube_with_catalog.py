import os
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import numpy as np
from astropy.io import fits

filename = "stage_25_master_observatory_dataset.fits"

# 1. Verify file presence and open the master archive container safely
if not os.path.exists(filename):
    print(f"Error: {filename} not found. Please execute your Stage 25 packager binary code first.")
    exit(1)

with fits.open(filename) as hdul:
    cube_data = hdul["STARRY_SKY_CUBE"].data
    table_data = hdul["CLEAN_CATALOG"].data

num_frames, height, width = cube_data.shape
print(f"[DATA] Loading Master Archive: Found {num_frames} frames with {len(table_data)} catalog entries.")

# 2. Set up the plotting canvas
fig, ax = plt.subplots(figsize=(8, 8))
ax.axis('off')  # Clean space view
fig.patch.set_facecolor('black')

# Initialize the view loop with Frame 1 metrics
vmin_val, vmax_val = 0.00, 1.50
im = ax.imshow(cube_data[0], cmap="inferno", origin="lower", vmin=vmin_val, vmax=vmax_val)

title = ax.text(0.5, 0.96, "", transform=ax.transAxes, color="white",
                ha="center", fontsize=12, weight="bold")

# Containers to manage dynamic text annotations and crosshair scatter anchors
scatter_plots = []
text_annotations = []

# 3. Define the sequential animation update function loop
def update(frame_idx):
    global scatter_plots, text_annotations

    # Render the active pixel array grid slice layer
    im.set_array(cube_data[frame_idx])
    title.set_text(f"Observational Sequence: Frame {frame_idx + 1}")

    # Clear out previous coordinate overlays so frames don't blur together
    for sp in scatter_plots:
        sp.remove()
    for txt in text_annotations:
        txt.remove()
    scatter_plots.clear()
    text_annotations.clear()

    # Query the binary table data dynamically for stars tracked inside THIS specific frame layer (1-based index)
    active_frame_rows = table_data[table_data["FRAME"] == (frame_idx + 1)]

    # Draw tracking crosshairs and coordinate labels for the surviving entries
    if len(active_frame_rows) > 0:
        # Overlay cyan circular crosshairs centered precisely on their fractional position anchors
        sp = ax.scatter(
            active_frame_rows["X_CENTER"],
            active_frame_rows["Y_CENTER"],
            s=120, edgecolors="cyan", facecolors="none", linewidths=1.0, zorder=3
        )
        scatter_plots.append(sp)

        # Inject dynamic tracking text metrics labels for each item row entry
        for row in active_frame_rows:
            star_id = row["STAR_ID"]
            fx = row["X_CENTER"]
            fy = row["Y_CENTER"]
            snr = row["SNR"]

            # Formatted text block showing fractional tracking parameters
            label_text = f"ID:{star_id:02d}\n({fx:.2f}, {fy:.2f})\nSNR:{snr:.1f}"

            # Offset text slightly to the upper right of the star center for pristine layout legibility
            txt = ax.text(
                fx + 3, fy + 3, label_text,
                color="cyan", fontsize=7, weight="medium",
                bbox=dict(facecolor='black', alpha=0.5, edgecolor='none', pad=1)
            )
            text_annotations.append(txt)

    return [im, title] + scatter_plots + text_annotations

# 4. Assemble the dynamic animation engine instance
ani = animation.FuncAnimation(
    fig, update, frames=num_frames, interval=250, blit=True
)

# 5. Compile and save the complete structural data loops as an animated GIF file
output_gif = "dynamic_tracked_stars.gif"
print(f"[BUILD] Assembling coordinate overlays... Writing to {output_gif}")

# Utilizing the savefig_kwargs macro flag dict configuration to secure background colors
ani.save(output_gif, writer='pillow', fps=4, savefig_kwargs={'facecolor': 'black'})
plt.close()

print(f"\n[SUCCESS] Master animation complete! Movie file saved to: {output_gif}\n")
