import os
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from astropy.io import fits

filename = "stage_40_telescope_rotated_cube.fits"

print("==================================================================")
print("     FIELD ROTATION TIME-SERIES MOVIE LOOPER INITIALIZED         ")
print("==================================================================")

# 1. Verify file presence and open the rotated FITS cube safely
if not os.path.exists(filename):
    print(f"[ERROR] File '{filename}' not found. Please compile and run stage_31_rotation.c first.")
    exit(1)

with fits.open(filename) as hdul:
    # Target the ROTATED_SKY extension layer and extract the 3D data array
    cube_data = hdul["ROTATED_SKY"].data

num_frames, height, width = cube_data.shape
print(f"[DATA] Loaded 3D Rotated Cube: Found {num_frames} frames ({width}x{height} pixels).")

# 2. Set up the dark plotting canvas
fig, ax = plt.subplots(figsize=(8, 8))
ax.axis('off')  # Hide pixel coordinates for an immersive dark sky view
fig.patch.set_facecolor('black')

# Initialize the canvas layer with the first frame
vmin_val, vmax_val = 0.00, 1.50
im = ax.imshow(cube_data[0], cmap="inferno", origin="lower", vmin=vmin_val, vmax=vmax_val)

title = ax.text(0.5, 0.96, "", transform=ax.transAxes, color="white",
                ha="center", fontsize=12, weight="bold")

# Add a subtle center reference point indicator crosshair marker
ax.scatter(width/2 - 0.5, height/2 - 0.5, color='white', marker='+', alpha=0.4, s=100,
           label="Mount Tracking Rotational Axis Center")
ax.legend(loc="lower right", framealpha=0.2, labelcolor='white')

# 3. Define the sequential animation update function loop
def update(frame_idx):
    # Calculate the active frame's angular position matching our C script logic
    angle_deg = frame_idx * 15.0

    # Refresh the active background frame layer slice
    im.set_array(cube_data[frame_idx])
    title.set_text(f"Alt-Az Field Rotation: Frame {frame_idx + 1} ({angle_deg:0.1f}° Shift)")

    return [im, title]

# 4. Assemble the dynamic animation engine instance
# interval=300 sets a clear pacing (3.3 frames per second)
ani = animation.FuncAnimation(
    fig, update, frames=num_frames, interval=300, blit=True
)

# 5. Compile and save the movie sequence as an animated GIF
output_gif = "field_rotation_loop.gif"
print(f"[BUILD] Rendering video frames... Writing to {output_gif}")

# Utilizing savefig_kwargs dictionary configuration to force background black color matching properties
ani.save(output_gif, writer='pillow', fps=3, savefig_kwargs={'facecolor': 'black'})
plt.close()

print(f"\n[SUCCESS] Field rotation animation saved successfully to: {output_gif}\n")
