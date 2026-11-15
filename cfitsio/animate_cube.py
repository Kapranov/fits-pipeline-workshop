import os
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from astropy.io import fits

# 1. Open the 3D FITS data cube
filename = "stage_17_time_cube.fits"
if not os.path.exists(filename):
    print(f"Error: {filename} not found. Please run your C generation script first.")
    exit(1)

with fits.open(filename) as hdul:
    # Target the STARRY_SKY_CUBE extension layer and extract the 3D data array
    # FITS shape is ordered as (Frames, Height, Width) in Python/NumPy
    cube_data = hdul["STARRY_SKY_CUBE"].data

num_frames = cube_data.shape[0]
print(f"Loading 3D data cube: Found {num_frames} time-series frames.")

# 2. Set up the plotting canvas
fig, ax = plt.subplots(figsize=(6, 6))
ax.axis('off')  # Hide pixel axis boundaries for a clean celestial look
fig.patch.set_facecolor('black')  # Darken window framework edges

# 3. Establish consistent color scaling thresholds across frames
# vmin sits right near the noise floor, vmax captures bright star pixels
vmin_val = 0.00
vmax_val = 1.50

# Initialize the canvas layer with the very first frame image matrix slice
im = ax.imshow(cube_data[0], cmap="inferno", origin="lower", vmin=vmin_val, vmax=vmax_val)
title = ax.text(0.5, 0.95, "Frame 1", transform=ax.transAxes, color="white",
                ha="center", fontsize=12, weight="bold")

# 4. Define the sequential rendering loop function updates
def update(frame_idx):
    # Swap out the active pixel array grid data buffer seamlessly
    im.set_array(cube_data[frame_idx])
    title.set_text(f"Time Series: Frame {frame_idx + 1}")
    return [im, title]

# 5. Build the Animation Engine instance tracker structure
# interval=200 means each frame displays for 200 milliseconds (5 frames per second)
ani = animation.FuncAnimation(
    fig, update, frames=num_frames, interval=200, blit=True
)

# 6. Save the outputs straight into a permanent looping GIF file
output_gif = "twinkling_stars.gif"
print(f"Assembling frames... Writing to {output_gif}")

# Using standard 'pillow' writer backend framework native to python packages
ani.save(output_gif, writer='pillow', fps=5, savefig_kwargs={'facecolor': 'black'})
plt.close()

print("Success! Twinkling simulation GIF animation saved successfully.")
