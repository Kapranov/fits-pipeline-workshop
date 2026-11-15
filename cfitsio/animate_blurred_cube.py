import os
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from astropy.io import fits

filename = "stage_38_telescope_blurred_cube.fits"

print("==================================================================")
print("     BLURRED IMAGE TIME-SERIES TRACKER ANIMATION INITIALIZED      ")
print("==================================================================")

# 1. Verify file presence and open the blurred FITS cube safely
if not os.path.exists(filename):
    print(f"[ERROR] File '{filename}' not found. Please compile and run stage_38_convolution.c first.")
    exit(1)

with fits.open(filename) as hdul:
    cube_data = hdul["BLURRED_SKY"].data

num_frames, height, width = cube_data.shape
print(f"[DATA] Loaded 3D Blurred Cube: Found {num_frames} frames ({width}x{height} pixels).")

# 2. Set up the dark plotting canvas
fig, ax = plt.subplots(figsize=(8, 8))
ax.axis('off')
fig.patch.set_facecolor('black')

# Initialize the canvas layer with Frame 1.
# Lowered vmax to 0.40 to capture the soft wings of the blurred star structures cleanly.
vmin_val, vmax_val = 0.00, 0.40
im = ax.imshow(cube_data[0], cmap="inferno", origin="lower", vmin=vmin_val, vmax=vmax_val)

title = ax.text(0.5, 0.96, "", transform=ax.transAxes, color="white",
                ha="center", fontsize=12, weight="bold")

# Lists to track dynamic overlay objects across loop steps
scatter_plots = []
text_annotations = []

# 3. Define the sequential rendering loop function updates
def update(frame_idx):
    global scatter_plots, text_annotations

    # Refresh the active background frame layer slice
    im.set_array(cube_data[frame_idx])
    title.set_text(f"Defocused Telescope Tracking: Frame {frame_idx + 1}")

    # Wipe the slate clean of previous text markers
    for sp in scatter_plots:
        sp.remove()
    for txt in text_annotations:
        txt.remove()
    scatter_plots.clear()
    text_annotations.clear()

    # 4. LIVE SUB-PIXEL LOCAL PEAK TRACKING LOOP
    frame_pixels = cube_data[frame_idx]
    star_threshold = 0.20 # Lowered to account for convolved peak flattening
    star_count = 0

    cx_list = []
    cy_list = []

    # Scan the matrix grid for local maxima
    for y in range(2, height - 2):
        for x in range(2, width - 2):
            val = frame_pixels[y, x]
            if val >= star_threshold:
                if (val >= frame_pixels[y, x-1] and val >= frame_pixels[y, x+1] and
                    val >= frame_pixels[y-1, x] and val >= frame_pixels[y+1, x] and
                    val >= frame_pixels[y-1, x-1] and val >= frame_pixels[y-1, x+1] and
                    val >= frame_pixels[y+1, x-1] and val >= frame_pixels[y+1, x+1]):

                    star_count += 1

                    # Compute localized 3x3 center-of-mass weighting geometry
                    sum_w, sum_wx, sum_wy = 0.0, 0.0, 0.0
                    for j in range(-1, 2):
                        for i in range(-1, 2):
                            intensity = frame_pixels[y + j, x + i]
                            sum_w += intensity
                            sum_wx += intensity * (x + i + 1) # 1-based index conversion
                            sum_wy += intensity * (y + j + 1)

                    centroid_x = sum_wx / sum_w if sum_w > 0 else (x + 1)
                    centroid_y = sum_wy / sum_w if sum_w > 0 else (y + 1)

                    cx_list.append(centroid_x)
                    cy_list.append(centroid_y)

                    # Only print text annotations for the first few stars to prevent overlapping text cramps
                    if star_count <= 8:
                        label_text = f"ID:{star_id_mock_map if 'star_id_mock_map' in locals() else star_count:02d}\n({centroid_x:.2f}, {centroid_y:.2f})"
                        txt = ax.text(
                            centroid_x + 3, centroid_y + 3, label_text,
                            color="#00ffff", fontsize=7, weight="bold", # Cyan hex highlight
                            bbox=dict(facecolor='black', alpha=0.6, edgecolor='none', pad=1)
                        )
                        text_annotations.append(txt)

    # 5. Draw cyan crosshairs precisely over the center tracking coordinates
    if len(cx_list) > 0:
        sp = ax.scatter(cx_list, cy_list, s=140, edgecolors="#00ffff", facecolors="none", linewidths=1.0, zorder=3)
        scatter_plots.append(sp)

    return [im, title] + scatter_plots + text_annotations

# 6. Assemble and compile the final animation sequence
ani = animation.FuncAnimation(
    fig, update, frames=num_frames, interval=250, blit=True
)

output_gif = "tracked_blurred_stars.gif"
print(f"[BUILD] Rendering video frames... Writing to {output_gif}")

# Save movie file safely with black frame configurations mapped securely
ani.save(output_gif, writer='pillow', fps=4, savefig_kwargs={'facecolor': 'black'})
plt.close()

print(f"\n[SUCCESS] Blurred star tracking animation saved to: {output_gif}\n")
