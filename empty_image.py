import numpy as np
from astropy.io import fits

# Create an empty 1024x1024 image array filled with zeros
empty_data = np.zeros((1024, 1024), dtype=np.float32)

# Wrap it in a PrimaryHDU
hdu = fits.PrimaryHDU(data=empty_data)

# Dynamically compute and inject CHECMSUM and DATASUM keys
hdu.add_checksum()

# Save the file
hdu.writeto('empty_image.fits', overwrite=True)
