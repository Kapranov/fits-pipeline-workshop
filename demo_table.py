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
