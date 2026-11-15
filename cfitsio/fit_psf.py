import numpy as np
import matplotlib.pyplot as plt
from astropy.io import fits
from scipy.optimize import curve_fit

# 1. Analytical 2D Gaussian mathematical equation definition
def gaussian_2d(coords, amplitude, xo, yo, sigma_x, sigma_y, background):
    x, y = coords
    xo = float(xo)
    yo = float(yo)
    # Exponent factor profile mapping calculations
    inner = ((x - xo)**2 / (2 * sigma_x**2)) + ((y - yo)**2 / (2 * sigma_y**2))
    return amplitude * np.exp(-inner) + background

filename = "stage_17_time_cube.fits"

print("==================================================================")
print("       2D GAUSSIAN PSF MODEL FITTING OPTIMIZATION ENGINE         ")
print("==================================================================")

try:
    with fits.open(filename) as hdul:
        cube_data = hdul["STARRY_SKY_CUBE"].data
    num_frames, height, width = cube_data.shape

    # Target Star Core Coordinates (Star ID 06 / Brightest core near X=28, Y=48)
    target_x, target_y = 28, 48
    box_radius = 5 # Extract an 11x11 pixel window cutout sub-grid box around peak

    # Slice coordinate cutout bounds configurations
    x_min, x_max = target_x - box_radius, target_x + box_radius + 1
    y_min, y_max = target_y - box_radius, target_y + box_radius + 1

    # Coordinate indexing grids mapping setup
    y_indices, x_indices = np.mgrid[y_min:y_max, x_min:x_max]
    coords_grid = np.vstack((x_indices.ravel(), y_indices.ravel()))

    frames = np.arange(1, num_frames + 1)
    fwhm_track = []
    centers_x, centers_y = [], []

    # 2. Frame-by-Frame Non-Linear Optimization Curve Fitting Loop
    for f in range(num_frames):
        # Extract the localized grid data cutout box footprint matrix
        cutout_box = cube_data[f, y_min:y_max, x_min:x_max].ravel()

        # Initial guess parameter values vector: [amplitude, xo, yo, sigma_x, sigma_y, background]
        initial_guess = (2.5, target_x, target_y, 1.2, 1.2, 0.04)

        # Fit bounds limits configurations to prevent divergence loops
        lower_bounds = (0.1, x_min, y_min, 0.3, 0.1, 0.0)
        upper_bounds = (5.0, x_max, y_max, 4.0, 4.0, 0.2)

        # Run Levenberg-Marquardt optimization script algorithm
        popt, _ = curve_fit(gaussian_2d, coords_grid, cutout_box, p0=initial_guess, bounds=(lower_bounds, upper_bounds))

        amplitude, xo, yo, sig_x, sig_y, bkg = popt

        # Calculate isotropic spatial Full-Width at Half-Maximum (FWHM)
        mean_sigma = (sig_x + sig_y) / 2.0
        fwhm = 2.35482 * mean_sigma

        fwhm_track.append(fwhm)
        centers_x.append(xo)
        centers_y.append(yo)

        print(f" Frame {f+1} -> Fitted Center: ({xo:.4f}, {yo:.4f}) | FWHM: {fwhm:.4f} pixels")

    print("-" * 66)

    # 3. Render a Multi-Panel Astronomical PSF Profiling Dashboard
    fig = plt.figure(figsize=(15, 5))
    plt.subplots_adjust(wspace=0.25)

    # Panel A: 2D Spatial Cutout Image for the Last Evaluated Frame Layer
    ax1 = fig.add_subplot(1, 3, 1)
    last_cutout = cube_data[-1, y_min:y_max, x_min:x_max]
    ax1.imshow(last_cutout, cmap="inferno", origin="lower", extent=[x_min, x_max, y_min, y_max])
    ax1.scatter(centers_x[-1], centers_y[-1], color='cyan', marker='x', s=100, linewidths=1.5, label="Fitted Center")
    ax1.set_title("Panel A: Target Core Cutout Matrix\n(Frame 5 Zoom View)", fontsize=11, weight="bold")
    ax1.set_xlabel("X (pixels)")
    ax1.set_ylabel("Y (pixels)")
    ax1.legend(loc="upper right")

    # Panel B: Fractional Sub-Pixel Tracking Drift Path
    ax2 = fig.add_subplot(1, 3, 2)
    ax2.plot(centers_x, centers_y, color='deepskyblue', linestyle='-', marker='o', linewidth=2, label="PSF Fitted Path")
    for idx, frame in enumerate(frames):
        ax2.text(centers_x[idx], centers_y[idx], f" F{frame}", color='darkblue', fontsize=9, weight="bold")
    ax2.set_title("Panel B: Sub-Pixel Target Center Drift\n(J2000 Jitter Jiggle Path)", fontsize=11, weight="bold")
    ax2.set_xlabel("Fractional X Center (pixels)")
    ax2.set_ylabel("Fractional Y Center (pixels)")
    ax2.grid(color="gray", linestyle=":", alpha=0.5)

    # Panel C: FWHM Atmospheric Blur Defocusing Variations Line Chart
    ax3 = fig.add_subplot(1, 3, 3)
    ax3.plot(frames, fwhm_track, color='crimson', linestyle='--', marker='s', linewidth=2, label="Measured FWHM")
    ax3.set_title("Panel C: Atmospheric Blurring Blur Shifts\n(Stellar Profile Defocusing Tracking)", fontsize=11, weight="bold")
    ax3.set_xlabel("Time Frame Index")
    ax3.set_ylabel("Profile Width FWHM (pixels)")
    ax3.set_xticks(frames)
    ax3.grid(color="gray", linestyle=":", alpha=0.5)
    ax3.legend(loc="upper right")

    # 4. Save visualization summary output graphics to drive sector layout
    output_filename = "psf_fitting_dashboard.png"
    plt.savefig(output_filename, dpi=300, bbox_inches="tight")
    plt.show()
    print(f"[SUCCESS] PSF model fitting compilation saved to: {output_filename}\n")

except FileNotFoundError:
    print(f"\n[ERROR] File '{filename}' not found. Please compile and run stage_17_generate_cube.c first.")
except Exception as e:
    print(f"\n[CRITICAL ERROR] Optimization routine script execution failure: {str(e)}")
