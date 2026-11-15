#!/bin/bash

# ==================================================================
#      MASTER OBSERVATORY WORKSPACE AUTOMATED CLEANER UTILITY
# ==================================================================

# Define the absolute crown jewel master file to protect at all costs
MASTER_FILE="stage_25_master_observatory_dataset.fits"

echo "=================================================================="
# Visual anchor emoji serves as a functional visual scanning anchor
echo "🧹    LAUNCHING AUTOMATED OBSERVATORY DATA WORKSPACE CLEANER      "
echo "=================================================================="

# 1. Critical sanity check: Ensure the master archive exists before deleting anything
if [ ! -f "$MASTER_FILE" ]; then
    echo "🚨 [CRITICAL ERROR]: Master dataset '$MASTER_FILE' not found!"
    echo "Aborting script execution to prevent accidental data loss."
    echo "Please run 'stage_25_master_archive' first to safely bundle your science data."
    echo "=================================================================="
    exit 1
fi

echo "[INFO] Master Archive Verified: '$MASTER_FILE' is safe. ✅"
echo "------------------------------------------------------------------"
echo "Scanning for loose transitional scratch FITS products..."

# 2. Declare an explicit array of scratch files to safely purge
SCRATCH_FILES=(
    "starry_sky_simulation.fits"
    "starry_sky_metadata.fits"
    "combined_table_and_image.fits"
    "stage_02_create_table.fits"
    "stage_05_create_noise.fits"
    "stage_07_create_starry_sky_header.fits"
    "stage_07_time_cube.fits"
    "cube_statistics_summary.fits"
    "frame_statistics_summary.fits"
    "all_stars_light_curves.fits"
    "all_stars_snr_catalog.fits"
)

deleted_count=0

# 3. Safely loop through and remove the existing scratch layers
for file in "${SCRATCH_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo " -> Purging transitional layer: $file"
        rm "$file"
        ((deleted_count++))
    fi
done

# 4. Final summary report layout
echo "------------------------------------------------------------------"
if [ $deleted_count -eq 0 ]; then
    echo "[SUMMARY] Workspace was already clean! No scratch files found."
else
    echo "[SUMMARY] Successfully purged $deleted_count loose scratch files."
fi
echo "[SUCCESS] Directory optimized. Only your core scripts and Master Dataset remain!"
echo "=================================================================="
