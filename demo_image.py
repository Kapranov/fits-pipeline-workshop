import astropy.io.fits as fits
import numpy as np

# 1. Define dimensions
width = 300
height = 200

# 2. Generate linear ramp matrix
jj, ii = np.indices((height, width))
data_array = (ii + jj).astype(np.int16)

# 3. Pack data and inject metadata keywords
hdu = fits.PrimaryHDU(data=data_array)
hdu.header["EXPOSURE"] = (1500, "Total Exposure Time")

# 4. Dynamically compute and inject CHECMSUM and DATASUM keys
hdu.add_checksum()

# 5. Commit payload safely to disk
output_filename = "demo_image.fits"
hdu.writeto(output_filename, overwrite=True)

print(f"File successfully saved with computed internal verification checksums!")
