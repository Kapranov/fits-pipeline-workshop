[![License: MIT](https://shields.io)](https://opensource.org)
[![Python 3.10+](https://shields.io)](https://python.org)

# ESO IFS Kinematic & Dust-Extinction Pipeline (`eso_ifs_pipeline.py`)

An end-to-end, automated Python command-line pipeline for querying, downloading, and analyzing Integral Field Spectroscopy (IFS) 3D data cubes (such as VLT/MUSE) from the **ESO Science Archive**.

The pipeline uses Virtual Observatory (VO) standards (**TAP**, **DataLink**, and **SODA**) via `pyvo` to stream proprietary or public spatial-spectral cutouts directly to disk, verifies sky footprint overlap, computes 2D kinematic moment maps and `[N II] λ6583 / Hα` excitation regimes, performs **Cardelli, Clayton & Mathis (1989) (CCM89)** Balmer decrement dust-extinction corrections, fits two-component (narrow disk + broad outflow) Gaussian models, and exports publication-ready figures, CSV/LaTeX tables, and a **16-extension WCS-calibrated FITS file**.

---

## Key Features

* **Authenticated ESO DataLink & SODA Streaming:** Wraps ESO OIDC Bearer tokens into `pyvo.auth.AuthSession` to dynamically discover SODA endpoints and stream memory-safe FITS sub-cubes without downloading full multi-hundred-GB parent cubes.
* **Spherical Footprint Verification:** Queries `ivoa.ObsCore` via ADQL `INTERSECTS(s_region, CIRCLE(...))` to confirm target overlap prior to download and plots the STC-S mosaic polygon against the requested cutout aperture.
* **2D Kinematic & Excitation Mapping:** Generates continuum-subtracted surface brightness maps, integrated spaxel flux maps, line-of-sight velocity fields (`Moment 1`), velocity dispersion maps (`Moment 2`), and `[N II] λ6583 / Hα` line-ratio diagnostics.
* **BPT-Style Kinematic Spaxel Classification:** Separates spaxels into **Class 1 (`H II` Starburst)**, **Class 2 (`Composite / Interface`)**, and **Class 3 (`Shock / Outflow`)** regimes using dispersion and ionization thresholds.
* **CCM89 Balmer Decrement Dust Correction:** Downloads matching **Hα (`6562.82 Å`)** and **Hβ (`4861.33 Å`)** cutouts to derive per-spaxel $E(B-V)$, visual extinction $A_V$ ($R_V = 3.1$), and dust-corrected $\text{H}\alpha$ luminosities and star formation rates (SFRs) with two-band continuum-noise error propagation and an optional `--max-av` cap.
* **6-Gaussian Outflow Decomposition:** Simultaneously fits a coupled narrow systemic disk component and broad outflow wing across the `[N II] λ6548 – Hα – [N II] λ6583` triplet to measure outflow velocity shifts, FWHM broadening, and shock enhancement factors.
* **Single-Target & Batch Survey Modes:** Supports individual target execution or multi-galaxy batch processing via `--batch-csv` with automatic master table merging and optional `--export-latex` manuscript table generation.

---

## Requirements & Installation

Ensure Python 3.10+ is installed, then install the dependencies listed in `requirements.txt`:

```bash
pip install -r requirements.txt
```

### Authentication for Proprietary ESO Data
If querying proprietary ESO archive datasets, obtain your `id_token` from the ESO Science Portal and either:
* Export it as an environment variable for non-interactive runs:
  ```bash
  export ESO_ID_TOKEN="your_verified_eso_id_token_string"
  ```
* Or run the script without setting the variable to be prompted securely via `getpass`.

---

## Complete CLI Parameter Reference

`python eso_ifs_pipeline.py` supports 13 command-line flags across two mutually exclusive execution modes:
Single-Target Mode (`--target` + `--dp-id`) and Batch Manifest Mode (`--batch-csv`).

| Flag | Type | Default | Required? | Description & Example Value |
| :--- | :--- | :--- | :--- | :--- |
| **`--target`** | `str` | `None` | Single mode | Target galaxy or astronomical name resolved via SIMBAD (e.g., `"NGC253"`). |
| **`--dp-id`** | `str` | `None` | Single mode | ESO Science Archive dataset identifier (e.g., `"ADP.2025-07-06T07:52:11.871"`). |
| **`--batch-csv`** | `str` | `None` | Batch mode | Path to a CSV manifest file containing multiple targets (e.g., `"targets.csv"`). |
| **`--ra`** | `float` | `None` | Optional | Manual Right Ascension override in decimal degrees ICRS (e.g., `11.888058`). |
| **`--dec`** | `float` | `None` | Optional | Manual Declination override in decimal degrees ICRS (e.g., `-25.288800`). |
| **`--z`** | `float` | `None` | Optional | Systemic redshift override; queried from SIMBAD if omitted (e.g., `0.000811`). |
| **`--dist-mpc`** | `float` | `None` | Optional | Luminosity distance in Mpc; computed via `Planck18` if omitted (e.g., `3.5`). |
| **`--radius`** | `float` | `10.0` | Optional | Circular SODA cutout extraction radius in arcseconds (e.g., `12.0`). |
| **`--band-half-nm`** | `float` | `5.0` | Optional | Half-width in nanometers around redshifted Hα and Hβ (e.g., `5.0` gives a `10 nm` window). |
| **`--snr`** | `float` | `5.0` | Optional | Minimum per-spaxel signal-to-noise ratio for moment and Balmer masking (e.g., `5.0`). |
| **`--max-av`** | `float` | `None` | Optional | Upper visual extinction cap $A_V$ in magnitudes to prevent noise spikes in extreme dust lanes (e.g., `6.0`). |
| **`--outdir`** | `str` | `"pipeline_outputs"` | Optional | Root output directory where `data/`, `figures/`, and `tables/` are created. |
| **`--force-download`** | `flag` | `False` | Optional | Forces re-downloading Hα and Hβ SODA cutouts even if cached files already exist on disk. |
| **`--export-latex`** | `flag` | `False` | Optional | Exports the kinematic summary CSV as a publication-ready LaTeX `table*` (`.tex`) file. |

---

## Usage Examples

```
export ESO_ID_TOKEN="eyJhbGciOiJSUzI1NiIs..."
python eso_ifs_pipeline.py --help
```

### 1. Single-Target Run (with Redshift, Distance, $A_V$ Cap & LaTeX Export)
```bash
python eso_ifs_pipeline.py \
    --target "NGC253" \
    --dp-id "ADP.2025-07-06T07:52:11.871" \
    --z 0.000811 \
    --dist-mpc 3.5 \
    --max-av 6.0 \
    --export-latex
```

### 2. Non-Interactive Run with Custom Coordinates & Output Folder
```bash
export ESO_ID_TOKEN="eyJhbGciOiJSUzI1NiIs..."

python eso_ifs_pipeline.py \
    --target "NGC253" \
    --dp-id "ADP.2025-07-06T07:52:11.871" \
    --ra 11.888058 \
    --dec -25.288800 \
    --z 0.000811 \
    --dist-mpc 3.5 \
    --radius 12.0 \
    --snr 5.0 \
    --max-av 6.0 \
    --export-latex \
    --outdir "ngc253_results"
```

### 3. Re-Running on Cached Cutouts
Once `<prefix>_ha_soda_cutout.fits` and `<prefix>_hb_soda_cutout.fits` are saved in `<outdir>/data/`, subsequent runs automatically reuse the cached cubes unless `--force-download` is specified:
```bash
python eso_ifs_pipeline.py \
    --target "NGC253" \
    --dp-id "ADP.2025-07-06T07:52:11.871" \
    --z 0.000811 \
    --dist-mpc 3.5 \
    --snr 8.0 \
    --max-av 6.0 \
    --export-latex
```

### 4. Multi-Target Batch Mode (`--batch-csv`)
Create a CSV manifest file (`targets.csv`) with `target` and `dp_id` columns (leave optional columns blank to use SIMBAD and `Planck18` defaults):

```csv
target,dp_id,ra,dec,z,dist_mpc,radius
NGC253,ADP.2025-07-06T07:52:11.871,,,0.000811,3.5,10.0
NGC1365,ADP.2020-04-28T13:45:12.123,,,,17.3,12.0
M83,ADP.2021-09-14T08:11:44.500,,,,,10.0
```

### 5. Execute the batch survey with combined CSV and LaTeX manuscript table generation:
```bash
python eso_ifs_pipeline.py \
    --batch-csv targets.csv \
    --snr 5.0 \
    --max-av 6.0 \
    --export-latex \
    --outdir "batch_survey"
```


### 6. Execute CSV to TeX to format the table specifically for AAS Journals (ApJ / AJ) using AASTeX:

```
python -c 'from eso_ifs_pipeline import export_summary_to_latex; export_summary_to_latex("pipeline_outputs/tables/ngc253_kinematic_summary.csv", "pipeline_outputs/tables/ngc253_kinematic_summary.tex")'

```

### 7. How to run it on any galaxy

```
bash> export ESO_ID_TOKEN="eyJhbGciOiJSUzI1NiIs..."

python eso_ifs_pipeline.py --target "NGC253" --list-only
python eso_ifs_pipeline.py --target "NGC253" --list-only --list-out ngc253_cubes.txt
python eso_ifs_pipeline.py --target "NGC253" --pp-id "1100.B-0651" --instrument "MUSE"
python eso_ifs_pipeline.py --target "NGC253" --pp-id "1100.B-0651" --instrument "MUSE" --list-only
python eso_ifs_pipeline.py --target "NGC253" --dp-id "ADP.2025-07-06T07:52:11.871"
python eso_ifs_pipeline.py --target "NGC253" --dp-id "ADP.2025-07-06T07:52:11.871" --z 0.000811 --radius 60.0 --dist-mpc 9.5 --max-av 10.0 --export-latex
python eso_ifs_pipeline.py --target "NGC253" --dp-id "ADP.2025-07-06T07:52:11.871" --z 0.000811 --dist-mpc 3.5 --pa 52.0 --inc 78.0 --radial-bin 2.0 --radius 60.0 --max-av 10.0 --export-latex

Target Resolved: NGC253 | RA=11.888058 deg, DEC=-25.288800 deg, z=0.000811
-> Spatial pixel area: 0.0400 arcsec^2 / spaxel
-> CCM89 Extinction (N=83582 Balmer spaxels): Median E(B-V) = 0.810 mag | Median A_V = 2.511 ± 0.320 mag | Capped 309 spaxels at A_V <= 10.0 mag
Saved 3-panel CCM89 dust diagnostic figure: pipeline_outputs/figures/ngc253_ccm89_dust_extinction.png
-> Luminosity Distance D_L = 3.500 Mpc (4*pi*D_L^2 = 1.466e+51 cm^2)
-> HII_Starburst (N=102530): L_Ha(corr) = 8.337e+40 ± 4.560e+38 erg/s | SFR(corr) = 0.4477 ± 0.0024 Msun/yr (A_V=2.40 mag)
-> Composite_Interface (N=160897): L_Ha(corr) = 3.644e+41 ± 4.413e+39 erg/s | SFR(corr) = 1.9566 ± 0.0237 Msun/yr (A_V=2.63 mag)
-> Shock_Outflow (N=66446): L_Ha(corr) = 4.116e+41 ± 4.007e+39 erg/s | SFR(corr) = 2.2105 ± 0.0215 Msun/yr (A_V=3.65 mag)
Exported CSV summary with Luminosity & SFR: pipeline_outputs/tables/ngc253_kinematic_summary.csv
Exported 11-extension MEF FITS (10**(-20) erg / (s cm**2 arcsec**2)): pipeline_outputs/data/ngc253_kinematics_mef.fits
Filename: (No file associated with this HDUList)
No.    Name      Ver    Type      Cards   Dimensions   Format
  0  PRIMARY       1 PrimaryHDU       6   ()
  1  HALPHA_SB     1 ImageHDU        34   (600, 600)   float32
  2  HALPHA_FLUX    1 ImageHDU        34   (600, 600)   float32
  3  HALPHA_MOM1    1 ImageHDU        34   (600, 600)   float32
  4  HALPHA_MOM2    1 ImageHDU        34   (600, 600)   float32
  5  NII6583_SB    1 ImageHDU        34   (600, 600)   float32
  6  NII6583_FLUX    1 ImageHDU        34   (600, 600)   float32
  7  NII_HA_RATIO    1 ImageHDU        34   (600, 600)   float32
  8  LOG_NII_HA    1 ImageHDU        34   (600, 600)   float32
  9  EXCITATION_MASK    1 ImageHDU        38   (600, 600)   int16
 10  HALPHA_FLUX_CORR    1 ImageHDU        34   (600, 600)   float32
 11  BALMER_DEC    1 ImageHDU        34   (600, 600)   float32
 12  EBV_CCM89     1 ImageHDU        34   (600, 600)   float32
 13  AV_CCM89      1 ImageHDU        34   (600, 600)   float32
 14  AV_CCM89_ERR    1 ImageHDU        34   (600, 600)   float32
 15  KINEMATIC_SUMMARY    1 BinTableHDU    111   3R x 39C   ['6A', 'D', 'D', 'K', '19A', 'K', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D']

--- Extracting Deprojected Galactocentric Radial Profiles ---
Exported radial profile CSV (143 annuli): pipeline_outputs/tables/ngc253_radial_profiles.csv
Saved 4-panel radial profile figure: pipeline_outputs/figures/ngc253_radial_profiles.png (300 DPI)
Appended 'RADIAL_PROFILES' extension to MEF FITS: pipeline_outputs/data/ngc253_kinematics_mef.fits
Exported publication-ready LaTeX table: pipeline_outputs/tables/ngc253_kinematic_summary.tex
Exported AASTeX radial profile table to: pipeline_outputs/tables/ngc253_radial_profiles.tex

Pipeline execution finished successfully!
```

```
bash> export ESO_ID_TOKEN="eyJhbGciOiJSUzI1NiIs..."

python eso_ifs_pipeline.py --target "M83" --list-only
python eso_ifs_pipeline.py --target "M83" --list-only --list-out m83_cubes.txt
python eso_ifs_pipeline.py --target "M83" --radius 12 --outdir results_m83
python eso_ifs_pipeline.py --target "M83" --dp-id "ADP.2019-06-25T17:20:12.161" --z 0.001711 --dist-mpc 4.61 --radius 45 --snr 10.0 --force-download --outdir results_m83
python eso_ifs_pipeline.py --target "M83" --dp-id "ADP.2019-06-25T17:20:12.161" --z 0.001711 --dist-mpc 4.61 --pa 225.0 --inc 24.0 --radial-bin 1.5 --radius 45 --snr 10.0 --outdir results_m83
python eso_ifs_pipeline.py --target "M83" --dp-id "ADP.2019-06-25T17:20:12.161" --z 0.001711 --dist-mpc 4.61 --pa 225.0 --inc 24.0 --radial-bin 1.5 --radius 45 --snr 10.0 --outdir results_m83

Target Resolved: M83 | RA=204.253830 deg, DEC=-29.865761 deg, z=0.001711
-> Spatial pixel area: 0.0400 arcsec^2 / spaxel
-> CCM89 Extinction (N=17739 Balmer spaxels): Median E(B-V) = 0.678 mag | Median A_V = 2.102 ± 0.120 mag
Saved 3-panel CCM89 dust diagnostic figure: results_m83/figures/m83_ccm89_dust_extinction.png
-> Luminosity Distance D_L = 4.610 Mpc (4*pi*D_L^2 = 2.543e+51 cm^2)
-> HII_Starburst (N=21211): L_Ha(corr) = 1.015e+41 ± 1.240e+38 erg/s | SFR(corr) = 0.5452 ± 0.0007 Msun/yr (A_V=2.00 mag)
-> Composite_Interface (N=14940): L_Ha(corr) = 7.252e+40 ± 1.550e+38 erg/s | SFR(corr) = 0.3894 ± 0.0008 Msun/yr (A_V=2.99 mag)
-> Shock_Outflow (N=1218): L_Ha(corr) = 1.095e+39 ± 3.878e+36 erg/s | SFR(corr) = 0.0059 ± 0.0000 Msun/yr (A_V=0.00 mag)
Exported CSV summary with Luminosity & SFR: results_m83/tables/m83_kinematic_summary.csv
Exported 11-extension MEF FITS (10**(-20)*erg/s/cm**2 arcsec**(-2)): results_m83/data/m83_kinematics_mef.fits
Filename: (No file associated with this HDUList)
No.    Name      Ver    Type      Cards   Dimensions   Format
  0  PRIMARY       1 PrimaryHDU       6   ()
  1  HALPHA_SB     1 ImageHDU        31   (323, 314)   float32
  2  HALPHA_FLUX    1 ImageHDU        31   (323, 314)   float32
  3  HALPHA_MOM1    1 ImageHDU        31   (323, 314)   float32
  4  HALPHA_MOM2    1 ImageHDU        31   (323, 314)   float32
  5  NII6583_SB    1 ImageHDU        31   (323, 314)   float32
  6  NII6583_FLUX    1 ImageHDU        31   (323, 314)   float32
  7  NII_HA_RATIO    1 ImageHDU        31   (323, 314)   float32
  8  LOG_NII_HA    1 ImageHDU        31   (323, 314)   float32
  9  EXCITATION_MASK    1 ImageHDU        35   (323, 314)   int16
 10  HALPHA_FLUX_CORR    1 ImageHDU        31   (323, 314)   float32
 11  BALMER_DEC    1 ImageHDU        31   (323, 314)   float32
 12  EBV_CCM89     1 ImageHDU        31   (323, 314)   float32
 13  AV_CCM89      1 ImageHDU        31   (323, 314)   float32
 14  AV_CCM89_ERR    1 ImageHDU        31   (323, 314)   float32
 15  KINEMATIC_SUMMARY    1 BinTableHDU    111   3R x 39C   ['3A', 'D', 'D', 'K', '19A', 'K', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D']

--- Extracting Deprojected Galactocentric Radial Profiles ---
Exported radial profile CSV (28 annuli): results_m83/tables/m83_radial_profiles.csv
Saved 4-panel radial profile figure: results_m83/figures/m83_radial_profiles.png (300 DPI)
Appended 'RADIAL_PROFILES' extension to MEF FITS: results_m83/data/m83_kinematics_mef.fits

Pipeline execution finished successfully!

python -c 'from eso_ifs_pipeline import export_summary_to_latex; export_summary_to_latex("results_m83/tables/m83_kinematic_summary.csv", "results_m83/tables/m83_kinematic_summary.tex")'
```

```
(python astro_env) bash> python

from astropy.io import fits

with fits.open("pipeline_outputs/data/ngc253_kinematics_mef.fits") as check_hdul:
    check_hdul.info()
```

The **M83** run succeeded cleanly with zero warnings, exporting the 15-extension
**WCS**-calibrated **MEF FITS** file (`results_m83/data/m83_kinematics_mef.fits`),
the  39-column summary CSV, and the CCM89 Balmer  decrement dust diagnostic figure.

**Scientific Highlights from Your M83 Run (`z = 0.001711`, `D_L = 4.61 Mpc`)**

* Balmer Decrement Dust Extinction (`N = 17,739 spaxels`):
    - Median Color Excess: `E(B-V) = 0.678 mag`
    - Median Visual Extinction: `A_V = 2.102 ± 0.120 mag`, matching literature
      measurements for the dusty circumnuclear starburst ring in M83.
* Dust-Corrected Star Formation Rates by Excitation Regime:
    - Class 1 (`HII_Starburst`, `N = 21,211 spaxels`):
      `L_Ha(corr) = 1.015 × 10^41 ± 1.240 × 10^38 erg/s` →
      `SFR(corr) = 0.5452 ± 0.0007 M_sun/yr` (`A_V = 2.00 mag`).
    - Class 2 (`Composite_Interface`, `N = 14,940 spaxels`):
      `L_Ha(corr) = 7.252 × 10^40 ± 1.550 × 10^38 erg/s` →
      `SFR(corr) = 0.3894 ± 0.0008 M_sun/yr` (`A_V = 2.99 mag`).
    - Class 3 (`Shock_Outflow`, `N = 1,218 spaxels`):
      `L_Ha(corr) = 1.095 × 10^39 ± 3.878 × 10^36 erg/s` (uncorrected lower
      limit `A_V = 0.00` mag where faint `Hβ` falls below `SNR = 10` in extended
      diffuse/shock regions).
* Total Nuclear Starburst Ring SFR: Combining the star-forming and composite ring
  regions yields `~ 0.935 M_sun/yr` within the central `R = 45"` (`~ 1.0 kpc`)
  nuclear zone.

---

## Directory & Output Structure

Running the pipeline generates an organized directory tree under `--outdir`:

```text
pipeline_outputs/
├── data/
│   ├── <prefix>_ha_soda_cutout.fits        # Downloaded H-alpha 3D spectral sub-cube
│   ├── <prefix>_hb_soda_cutout.fits        # Downloaded H-beta 3D spectral sub-cube
│   └── <prefix>_kinematics_mef.fits        # 16-extension WCS-calibrated MEF FITS product
├── figures/
│   ├── <prefix>_ha_footprint.png           # Sky footprint & ObsCore s_region verification (H-alpha)
│   ├── <prefix>_hb_footprint.png           # Sky footprint & ObsCore s_region verification (H-beta)
│   ├── <prefix>_ccm89_dust_extinction.png  # 3-panel Balmer decrement, A_V, and corrected H-alpha map
│   ├── <prefix>_kinematic_maps.png         # 6-panel Moment 0, 1, 2, [N II], and ratio maps
│   ├── <prefix>_bpt_scatter.png            # Kinematic BPT-style scatter & spatial regime map
│   └── <prefix>_outflow_fit.png            # 6-Gaussian narrow disk + broad outflow decomposition
└── tables/
    ├── <prefix>_kinematic_summary.csv      # Per-target kinematic, SFR, extinction, and Gaussian table
    ├── <prefix>_kinematic_summary.tex      # Publication-ready LaTeX table* (with --export-latex)
    ├── master_batch_kinematic_summary.csv  # Combined survey table (generated in --batch-csv mode)
    └── master_batch_kinematic_summary.tex  # Combined LaTeX survey table (with --batch-csv --export-latex)
```

---

## Multi-Extension FITS (`*_kinematics_mef.fits`) Structure

Every output MEF FITS file preserves celestial WCS coordinates (`PIXAREA` recorded in header) and standard `BUNIT` keywords across **16 HDUs**:

| Ext # | `EXTNAME` | Type | `BUNIT` | Description |
| :---: | :--- | :--- | :--- | :--- |
| `0` | `PRIMARY` | `PrimaryHDU` | — | Target name and systemic redshift metadata |
| `1` | `HALPHA_SB` | `ImageHDU` | `10**(-20) erg / (s cm**2 arcsec**2)` | Continuum-subtracted Hα surface brightness |
| `2` | `HALPHA_FLUX` | `ImageHDU` | `10**(-20) erg / (s cm**2)` | Continuum-subtracted Hα integrated flux per spaxel |
| `3` | `HALPHA_MOM1` | `ImageHDU` | `km / s` | Hα line-of-sight velocity offset (Moment 1) |
| `4` | `HALPHA_MOM2` | `ImageHDU` | `km / s` | Hα velocity dispersion $\sigma$ (Moment 2) |
| `5` | `NII6583_SB` | `ImageHDU` | `10**(-20) erg / (s cm**2 arcsec**2)` | Continuum-subtracted `[N II] λ6583` surface brightness |
| `6` | `NII6583_FLUX` | `ImageHDU` | `10**(-20) erg / (s cm**2)` | Continuum-subtracted `[N II] λ6583` integrated flux per spaxel |
| `7` | `NII_HA_RATIO` | `ImageHDU` | "" (dimensionless) | Linear [N II] λ6583 / Hα flux ratio |
| `8` | `LOG_NII_HA` | `ImageHDU` | dex | $\log_{10}(\text{[N II] }\lambda6583 / \text{H}\alpha)$ excitation ratio |
| `9` | `EXCITATION_MASK` | `ImageHDU` | code | Integer regime mask (0=Masked, 1=H II, 2=Composite, 3=Shock) |
| `10` | `HALPHA_FLUX_CORR` | `ImageHDU` | 10**(-20) erg / (s cm**2) | CCM89 dust-corrected Hα flux per spaxel |
| `11` | `BALMER_DEC` | `ImageHDU` | "" (dimensionless) | Observed $\text{H}\alpha / \text{H}\beta$ flux ratio |
| `12` | `EBV_CCM89` | `ImageHDU` | mag | Color excess $E(B-V)$ via CCM89 ($R_V = 3.1$) |
| `13` | `AV_CCM89` | `ImageHDU` | mag | Visual dust extinction $A_V$ via CCM89 ($R_V = 3.1$) |
| `14` | `AV_CCM89_ERR` | `ImageHDU` | mag | Propagated $1\sigma$ uncertainty on $A_V$ |
| `15` | `KINEMATIC_SUMMARY` | `BinTableHDU` | Mixed FITS units | Full 39-column summary table of luminosities, SFRs, and Gaussian fits |

---

## The project directory and install all dependencies in one command

```
numpy>=1.24.0
scipy>=1.10.0
matplotlib>=3.7.0
astropy>=5.3.0
pyvo>=1.4.0
astroquery>=0.4.6
requests>=2.28.0


bash> pip install -r requirements.txt
```

---

## License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

### 15 Nov 2026 by Oleg G.Kapranov
