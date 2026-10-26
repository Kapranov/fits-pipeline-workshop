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

### Stage #2 Create Demo Table (`demo_table.py`)

```
import astropy.io.fits as fits
import numpy as np

# 1. Initialize sample array structures (100 elements each)
# Matching the uarray (unsigned short) and varray (unsigned int) profiles
uarray = np.arange(100, dtype=np.uint16)  # Unsigned 16-bit short integers
varray = np.arange(100, dtype=np.uint32)  # Unsigned 32-bit integers

# 2. Define the table columns
# 'format' maps to FITS column types: '100I' = 100 16-bit integers, '100J' = 100 32-bit integers
col1 = fits.Column(name="Col_1", format="100I", array=[uarray])
col2 = fits.Column(name="Col_2", format="100J", array=[varray])

# Combine the column definitions into a field layout schema
col_definitions = fits.ColDefs([col1, col2])

# 3. Create the Binary Table HDU extension layer
# This structures your extension name 'Test_table'
table_hdu = fits.BinTableHDU.from_columns(col_definitions, name="Test_table")

# 4. Assembly: FITS tables need to append behind a structural Primary HDU skeleton
primary_hdu = fits.PrimaryHDU()
hdul = fits.HDUList([primary_hdu, table_hdu])

# 5. Apply the robust integrity validation layer to every internal component
for hdu in hdul:
    hdu.add_checksum()

# 6. Commit payload directly to file disk
output_filename = "demo_table.fits"
hdul.writeto(output_filename, overwrite=True)

print(f"Successfully generated binary table file '{output_filename}' passing strict validation!")
```

output:

```
bash> fitscheck demo_table.fits
bash> fitsverify demo_table.fits

              fitsverify 4.22 (CFITSIO V4.7.0)
              --------------------------------


File: demo_table.fits

2 Header-Data Units in this file.

=================== HDU 1: Primary Array ===================

 7 header keywords

 Null data array; NAXIS = 0

=================== HDU 2: BINARY Table ====================

 16 header keywords

 TEST_TABLE  (2 columns x 1 rows)

 Col# Name (Units)       Format
   1 Col_1                100I
   2 Col_2                100J

++++++++++++++++++++++ Error Summary  ++++++++++++++++++++++

 HDU#  Name (version)       Type             Warnings  Errors
 1                          Primary Array    0         0
 2     TEST_TABLE           Binary Table     0         0

**** Verification found 0 warning(s) and 0 error(s). ****
```

### Stage #2 Create Demo Large Table (`demo_large_table.py`)

```
import astropy.io.fits as fits
import numpy as np

# 1. Define configuration constants
num_rows = 99
num_cols = 99
elements_per_cell = 100

# 2. Dynamically build the list of 99 columns
columns_list = []

for c in range(1, num_cols + 1):
    col_name = f"Col_{c}"

    # Alternate data types between Unsigned Short (I) and Unsigned Int (J) to mimic the C template
    if c % 2 != 0:
        # Unsigned 16-bit short integers ('100I')
        fits_format = f"{elements_per_cell}I"
        # Generate dummy data grid shape: (99 rows, 100 columns)
        data_matrix = np.full((num_rows, elements_per_cell), c, dtype=np.uint16)
    else:
        # Unsigned 32-bit integers ('100J')
        fits_format = f"{elements_per_cell}J"
        data_matrix = np.full((num_rows, elements_per_cell), c * 100, dtype=np.uint32)

    # Instantiate the Column layout schema
    column = fits.Column(name=col_name, format=fits_format, array=data_matrix)
    columns_list.append(column)

# Combine all generated columns into an official definition table wrapper
col_definitions = fits.ColDefs(columns_list)

# 3. Create the Binary Table HDU extension layer
table_hdu = fits.BinTableHDU.from_columns(col_definitions, name="Test_table")

# 4. Assembly: Append the table extension to an empty structural Primary HDU frame
primary_hdu = fits.PrimaryHDU()
hdul = fits.HDUList([primary_hdu, table_hdu])

# 5. Apply the required integrity validation checksum layer
for hdu in hdul:
    hdu.add_checksum()

# 6. Commit the heavy payload to disk
output_filename = "demo_large_table.fits"
hdul.writeto(output_filename, overwrite=True)

print(f"Successfully generated a matrix of {num_rows} rows by {num_cols} columns!")
print(f"Saved file as '{output_filename}' with passing data integrity tags.")
```

output:

```
bash> fitscheck demo_large_table.fits
bash> fitsverify demo_large_table.fits

              fitsverify 4.22 (CFITSIO V4.7.0)
              --------------------------------


File: demo_large_table.fits

2 Header-Data Units in this file.

=================== HDU 1: Primary Array ===================

 7 header keywords

 Null data array; NAXIS = 0

=================== HDU 2: BINARY Table ====================

 210 header keywords

 TEST_TABLE  (99 columns x 99 rows)

 Col# Name (Units)       Format
   1 Col_1                100I
   2 Col_2                100J
   3 Col_3                100I
   4 Col_4                100J
   5 Col_5                100I
   6 Col_6                100J
   7 Col_7                100I
   8 Col_8                100J
   9 Col_9                100I
  10 Col_10               100J
  11 Col_11               100I
  12 Col_12               100J
  13 Col_13               100I
  14 Col_14               100J
  15 Col_15               100I
  16 Col_16               100J
  17 Col_17               100I
  18 Col_18               100J
  19 Col_19               100I
  20 Col_20               100J
  21 Col_21               100I
  22 Col_22               100J
  23 Col_23               100I
  24 Col_24               100J
  25 Col_25               100I
  26 Col_26               100J
  27 Col_27               100I
  28 Col_28               100J
  29 Col_29               100I
  30 Col_30               100J
  31 Col_31               100I
  32 Col_32               100J
  33 Col_33               100I
  34 Col_34               100J
  35 Col_35               100I
  36 Col_36               100J
  37 Col_37               100I
  38 Col_38               100J
  39 Col_39               100I
  40 Col_40               100J
  41 Col_41               100I
  42 Col_42               100J
  43 Col_43               100I
  44 Col_44               100J
  45 Col_45               100I
  46 Col_46               100J
  47 Col_47               100I
  48 Col_48               100J
  49 Col_49               100I
  50 Col_50               100J
  51 Col_51               100I
  52 Col_52               100J
  53 Col_53               100I
  54 Col_54               100J
  55 Col_55               100I
  56 Col_56               100J
  57 Col_57               100I
  58 Col_58               100J
  59 Col_59               100I
  60 Col_60               100J
  61 Col_61               100I
  62 Col_62               100J
  63 Col_63               100I
  64 Col_64               100J
  65 Col_65               100I
  66 Col_66               100J
  67 Col_67               100I
  68 Col_68               100J
  69 Col_69               100I
  70 Col_70               100J
  71 Col_71               100I
  72 Col_72               100J
  73 Col_73               100I
  74 Col_74               100J
  75 Col_75               100I
  76 Col_76               100J
  77 Col_77               100I
  78 Col_78               100J
  79 Col_79               100I
  80 Col_80               100J
  81 Col_81               100I
  82 Col_82               100J
  83 Col_83               100I
  84 Col_84               100J
  85 Col_85               100I
  86 Col_86               100J
  87 Col_87               100I
  88 Col_88               100J
  89 Col_89               100I
  90 Col_90               100J
  91 Col_91               100I
  92 Col_92               100J
  93 Col_93               100I
  94 Col_94               100J
  95 Col_95               100I
  96 Col_96               100J
  97 Col_97               100I
  98 Col_98               100J
  99 Col_99               100I

++++++++++++++++++++++ Error Summary  ++++++++++++++++++++++

 HDU#  Name (version)       Type             Warnings  Errors
 1                          Primary Array    0         0
 2     TEST_TABLE           Binary Table     0         0

**** Verification found 0 warning(s) and 0 error(s). ****
```

### 26 Oct 2026 by Oleg G.Kapranov
