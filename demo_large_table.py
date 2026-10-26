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

