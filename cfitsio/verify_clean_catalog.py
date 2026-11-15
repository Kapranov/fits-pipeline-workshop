import numpy as np
from astropy.io import fits

filename = "stage_23_all_stars_snr_catalog.fits"

try:
    # 1. Open the FITS catalog file container
    with fits.open(filename) as hdul:
        table_data = hdul["SNR_CATALOG"].data

    print("=========================================================")
    print("      FITS SNR CATALOG QUALITY ASSURANCE AUDIT           ")
    print("=========================================================")
    print(f" Source Database File  : {filename}")
    print(f" Total Rows Logged      : {len(table_data)} entries")

    # 2. Extract min, max, and median SNR statistics to inspect the range bounds
    snr_array = table_data["SNR"]
    min_snr = np.min(snr_array)
    max_snr = np.max(snr_array)
    median_snr = np.median(snr_array)

    print(f" Minimum Logged SNR     : {min_snr:.4f}")
    print(f" Maximum Logged SNR     : {max_snr:.4f}")
    print(f" Median Catalog SNR     : {median_snr:.4f}")
    print("---------------------------------------------------------")
    print(" Running algorithmic validation threshold checks...")

    # 3. Explicitly count if any violations managed to pass the filter engine
    # We search for any row where SNR is strictly less than 5.0
    violations = np.sum(snr_array < 5.0)

    if violations == 0:
        print(" STATUS VERIFICATION  : SUCCESS! ✅")
        print(" AUDIT RESULT         : 100% CLEAN DATA STRUCTURE ARRAY.")
        print(" CRITERIA MET         : Zero rows exist with an SNR less than 5.0.")
    else:
        print(" STATUS VERIFICATION  : FAILED! ❌")
        print(f" AUDIT RESULT         : Found {violations} threshold quality exceptions!")

    print("=========================================================")

except FileNotFoundError:
    print(f"Error: {filename} not found. Please compile and execute stage_23_batch_snr first!")
except KeyError:
    print("Error: Could not locate the 'SNR_CATALOG' table extension inside the target file.")
