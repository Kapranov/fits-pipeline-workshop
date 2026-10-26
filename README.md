# FITS Pipeline Workshop

## FITS astronomical format tutorials on how to use

### Stage #1 Create Demo Image (`demo_image.py`)

```
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
```

output:

```
bash> fitscheck demo_image.fits
bash> fitsverify demo_image.fits

              fitsverify 4.22 (CFITSIO V4.7.0)
              --------------------------------


File: demo_image.fits

1 Header-Data Units in this file.

=================== HDU 1: Primary Array ===================

 10 header keywords

 16-bit integer pixels,  2 axes (300 x 200),

++++++++++++++++++++++ Error Summary  ++++++++++++++++++++++

 HDU#  Name (version)       Type             Warnings  Errors
 1                          Primary Array    0         0

**** Verification found 0 warning(s) and 0 error(s). ****
```

### 26 Oct 2026 by Oleg G.Kapranov
