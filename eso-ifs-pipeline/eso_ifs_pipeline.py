#!/usr/bin/env python3
"""
End-to-End ESO IFS Kinematic & Excitation Pipeline CLI
Executes TAP ObsCore overlap checks, DataLink SODA cutouts, 2D moment mapping,
BPT-style classification, 6-Gaussian outflow decomposition, and MEF FITS export.
"""

import os
import re
import csv
import shutil
import getpass
import argparse
import requests
import numpy as np
import matplotlib
matplotlib.use("Agg")  # Headless backend for clean CLI batch execution
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import pyvo as vo
import warnings
from matplotlib.patches import Polygon
from matplotlib.colors import ListedColormap
from scipy.optimize import curve_fit
from pyvo.dal.adhoc import DatalinkResults, SodaQuery
from astropy.coordinates import SkyCoord
from astropy import units as u
from astropy.io import fits
from astropy.wcs import WCS
from astropy.table import Table
from astropy.cosmology import Planck18
from astropy.wcs.utils import proj_plane_pixel_area

def resolve_target_simbad(target_name: str, ra_override=None, dec_override=None, z_override=None):
    """Resolves coordinates and redshift via SIMBAD, NED, and local nearby-galaxy cache."""
    if ra_override is not None and dec_override is not None:
        ra_deg, dec_deg = float(ra_override), float(dec_override)
    else:
        coords = SkyCoord.from_name(target_name)
        ra_deg, dec_deg = float(coords.ra.deg), float(coords.dec.deg)

    if z_override is not None:
        redshift = float(z_override)
    else:
        redshift = None
        # 1. Try SIMBAD
        try:
            from astroquery.simbad import Simbad
            custom_simbad = Simbad()
            custom_simbad.add_votable_fields("rvz_redshift", "rvz_radvel")
            res = custom_simbad.query_object(target_name)
            if res is not None and len(res) > 0:
                if "rvz_redshift" in res.colnames and np.isfinite(res["rvz_redshift"]):
                    val = float(res["rvz_redshift"])
                    if -0.01 < val < 10.0:
                        redshift = val
                elif "rvz_radvel" in res.colnames and np.isfinite(res["rvz_radvel"]):
                    val = float(res["rvz_radvel"]) / 299792.458
                    if -0.01 < val < 10.0:
                        redshift = val
        except Exception:
            pass

        # 2. Fallback to NASA/IPAC Extragalactic Database (NED) — explicitly index row [0]
        if redshift is None:
            try:
                from astroquery.ipac.ned import Ned
                ned_res = Ned.query_object(target_name)
                if ned_res is not None and len(ned_res) > 0 and "Redshift" in ned_res.colnames:
                    val = float(ned_res["Redshift"])
                    if np.isfinite(val) and -0.01 < val < 10.0:
                        redshift = val
            except Exception:
                pass

        # 3. Local lookup cache for nearby galaxies
        if redshift is None:
            known_z = {
                "NGC253": 0.000811,
                "M83": 0.001711,
                "NGC5236": 0.001711,
                "M51": 0.001544,
                "NGC1365": 0.005457,
            }
            clean_key = re.sub(r"[^A-Z0-9]", "", target_name.upper())
            redshift = known_z.get(clean_key, 0.0)
            if redshift == 0.0:
                print(f"Warning: Could not resolve redshift for '{target_name}'; defaulting to z = 0.0 (pass --z manually).")

    print(f"Target Resolved: {target_name} | RA={ra_deg:.6f} deg, DEC={dec_deg:.6f} deg, z={redshift:.6f}")
    return ra_deg, dec_deg, redshift

def get_eso_authenticated_session(id_token: str) -> requests.Session:
    """Builds an authorized requests.Session using a verified ESO id_token."""
    clean_token = id_token.strip().replace("Bearer ", "")
    if clean_token.endswith("=="):
        clean_token = clean_token[:-2]
    access_token = f"{clean_token}=="

    session = requests.Session()
    session.headers.update({
        "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64)",
        "Authorization": f"Bearer {access_token}"
    })
    return session


def fetch_obscore_and_verify_overlap(dp_id: str, ra: float, dec: float, radius_deg: float, tap_url: str) -> dict:
    """Queries ESO TAP ObsCore for footprint metadata and tests spherical intersection."""
    tap_service = vo.dal.TAPService(tap_url)
    clean_dp_id = dp_id.replace("ivo://eso.org/ID?", "")

    meta_query = f"""
    SELECT s_ra, s_dec, s_fov, s_region, em_min, em_max
    FROM ivoa.ObsCore
    WHERE dp_id = '{clean_dp_id}'
    """
    res = tap_service.search(query=meta_query)
    if len(res) == 0:
        raise ValueError(f"Dataset '{clean_dp_id}' not found in ESO ObsCore.")

    row = res[0]
    s_region = str(row["s_region"]) if row["s_region"] is not None else ""

    overlap_query = f"""
    SELECT dp_id
    FROM ivoa.ObsCore
    WHERE dp_id = '{clean_dp_id}'
      AND 1 = INTERSECTS(s_region, CIRCLE('ICRS', {ra}, {dec}, {radius_deg}))
    """
    has_overlap = len(tap_service.search(query=overlap_query)) > 0

    return {
        "meta_ra": float(row["s_ra"]),
        "meta_dec": float(row["s_dec"]),
        "s_region": s_region,
        "has_overlap": has_overlap
    }

def list_matching_eso_cubes(
    ra: float,
    dec: float,
    radius_arcsec: float,
    dp_id: str = None,
    output_txt: str = None
):
    """
    Dry-run query against ESO's ivoa.ObsCore TAP service to list all 3D spectral cubes
    overlapping the target sky coordinates and save the table to a .txt file.
    """
    archive_host = "archive." + "eso.org"
    tap_url = f"https://{archive_host}/tap_obs"
    tap_service = vo.dal.TAPService(tap_url)
    radius_deg = (radius_arcsec * u.arcsec).to("deg").value

    dp_filter = ""
    if dp_id:
        clean_dp_id = dp_id.replace("ivo://eso.org/ID?", "")
        dp_filter = f"AND dp_id = '{clean_dp_id}'"

    query = f"""
    SELECT dp_id, instrument_name, target_name, em_min, em_max, t_exptime, obs_release_date
    FROM ivoa.ObsCore
    WHERE dataproduct_type = 'cube'
      AND 1 = INTERSECTS(s_region, CIRCLE('ICRS', {ra}, {dec}, {radius_deg}))
      {dp_filter}
    ORDER BY obs_release_date DESC
    """

    print(f"\nQuerying ESO TAP ObsCore for cubes overlapping RA={ra:.5f}, DEC={dec:.5f} (R={radius_arcsec}\")...")
    results = tap_service.search(query=query)

    lines = [
        f"ESO TAP ObsCore Cube Search Results",
        f"Query Center: RA={ra:.6f} deg, DEC={dec:.6f} deg | Search Radius={radius_arcsec} arcsec",
        f"Total Matching Cubes: {len(results)}",
        ""
    ]

    if len(results) == 0:
        lines.append("No matching 3D spectral cubes found at these coordinates.")
    else:
        header = f"{'Dataset ID (dp_id)':<30} | {'Instrument':<10} | {'Target':<14} | {'Band [nm]':<15} | {'Exp [s]':<8} | {'Release Date'}"
        lines.append(header)
        lines.append("-" * len(header))

        for row in results:
            did = str(row["dp_id"])
            inst = str(row["instrument_name"] or "N/A")
            targ = str(row["target_name"] or "N/A")[:14]
            em_min_nm = float(row["em_min"]) * 1e9 if row["em_min"] is not None else 0.0
            em_max_nm = float(row["em_max"]) * 1e9 if row["em_max"] is not None else 0.0
            band_str = f"{em_min_nm:.0f}-{em_max_nm:.0f}"
            exp_s = f"{float(row['t_exptime']):.0f}" if row["t_exptime"] is not None else "N/A"
            rel_date = str(row["obs_release_date"] or "Proprietary")[:10]

            lines.append(f"{did:<30} | {inst:<10} | {targ:<14} | {band_str:<15} | {exp_s:<8} | {rel_date}")

    report_text = "\n".join(lines)
    print("\n" + report_text + "\n")

    if output_txt:
        out_dir = os.path.dirname(output_txt)
        if out_dir:
            os.makedirs(out_dir, exist_ok=True)
        with open(output_txt, mode="w", encoding="utf-8") as f:
            f.write(report_text + "\n")
        print(f"Saved cube search list to: {output_txt}")

def plot_footprint_overlay(dp_id, meta, target_ra, target_dec, radius_deg, fig_path):
    """Plots the ObsCore s_region polygon overlaid with the requested SODA cutout circle."""
    s_region = meta["s_region"]
    poly_blocks = re.findall(r'POLYGON\s*(?:[A-Z0-9_]+\s*)?\(?([0-9eE.+\-\s,]+)\)?', s_region, flags=re.IGNORECASE)
    shapes = []
    for block in poly_blocks:
        nums = [float(x) for x in re.findall(r'[+-]?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?', block)]
        if len(nums) >= 6 and len(nums) % 2 == 0:
            shapes.append(np.array(nums).reshape(-1, 2))

    if not shapes:
        return

    fig, ax = plt.subplots(figsize=(7.5, 7.5))
    cos_dec = np.cos(np.deg2rad(target_dec))
    all_ras, all_decs = [target_ra, meta["meta_ra"]], [target_dec, meta["meta_dec"]]

    for idx, coords in enumerate(shapes):
        label = "ObsCore Footprint (s_region)" if idx == 0 else None
        ax.add_patch(Polygon(coords, closed=True, facecolor="dodgerblue", edgecolor="navy", alpha=0.22, lw=1.8, label=label))
        all_ras.extend(coords[:, 0])
        all_decs.extend(coords[:, 1])

    ax.scatter(meta["meta_ra"], meta["meta_dec"], color="navy", marker="+", s=90, lw=1.5, label="ObsCore Center")
    theta = np.linspace(0, 2 * np.pi, 200)
    c_ra = target_ra + (radius_deg / cos_dec) * np.cos(theta)
    c_dec = target_dec + radius_deg * np.sin(theta)
    color = "crimson" if meta["has_overlap"] else "darkorange"
    status = "OVERLAP CONFIRMED" if meta["has_overlap"] else "NO OVERLAP"

    ax.plot(c_ra, c_dec, color=color, lw=2.2, ls="--", label=f"SODA Cutout ({status})")
    ax.fill(c_ra, c_dec, color=color, alpha=0.25)
    ax.scatter(target_ra, target_dec, color=color, marker="x", s=70, lw=2, label="Target Center")

    ra_margin = (max(all_ras) - min(all_ras)) * 0.15 + (radius_deg / cos_dec) * 1.5
    dec_margin = (max(all_decs) - min(all_decs)) * 0.15 + radius_deg * 1.5
    ax.set_xlim(max(all_ras) + ra_margin, min(all_ras) - ra_margin)
    ax.set_ylim(min(all_decs) - dec_margin, max(all_decs) + dec_margin)
    ax.set_aspect(1.0 / cos_dec)
    ax.set_xlabel("Right Ascension (ICRS) [deg]")
    ax.set_ylabel("Declination (ICRS) [deg]")
    ax.set_title(f"Sky Footprint Verification: {dp_id}\nStatus: {status}", fontweight="bold")
    ax.grid(True, ls=":", alpha=0.6)
    ax.legend(loc="best", fontsize=9)
    fig.savefig(fig_path, dpi=250, bbox_inches="tight", facecolor="white")
    plt.close(fig)


def download_soda_cutout(dp_id, ra, dec, radius_arcsec, wave_min, wave_max, output_fits, id_token, footprint_fig):
    """Verifies spatial overlap, resolves DataLink SODA service, and streams the FITS cutout."""
    archive_host = "archive." + "eso.org"
    portal_host = "dataportal." + "eso.org"
    tap_url = f"https://{archive_host}/tap_obs"
    clean_dp_id = dp_id.replace("ivo://eso.org/ID?", "")
    radius_deg = (radius_arcsec * u.arcsec).to("deg").value

    meta = fetch_obscore_and_verify_overlap(clean_dp_id, ra, dec, radius_deg, tap_url)
    plot_footprint_overlay(clean_dp_id, meta, ra, dec, radius_deg, footprint_fig)

    if not meta["has_overlap"]:
        raise ValueError(f"Target circle does not overlap with s_region of {clean_dp_id}.")

    eso_session = get_eso_authenticated_session(id_token)
    datalink_url = f"https://{archive_host}/datalink/links?ID=ivo://eso.org/ID?{clean_dp_id}"

    preflight = eso_session.get(datalink_url)
    if preflight.status_code in (401, 403) or not preflight.text.strip():
        raise PermissionError(f"ESO authentication failed or token expired (HTTP {preflight.status_code}).")

    auth_coord = vo.auth.AuthSession()
    auth_coord.credentials.set("bearer-token", eso_session)
    auth_coord.add_security_method_for_url(f"https://{archive_host}", "bearer-token")
    auth_coord.add_security_method_for_url(f"https://{portal_host}", "bearer-token")

    dl_result = DatalinkResults.from_result_url(datalink_url, session=auth_coord)
    try:
        soda_desc = dl_result.get_adhocservice_by_ivoid("ivo://ivoa.net/std/SODA#sync-1.0")
    except Exception:
        soda_desc = dl_result.get_adhocservice_by_id(f"{clean_dp_id}_soda")

    sq = SodaQuery.from_resource(dl_result, soda_desc, session=auth_coord)
    sq.circle = (ra, dec, radius_deg)
    sq.band = (wave_min, wave_max)

    os.makedirs(os.path.dirname(output_fits), exist_ok=True)
    with sq.execute_stream() as resp:
        if getattr(resp, "status", 200) == 204:
            raise RuntimeError(
                f"SODA returned HTTP 204 No Content for {clean_dp_id} "
                f"(band {wave_min*1e9:.1f}-{wave_max*1e9:.1f} nm may be outside cube coverage)."
            )
        with open(output_fits, "wb") as f:
            shutil.copyfileobj(resp, f)
    sq.raise_if_error()

    if os.path.getsize(output_fits) == 0:
        os.remove(output_fits)
        raise RuntimeError(f"Downloaded SODA cutout is empty (0 bytes): {output_fits}")

def compute_ccm89_extinction_maps(
    hb_cube_path: str,
    ha_flux_map: np.ndarray,
    ha_flux_err_map: np.ndarray,
    redshift: float,
    d_spec_ref: float,
    snr_thresh: float = 5.0,
    max_av: float = None
):
    """
    Computes 2D Balmer decrement (H-alpha / H-beta), E(B-V), A_V, and dust-corrected
    H-alpha flux maps with full two-band (H-alpha + H-beta) continuum-noise error propagation
    under the Cardelli, Clayton & Mathis (1989) extinction law (R_V = 3.1).
    """
    c_kms = 299792.458
    wl_hb = 0.486133 * (1.0 + redshift)
    ln10 = np.log(10.0)

    def ccm89_k(wl_microns, rv=3.1):
        x = 1.0 / wl_microns
        y = x - 1.82
        a = (
            1.0 + 0.17699 * y - 0.50447 * y**2 - 0.02427 * y**3
            + 0.72085 * y**4 + 0.01979 * y**5 - 0.77530 * y**6 + 0.32999 * y**7
        )
        b = (
            1.41338 * y + 2.28305 * y**2 + 1.07233 * y**3 - 5.38434 * y**4
            - 0.62251 * y**5 + 5.30260 * y**6 - 2.09002 * y**7
        )
        return rv * a + b

    k_hb = ccm89_k(0.486133, rv=3.1)  # ~3.609
    k_ha = ccm89_k(0.656282, rv=3.1)  # ~2.535

    with fits.open(hb_cube_path) as hdul:
        hb_data, hb_header = None, None
        for hdu in hdul:
            if hdu.data is not None and hdu.data.ndim == 3:
                hb_data = hdu.data.astype(np.float64)
                hb_header = hdu.header
                break
        if hb_data is None:
            raise RuntimeError(f"No 3D H-beta cube found in {hb_cube_path}")

    wcs_hb = WCS(hb_header)
    n_spec, n_y, n_x = hb_data.shape
    coords = np.zeros((n_spec, 3))
    coords[:, 0] = n_x // 2
    coords[:, 1] = n_y // 2
    coords[:, 2] = np.arange(n_spec)
    wl = wcs_hb.wcs_pix2world(coords, 0)[:, 2] * 1e6
    d_lambda_hb = np.abs(np.median(np.diff(wl)))
    d_spec_hb = (d_lambda_hb * u.um).to(u.Angstrom).value if "Angstrom" in hb_header.get("BUNIT", "Angstrom") else d_lambda_hb * 1e4

    vel_hb = c_kms * (wl - wl_hb) / wl_hb

    # Local continuum & continuum RMS per spaxel from line-free channels (> 800 km/s from H-beta)
    cont_mask = np.abs(vel_hb) > 800.0
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", category=RuntimeWarning)
        if np.sum(cont_mask) >= 4:
            cont_hb = np.nanmedian(hb_data[cont_mask], axis=0)
            std_hb = np.nanstd(hb_data[cont_mask], axis=0)
        else:
            cont_hb = np.nanpercentile(hb_data, 15, axis=0)
            std_hb = np.nanstd(hb_data[:5], axis=0)

    hb_sub = hb_data - cont_hb[None, :, :]
    hb_win = (vel_hb >= -350.0) & (vel_hb <= 350.0)
    hb_pos = np.where(np.isfinite(hb_sub[hb_win]) & (hb_sub[hb_win] > 0), hb_sub[hb_win], 0.0)

    flux_hb_phys = np.sum(hb_pos, axis=0) * d_spec_hb
    hb_flux_err = std_hb * np.sqrt(np.sum(hb_win)) * d_spec_hb

    with np.errstate(invalid="ignore", divide="ignore"):
        snr_hb = np.where(np.isfinite(hb_flux_err) & (hb_flux_err > 0), flux_hb_phys / hb_flux_err, 0.0)

    valid_ha = np.isfinite(ha_flux_map) & (ha_flux_map > 0)
    valid_balmer = valid_ha & (snr_hb >= snr_thresh) & (flux_hb_phys > 0)

    with np.errstate(invalid="ignore", divide="ignore"):
        balmer_dec = np.where(valid_balmer, ha_flux_map / flux_hb_phys, np.nan)
        raw_ebv = (2.5 / (k_hb - k_ha)) * np.log10(balmer_dec / 2.86)
        ebv = np.where(valid_balmer, np.maximum(0.0, raw_ebv), np.nan)
        av_map = 3.1 * ebv

        # Apply optional upper A_V cap to prevent exponential noise amplification
        n_capped = 0
        if max_av is not None and max_av > 0:
            n_capped = int(np.sum(np.isfinite(av_map) & (av_map > max_av)))
            av_map = np.where(valid_balmer, np.minimum(av_map, float(max_av)), np.nan)
            ebv = av_map / 3.1

        aha_map = k_ha * ebv

        # Two-band continuum-noise propagation into E(B-V), A_V, and A(H-alpha)
        rel_err_ha = np.where(valid_ha, ha_flux_err_map / ha_flux_map, np.nan)
        rel_err_hb = np.where(valid_balmer, hb_flux_err / flux_hb_phys, np.nan)
        rel_err_balmer = np.sqrt(rel_err_ha**2 + rel_err_hb**2)

        ebv_err = np.where(valid_balmer, (2.5 / ((k_hb - k_ha) * ln10)) * rel_err_balmer, np.nan)
        av_err_map = 3.1 * ebv_err
        aha_err_map = k_ha * ebv_err

    # Fallback median extinction & median uncertainty for spaxels without valid H-beta SNR
    n_balmer = int(np.sum(valid_balmer))
    if n_balmer > 0:
        median_aha = float(np.nanmedian(aha_map))
        median_av = float(np.nanmedian(av_map))
        # Standard error on the median A(H-alpha) across valid Balmer spaxels
        median_aha_err = 1.253 * float(np.nanstd(aha_map)) / np.sqrt(n_balmer)
    else:
        median_aha, median_av, median_aha_err = 0.0, 0.0, 0.0

    aha_filled = np.where(np.isfinite(aha_map), aha_map, median_aha)
    aha_err_filled = np.where(np.isfinite(aha_err_map), aha_err_map, median_aha_err)

    corr_factor = 10.0 ** (0.4 * aha_filled)
    ha_flux_corr = np.where(valid_ha, ha_flux_map * corr_factor, np.nan)

    # Full per-spaxel corrected flux uncertainty combining H-alpha flux noise + A(H-alpha) noise
    with np.errstate(invalid="ignore", divide="ignore"):
        rel_err_corr = np.sqrt(rel_err_ha**2 + (0.4 * ln10 * aha_err_filled)**2)
        ha_flux_corr_err = np.where(valid_ha, ha_flux_corr * rel_err_corr, np.nan)

    hb_flux_map = np.where(valid_balmer, flux_hb_phys, np.nan)

    cap_note = f" | Capped {n_capped} spaxels at A_V <= {max_av:.1f} mag" if (max_av is not None and max_av > 0) else ""
    print(
        f"-> CCM89 Extinction (N={n_balmer} Balmer spaxels): "
        f"Median E(B-V) = {np.nanmedian(ebv):.3f} mag | "
        f"Median A_V = {median_av:.3f} ± {np.nanmedian(av_err_map):.3f} mag{cap_note}"
    )

    return {
        "hb_flux": hb_flux_map,
        "balmer_dec": balmer_dec,
        "ebv": ebv,
        "av": av_map,
        "av_err": av_err_map,
        "aha": aha_map,
        "ha_flux_corr": ha_flux_corr,
        "ha_flux_corr_err": ha_flux_corr_err,
        "median_av": median_av,
    }

def run_kinematic_and_mef_analysis(cube_path, mef_path, csv_path, fig_dir, prefix, target_name, redshift, snr_thresh, hb_cube_path=None, dist_mpc=None, max_av=None):
    """Computes moments, excitation ratios, BPT masks, 6-Gaussian fits, plots, CSV, and 8-ext MEF."""
    c_kms = 299792.458
    fwhm_factor = 2.0 * np.sqrt(2.0 * np.log(2.0))
    wl_nii1 = 0.654805 * (1.0 + redshift)
    wl_ha   = 0.656282 * (1.0 + redshift)
    wl_nii2 = 0.658345 * (1.0 + redshift)

    with fits.open(cube_path) as hdul:
        data, header = None, None
        for hdu in hdul:
            if hdu.data is not None and hdu.data.ndim == 3:
                data = hdu.data.astype(np.float64)
                header = hdu.header
                break
        if data is None:
            raise RuntimeError(f"No 3D cube found in {cube_path}")

    wcs = WCS(header)
    celestial_wcs = wcs.celestial
    n_spec, n_y, n_x = data.shape

    coords_matrix = np.zeros((n_spec, 3))
    coords_matrix[:, 0] = n_x // 2
    coords_matrix[:, 1] = n_y // 2
    coords_matrix[:, 2] = np.arange(n_spec)
    wl = wcs.wcs_pix2world(coords_matrix, 0)[:, 2] * 1e6
    d_lambda = np.abs(np.median(np.diff(wl)))

    vel_ha = c_kms * (wl - wl_ha) / wl_ha
    vel_nii2 = c_kms * (wl - wl_nii2) / wl_nii2
    dv = np.abs(np.median(np.diff(vel_ha)))

    # 1. Continuum subtraction
    cont_mask = (vel_ha < -1300.0) | (vel_ha > 1500.0)
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", category=RuntimeWarning)
        if np.sum(cont_mask) >= 4:
            continuum = np.nanmedian(data[cont_mask], axis=0)
            cont_std = np.nanstd(data[cont_mask], axis=0)
        else:
            continuum = np.nanpercentile(data, 15, axis=0)
            cont_std = np.nanstd(data[:5], axis=0)

    cube_sub = data - continuum[None, :, :]

    # 2. H-alpha Moments 0, 1, 2 & [N II] 6583 Integrated Flux + Surface Brightness
    ha_mask = (vel_ha >= -350.0) & (vel_ha <= 350.0)
    nii_mask = (vel_nii2 >= -350.0) & (vel_nii2 <= 350.0)
    ha_pos = np.where(np.isfinite(cube_sub[ha_mask]) & (cube_sub[ha_mask] > 0), cube_sub[ha_mask], 0.0)
    nii_pos = np.where(np.isfinite(cube_sub[nii_mask]) & (cube_sub[nii_mask] > 0), cube_sub[nii_mask], 0.0)
    sub_vel = vel_ha[ha_mask][:, None, None]

    # Compute exact spaxel sky area in arcsec**2 from celestial WCS
    pixel_area_deg2 = float(proj_plane_pixel_area(celestial_wcs))
    pixel_area_arcsec2 = (pixel_area_deg2 * u.deg**2).to(u.arcsec**2).value
    print(f"-> Spatial pixel area: {pixel_area_arcsec2:.4f} arcsec^2 / spaxel")

    # Convert spectral step d_lambda (um) -> Angstrom and build physical flux & SB units
    raw_bunit_str = header.get("BUNIT", "10**(-20) erg / (s cm**2 Angstrom)").strip()
    if "Angstrom" in raw_bunit_str:
        d_spec = (d_lambda * u.um).to(u.Angstrom).value
        base_flux_unit = raw_bunit_str.replace("/Angstrom", "").replace("/ Angstrom", "").strip()
    else:
        d_spec = d_lambda * 1e4
        base_flux_unit = "10**(-20) erg / (s cm**2)"

    if base_flux_unit.endswith(")"):
        sb_unit = base_flux_unit[:-1] + " arcsec**2)"
    else:
        sb_unit = f"{base_flux_unit} arcsec**(-2)"

    # Integrated flux per spaxel [10**(-20) erg / (s cm**2)]
    flux_ha_phys = np.sum(ha_pos, axis=0) * d_spec
    flux_nii_phys = np.sum(nii_pos, axis=0) * d_spec

    # Surface brightness [10**(-20) erg / (s cm**2 arcsec**2)]
    sb_ha = flux_ha_phys / pixel_area_arcsec2
    sb_nii = flux_nii_phys / pixel_area_arcsec2

    with np.errstate(invalid="ignore", divide="ignore"):
        mom1 = np.sum(ha_pos * sub_vel, axis=0) / np.sum(ha_pos, axis=0)
        mom2 = np.sqrt(np.sum(ha_pos * (sub_vel - mom1[None, :, :]) ** 2, axis=0) / np.sum(ha_pos, axis=0))

        snr_ha = np.where(cont_std > 0, flux_ha_phys / (cont_std * np.sqrt(np.sum(ha_mask)) * d_spec), 0.0)
        snr_nii = np.where(cont_std > 0, flux_nii_phys / (cont_std * np.sqrt(np.sum(nii_mask)) * d_spec), 0.0)

        valid_ha = (snr_ha >= snr_thresh) & (flux_ha_phys > 0)
        valid_both = valid_ha & (snr_nii >= snr_thresh) & (flux_nii_phys > 0)

        # Surface brightness maps
        mom0_map = np.where(valid_ha, sb_ha, np.nan)
        nii_map = np.where(valid_both, sb_nii, np.nan)

        # Integrated flux per spaxel maps
        ha_flux_map = np.where(valid_ha, flux_ha_phys, np.nan)
        nii_flux_map = np.where(valid_both, flux_nii_phys, np.nan)

        # Kinematic moment maps
        mom1_map = np.where(valid_ha, mom1, np.nan)
        mom2_map = np.where(valid_ha, mom2, np.nan)

        # Line ratios
        ratio_lin = np.where(valid_both, flux_nii_phys / flux_ha_phys, np.nan)
        ratio_log = np.log10(ratio_lin)

    ha_flux_err_phys = cont_std * np.sqrt(np.sum(ha_mask)) * d_spec
    ha_flux_err_map = np.where(valid_ha, ha_flux_err_phys, np.nan)

    if hb_cube_path and os.path.exists(hb_cube_path):
        dust = compute_ccm89_extinction_maps(
            hb_cube_path=hb_cube_path,
            ha_flux_map=ha_flux_map,
            ha_flux_err_map=ha_flux_err_map,
            redshift=redshift,
            d_spec_ref=d_spec,
            snr_thresh=snr_thresh,
            max_av=max_av

        )
        ha_flux_corr_map = dust["ha_flux_corr"]
        ha_flux_corr_err_map = dust["ha_flux_corr_err"]
        av_map = dust["av"]
        av_err_map = dust["av_err"]
        ebv_map = dust["ebv"]
        balmer_map = dust["balmer_dec"]

        # ---> INSERT NEW 3-PANEL CCM89 DUST FIGURE BLOCK HERE <---
        if np.any(np.isfinite(av_map)):
            fig_dust = plt.figure(figsize=(20, 5.8))

            # PANEL 1: Observed Balmer Decrement (H-alpha / H-beta)
            ax_d1 = fig_dust.add_axes([0.05, 0.16, 0.24, 0.68], projection=celestial_wcs)
            bd_min, bd_max = np.nanpercentile(balmer_map, [2.0, 98.0])
            im_d1 = ax_d1.imshow(balmer_map, cmap="YlOrRd", origin="lower", vmin=max(2.0, bd_min), vmax=bd_max)
            ax_d1.contour(balmer_map, levels=[2.86], colors="navy", linestyles="--", linewidths=1.2)
            ax_d1.set_title(r"1. Balmer Decrement (H$\alpha$ / H$\beta$)", fontweight="bold")
            ax_d1.set_xlabel("RA (J2000)")
            ax_d1.set_ylabel("Dec (J2000)")
            ax_d1.grid(color="gray", ls=":", alpha=0.3)
            cax_d1 = fig_dust.add_axes([0.30, 0.16, 0.01, 0.68])
            cbar_d1 = fig_dust.colorbar(im_d1, cax=cax_d1, label=r"Observed H$\alpha$ / H$\beta$ Ratio")
            cbar_d1.ax.axhline(2.86, color="navy", ls="--", lw=1.2)

            # PANEL 2: CCM89 Visual Dust Extinction A_V [mag]
            ax_d2 = fig_dust.add_axes([0.38, 0.16, 0.24, 0.68], projection=celestial_wcs)
            av_min, av_max = np.nanpercentile(av_map, [2.0, 98.0])
            im_d2 = ax_d2.imshow(av_map, cmap="magma_r", origin="lower", vmin=max(0.0, av_min), vmax=av_max)
            ax_d2.contour(av_map, levels=4, colors="white", alpha=0.45, linewidths=0.8)
            ax_d2.set_title(r"2. CCM89 Visual Extinction ($A_V$)", fontweight="bold")
            ax_d2.set_xlabel("RA (J2000)")
            ax_d2.set_ylabel("")
            ax_d2.grid(color="gray", ls=":", alpha=0.3)
            cax_d2 = fig_dust.add_axes([0.63, 0.16, 0.01, 0.68])
            fig_dust.colorbar(im_d2, cax=cax_d2, label=r"$A_V$ [mag] ($R_V = 3.1$)")

            # PANEL 3: Dust-Corrected H-alpha Flux per Spaxel
            ax_d3 = fig_dust.add_axes([0.71, 0.16, 0.24, 0.68], projection=celestial_wcs)
            fc_min, fc_max = np.nanpercentile(ha_flux_corr_map, [2.0, 98.0])
            im_d3 = ax_d3.imshow(ha_flux_corr_map, cmap="inferno", origin="lower", vmin=fc_min, vmax=fc_max)
            ax_d3.set_title(r"3. Dust-Corrected H$\alpha$ Flux", fontweight="bold")
            ax_d3.set_xlabel("RA (J2000)")
            ax_d3.set_ylabel("")
            ax_d3.grid(color="gray", ls=":", alpha=0.3)
            cax_d3 = fig_dust.add_axes([0.96, 0.16, 0.01, 0.68])
            fig_dust.colorbar(im_d3, cax=cax_d3, label=f"Corrected Flux [{base_flux_unit}]")

            n_balmer_valid = int(np.sum(np.isfinite(av_map)))
            med_av_title = float(np.nanmedian(av_map))
            fig_dust.suptitle(
                f"CCM89 Balmer Decrement Dust Attenuation: {target_name} "
                f"(N={n_balmer_valid} Balmer Spaxels, Median A_V = {med_av_title:.2f} mag)",
                fontsize=14, fontweight="bold"
            )

            dust_fig_path = os.path.join(fig_dir, f"{prefix}_ccm89_dust_extinction.png")
            fig_dust.savefig(dust_fig_path, dpi=250, bbox_inches="tight", facecolor="white")
            plt.close(fig_dust)
            print(f"Saved 3-panel CCM89 dust diagnostic figure: {dust_fig_path}")
            # ---> END INSERT <---
        median_av_val = dust["median_av"]
    else:
        ha_flux_corr_map = ha_flux_map.copy()
        ha_flux_corr_err_map = ha_flux_err_map.copy()
        av_map = np.full_like(ha_flux_map, np.nan)
        av_err_map = np.full_like(ha_flux_map, np.nan)
        ebv_map = np.full_like(ha_flux_map, np.nan)
        balmer_map = np.full_like(ha_flux_map, np.nan)
        median_av_val = 0.0

    # 3. Excitation Mask Classification
    is_hii = valid_both & (ratio_log < -0.30) & (mom2_map < 105.0)
    is_shock = valid_both & (ratio_log >= -0.20) & (mom2_map >= 100.0)
    is_comp = valid_both & (~is_hii) & (~is_shock)

    mask_int = np.zeros((n_y, n_x), dtype=np.int16)
    mask_int[is_hii] = 1
    mask_int[is_comp] = 2
    mask_int[is_shock] = 3

    # 4. Save 6-Panel Moment & Excitation Diagnostic Plot
    fig = plt.figure(figsize=(20, 11))

    panels = [
        (mom0_map, "inferno", "1. Hα Integrated Flux (Moment 0)", [0.05, 0.56, 0.24, 0.34]),
        (mom1_map, "RdBu_r", "2. LOS Velocity Δv [km/s] (Moment 1)", [0.38, 0.56, 0.24, 0.34]),
        (mom2_map, "viridis", "3. Velocity Dispersion σ [km/s] (Moment 2)", [0.71, 0.56, 0.24, 0.34]),
        (nii_map, "magma", "4. [N II] λ6583 Integrated Flux", [0.05, 0.10, 0.24, 0.34]),
        (ratio_lin, "plasma", "5. Linear [N II] λ6583 / Hα Ratio", [0.38, 0.10, 0.24, 0.34]),
        (ratio_log, "coolwarm", "6. log10([N II] / Hα) Excitation Map", [0.71, 0.10, 0.24, 0.34])
    ]

    for arr, cmap, title, rect in panels:
        ax = fig.add_axes(rect, projection=celestial_wcs)
        if "Moment 1" in title:
            vlim = np.nanpercentile(np.abs(arr), 96.0)
            vmin, vmax = -vlim, vlim
        elif "log10" in title:
            vmin, vmax = -0.6, 0.2
        else:
            vmin, vmax = np.nanpercentile(arr, [2.0, 98.0])
            im = ax.imshow(arr, cmap=cmap, origin="lower", vmin=vmin, vmax=vmax)
            ax.set_title(title, fontweight="bold")
            ax.set_xlabel("RA (J2000)")
            ax.set_ylabel("Dec (J2000)")
            cax = fig.add_axes([rect[0] + rect[2] + 0.01, rect[1], 0.01, rect[3]])
            fig.colorbar(im, cax=cax)

        fig.suptitle(f"Kinematic & Ionization Maps: {target_name} (z = {redshift:.6f})", fontsize=15, fontweight="bold")
        fig.savefig(os.path.join(fig_dir, f"{prefix}_kinematic_maps.png"), dpi=250, bbox_inches="tight", facecolor="white")
        plt.close(fig)

    # 5. Save BPT-Style Scatter & Spatial Regime Figure
    fig_bpt = plt.figure(figsize=(15.5, 6.2))
    ax_sc = fig_bpt.add_axes([0.07, 0.14, 0.45, 0.75])
    sc = ax_sc.scatter(
        mom2_map[valid_both], ratio_log[valid_both], c=np.log10(mom0_map[valid_both]),
        cmap="inferno", s=16, alpha=0.78, edgecolors="none"
    )
    ax_sc.axhline(-0.30, color="royalblue", ls="--", lw=1.3, label="H II / Composite (-0.30)")
    ax_sc.axhline(-0.20, color="crimson", ls=":", lw=1.3, label="Shock Threshold (-0.20)")
    ax_sc.axvline(105.0, color="darkgreen", ls="-.", lw=1.3, label="Dispersion Threshold (105 km/s)")
    ax_sc.set_xlabel(r"Velocity Dispersion $\sigma$ [km s$^{-1}$]")
    ax_sc.set_ylabel(r"$\log_{10}$([N II] $\lambda$6583 / H$\alpha$)")
    ax_sc.set_title("1. Kinematic BPT-Style Spaxel Scatter", fontweight="bold")
    ax_sc.grid(True, ls=":", alpha=0.5)
    ax_sc.legend(loc="lower right", fontsize=8.5)
    fig_bpt.colorbar(sc, ax=ax_sc, pad=0.02, label=r"$\log_{10}$(H$\alpha$ Flux)")

    ax_map = fig_bpt.add_axes([0.60, 0.14, 0.34, 0.75], projection=celestial_wcs)
    class_plot = np.where(mask_int > 0, mask_int.astype(float), np.nan)
    ax_map.imshow(class_plot, cmap=ListedColormap(["#2b5c8f", "#f2b134", "#c0392b"]), origin="lower", vmin=0.5, vmax=3.5)
    ax_map.set_xlabel("RA (J2000)")
    ax_map.set_ylabel("Dec (J2000)")
    ax_map.set_title("2. Spatial Excitation Regimes", fontweight="bold")
    ax_map.legend(handles=[
        mpatches.Patch(color="#2b5c8f", label=f"H II Starburst ({np.sum(is_hii)})"),
        mpatches.Patch(color="#f2b134", label=f"Composite ({np.sum(is_comp)})"),
        mpatches.Patch(color="#c0392b", label=f"Shock / Outflow ({np.sum(is_shock)})")
    ], loc="upper right", fontsize=8.5)
    fig_bpt.savefig(os.path.join(fig_dir, f"{prefix}_bpt_scatter.png"), dpi=250, bbox_inches="tight", facecolor="white")
    plt.close(fig_bpt)

    # 6. Multi-Gaussian Decomposition, Integrated H-alpha Luminosity, SFR & CSV Export
    if dist_mpc is not None and dist_mpc > 0:
        d_l_mpc = float(dist_mpc)
    elif redshift > 0:
        d_l_mpc = float(Planck18.luminosity_distance(redshift).to(u.Mpc).value)
    else:
        d_l_mpc = 10.0  # Safe fallback for z <= 0

    d_l_cm = (d_l_mpc * u.Mpc).to(u.cm).value
    geom_area_cm2 = 4.0 * np.pi * (d_l_cm ** 2)

    # Detect 10**(-20) scale factor from parent cube BUNIT
    flux_scale = 1e-20 if "10**(-20)" in raw_bunit_str or "1e-20" in raw_bunit_str.lower() else 1.0
    sfr_coeff = 5.37e-42  # Murphy et al. (2011) / Kennicutt & Evans (2012) Chabrier IMF [Msun/yr / (erg/s)]

    print(f"-> Luminosity Distance D_L = {d_l_mpc:.3f} Mpc (4*pi*D_L^2 = {geom_area_cm2:.3e} cm^2)")

    def double_triplet_model(w, c0, c1, a_ha_n, a_nii2_n, v_n, sig_n, a_ha_b, a_nii2_b, v_b, sig_b):
        cont = c0 + c1 * (w - wl_ha)

        def comp(a_ha, a_nii2, v_kms, sig_kms):
            a_nii1 = a_nii2 / 2.96
            shift = 1.0 + (v_kms / c_kms)
            mu1, mu2, mu3 = wl_nii1 * shift, wl_ha * shift, wl_nii2 * shift
            s1, s2, s3 = mu1 * (sig_kms / c_kms), mu2 * (sig_kms / c_kms), mu3 * (sig_kms / c_kms)
            return (
                a_nii1 * np.exp(-0.5 * ((w - mu1) / s1) ** 2) +
                a_ha * np.exp(-0.5 * ((w - mu2) / s2) ** 2) +
                a_nii2 * np.exp(-0.5 * ((w - mu3) / s3) ** 2)
            )

        return cont + comp(a_ha_n, a_nii2_n, v_n, sig_n) + comp(a_ha_b, a_nii2_b, v_b, sig_b)

    regimes = [(1, "HII_Starburst"), (2, "Composite_Interface"), (3, "Shock_Outflow")]
    csv_rows = []
    for code, label in regimes:
        spax = (mask_int == code)
        n_spax = int(np.sum(spax))
        if n_spax == 0:
            continue

        # --- Integrated Flux, Luminosity & SFR across all spaxels in this regime ---
        total_flux_scaled = float(np.nansum(ha_flux_map[spax]))
        total_flux_cgs = total_flux_scaled * flux_scale
        total_lum_ha = total_flux_cgs * geom_area_cm2
        sfr_ha = sfr_coeff * total_lum_ha

        # Propagate 1-sigma flux uncertainty over the regime aperture
        spax_flux_err = cont_std[spax] * np.sqrt(np.sum(ha_mask)) * d_spec
        total_flux_err_cgs = float(np.sqrt(np.nansum(spax_flux_err ** 2))) * flux_scale
        total_lum_ha_err = total_flux_err_cgs * geom_area_cm2
        sfr_ha_err = sfr_coeff * total_lum_ha_err

        # CCM89 Dust-Corrected Luminosity, SFR & Median A_V
        corr_flux_scaled = float(np.nansum(ha_flux_corr_map[spax]))
        corr_flux_cgs = corr_flux_scaled * flux_scale
        corr_lum_ha = corr_flux_cgs * geom_area_cm2
        corr_sfr_ha = sfr_coeff * corr_lum_ha
        regime_av = float(np.nanmedian(av_map[spax])) if np.any(np.isfinite(av_map[spax])) else dust["median_av"]

        # Quadrature sum of per-spaxel corrected flux errors (including H-alpha + H-beta noise)
        corr_flux_err_cgs = float(np.sqrt(np.nansum(ha_flux_corr_err_map[spax] ** 2))) * flux_scale
        corr_lum_ha_err = corr_flux_err_cgs * geom_area_cm2
        corr_sfr_ha_err = sfr_coeff * corr_lum_ha_err

        # Propagate 1-sigma uncertainty through spaxel correction factors
        with np.errstate(invalid="ignore", divide="ignore"):
            spax_corr_factor = np.where(
                ha_flux_map[spax] > 0,
                ha_flux_corr_map[spax] / ha_flux_map[spax],
                1.0
            )

        # --- Multi-Gaussian Kinematic Decomposition ---
        mean_spec = np.nanmean(data[:, spax], axis=1)
        spec_err = np.nanstd(data[:, spax], axis=1) / np.sqrt(n_spax)
        spec_err = np.where(spec_err > 0, spec_err, np.nanmedian(spec_err[spec_err > 0]))

        c0_init = np.nanpercentile(mean_spec, 15)
        pk_init = np.nanmax(mean_spec) - c0_init
        p0 = [c0_init, 0.0, 0.70 * pk_init, 0.50 * pk_init, 0.0, 65.0, 0.30 * pk_init, 0.30 * pk_init, -20.0, 180.0]
        bounds = (
            [-np.inf, -np.inf, 0.0, 0.0, -300.0, 20.0, 0.0, 0.0, -500.0, 100.0],
            [ np.inf,  np.inf, np.inf, np.inf, 300.0, 130.0, np.inf, np.inf, 500.0, 600.0]
        )

        popt, pcov = curve_fit(double_triplet_model, wl, mean_spec, p0=p0, sigma=spec_err, bounds=bounds, maxfev=25000)
        perr = np.sqrt(np.diag(pcov))

        _, _, a_ha_n, a_nii2_n, v_n, sig_n, a_ha_b, a_nii2_b, v_b, sig_b = popt
        _, _, _, _, v_n_err, sig_n_err, _, _, v_b_err, sig_b_err = perr

        fwhm_n, fwhm_n_err = fwhm_factor * sig_n, fwhm_factor * sig_n_err
        fwhm_b, fwhm_b_err = fwhm_factor * sig_b, fwhm_factor * sig_b_err

        dv_out = v_b - v_n
        dv_out_err = np.sqrt(max(float(pcov[(8, 8)]) + float(pcov[(4, 4)]) - 2.0 * float(pcov[(4, 8)]), 0.0))

        broad_frac_ha = 100.0 * (a_ha_b * sig_b) / (a_ha_n * sig_n + a_ha_b * sig_b)
        broad_frac_nii = 100.0 * (a_nii2_b * sig_b) / (a_nii2_n * sig_n + a_nii2_b * sig_b)

        r_n = a_nii2_n / a_ha_n
        r_b = a_nii2_b / a_ha_b

        r_n_err = np.sqrt(max((r_n ** 2) * (
            float(pcov[(3, 3)]) / (a_nii2_n ** 2) +
            float(pcov[(2, 2)]) / (a_ha_n ** 2) -
            2.0 * float(pcov[(2, 3)]) / (a_ha_n * a_nii2_n)
        ), 0.0))
        r_b_err = np.sqrt(max((r_b ** 2) * (
            float(pcov[(7, 7)]) / (a_nii2_b ** 2) +
            float(pcov[(6, 6)]) / (a_ha_b ** 2) -
            2.0 * float(pcov[(6, 7)]) / (a_ha_b * a_nii2_b)
        ), 0.0))

        log_rn, log_rb = np.log10(r_n), np.log10(r_b)
        log_rn_err = r_n_err / (r_n * np.log(10.0))
        log_rb_err = r_b_err / (r_b * np.log(10.0))

        enh = r_b / r_n
        enh_err = enh * np.sqrt((r_b_err / r_b) ** 2 + (r_n_err / r_n) ** 2)

        print(
            f"-> {label} (N={n_spax}): L_Ha(corr) = {corr_lum_ha:.3e} ± {corr_lum_ha_err:.3e} erg/s | "
            f"SFR(corr) = {corr_sfr_ha:.4f} ± {corr_sfr_ha_err:.4f} Msun/yr (A_V={regime_av:.2f} mag)"
        )

        csv_rows.append({
            "Target": target_name, "Redshift_z": round(redshift, 6), "Dist_Mpc": round(d_l_mpc, 3),
            "Regime_Code": code, "Regime_Label": label, "N_Spaxels": n_spax,
            "Total_Flux_Ha_cgs": f"{total_flux_cgs:.4e}", "Total_Flux_Ha_err_cgs": f"{total_flux_err_cgs:.4e}",
            "Lum_Ha_erg_s": f"{total_lum_ha:.4e}", "Lum_Ha_err_erg_s": f"{total_lum_ha_err:.4e}",
            "SFR_Ha_Msun_yr": round(sfr_ha, 5), "SFR_Ha_err_Msun_yr": round(sfr_ha_err, 5),
            "Median_AV_mag": round(regime_av, 3),
            "Lum_Ha_Corr_erg_s": f"{corr_lum_ha:.4e}", "Lum_Ha_Corr_err_erg_s": f"{corr_lum_ha_err:.4e}",
            "SFR_Ha_Corr_Msun_yr": round(corr_sfr_ha, 5), "SFR_Ha_Corr_err_Msun_yr": round(corr_sfr_ha_err, 5),
            "v_narrow_kms": round(v_n, 2), "v_narrow_err_kms": round(v_n_err, 2),
            "FWHM_narrow_kms": round(fwhm_n, 2), "FWHM_narrow_err_kms": round(fwhm_n_err, 2),
            "v_broad_kms": round(v_b, 2), "v_broad_err_kms": round(v_b_err, 2),
            "FWHM_broad_kms": round(fwhm_b, 2), "FWHM_broad_err_kms": round(fwhm_b_err, 2),
            "Delta_v_outflow_kms": round(dv_out, 2), "Delta_v_outflow_err_kms": round(dv_out_err, 2),
            "Broad_Flux_Frac_Ha_pct": round(broad_frac_ha, 2), "Broad_Flux_Frac_NII_pct": round(broad_frac_nii, 2),
            "NII_Ha_Ratio_Narrow": round(r_n, 4), "NII_Ha_Ratio_Narrow_err": round(r_n_err, 4),
            "NII_Ha_Ratio_Broad": round(r_b, 4), "NII_Ha_Ratio_Broad_err": round(r_b_err, 4),
            "Log10_NII_Ha_Narrow": round(log_rn, 4), "Log10_NII_Ha_Narrow_err": round(log_rn_err, 4),
            "Log10_NII_Ha_Broad": round(log_rb, 4), "Log10_NII_Ha_Broad_err": round(log_rb_err, 4),
            "Shock_Enhancement_Factor": round(enh, 3), "Shock_Enhancement_err": round(enh_err, 3),
        })

        # Plot Class 3 Outflow Decomposition Figure
        if code == 3:
            c0, c1 = float(popt[0]), float(popt[1])
            wl_fine = np.linspace(wl.min(), wl.max(), 1000)
            cont_f = c0 + c1 * (wl_fine - wl_ha)
            nar_f = double_triplet_model(wl_fine, 0, 0, a_ha_n, a_nii2_n, v_n, sig_n, 0, 0, v_b, sig_b)
            brd_f = double_triplet_model(wl_fine, 0, 0, 0, 0, v_n, sig_n, a_ha_b, a_nii2_b, v_b, sig_b)

            fig_g, ax_g = plt.subplots(figsize=(12, 5.5))
            ax_g.step(wl, mean_spec, where="mid", color="black", lw=1.6, label=f"Class 3 Outflow Spectrum (N={n_spax})")
            ax_g.plot(wl_fine, cont_f + nar_f + brd_f, color="crimson", lw=2.2, label="Total 6-Gaussian Fit")
            ax_g.plot(wl_fine, cont_f + nar_f, color="royalblue", ls="--", lw=1.8, label=f"Narrow Disk (FWHM={fwhm_n:.0f} km/s)")
            ax_g.plot(wl_fine, cont_f + brd_f, color="darkorange", ls="-.", lw=2.0, label=f"Broad Outflow (FWHM={fwhm_b:.0f} km/s)")
            ax_g.fill_between(wl_fine, cont_f, cont_f + brd_f, color="darkorange", alpha=0.18)
            ax_g.set_xlabel(r"Observed Wavelength [$\mu$m]")
            ax_g.set_ylabel("Mean Flux / Spaxel")
            ax_g.set_title(f"Two-Component Kinematic Decomposition: {target_name} (Enhancement = {enh:.2f}x)", fontweight="bold")
            ax_g.legend(loc="upper left")
            ax_g.grid(True, ls=":", alpha=0.5)
            fig_g.savefig(os.path.join(fig_dir, f"{prefix}_outflow_fit.png"), dpi=250, bbox_inches="tight", facecolor="white")
            plt.close(fig_g)

    # Save CSV
    if csv_rows:
        os.makedirs(os.path.dirname(csv_path), exist_ok=True)
        with open(csv_path, "w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=list(csv_rows[0].keys()))
            writer.writeheader()
            writer.writerows(csv_rows)
        print(f"Exported CSV summary with Luminosity & SFR: {csv_path}")

    # 7. Write Complete 8-Extension MEF FITS File with Uniform Physical BUNIT Headers
    os.makedirs(os.path.dirname(mef_path), exist_ok=True)

    base_wcs_hdr = celestial_wcs.to_header()
    base_wcs_hdr["OBJECT"] = target_name
    base_wcs_hdr["REDSHIFT"] = redshift
    base_wcs_hdr["PIXAREA"] = (round(pixel_area_arcsec2, 6), "Spaxel area [arcsec**2]")

    def build_image_hdu(arr, extname, bunit, comment, extra_cards=None):
        hdr = base_wcs_hdr.copy()
        # FITS card max is 80 chars: 'BUNIT   = ' (10) + quotes/padding (2) + ' / ' (3) = 15 overhead
        max_comment_len = max(0, 80 - 15 - max(len(str(bunit)), 8))
        hdr["BUNIT"] = (bunit, comment[:max_comment_len])
        if extra_cards:
            for key, val in extra_cards.items():
                hdr[key] = val
        return fits.ImageHDU(arr, header=hdr, name=extname)

    primary = fits.PrimaryHDU()
    primary.header["TARGET"] = target_name
    primary.header["REDSHIFT"] = redshift

    mask_cards = {
        "CLASS_0": ("MASKED_LOW_SNR", "Invalid or below SNR threshold"),
        "CLASS_1": ("HII_STARBURST", "log10([NII]/Ha) < -0.30 & sigma < 105 km/s"),
        "CLASS_2": ("COMPOSITE_INTERFACE", "Intermediate excitation / turbulence"),
        "CLASS_3": ("SHOCK_OUTFLOW", "log10([NII]/Ha) >= -0.20 & sigma >= 100 km/s"),
    }

    av_comment = f"CCM89 A_V (capped <= {max_av} mag)" if max_av else "CCM89 visual extinction A_V"

    hdus = [
        primary,
        build_image_hdu(
            mom0_map.astype(np.float32),
            "HALPHA_SB",
            sb_unit,
            "H-alpha surface brightness"
        ),
        build_image_hdu(
            ha_flux_map.astype(np.float32),
            "HALPHA_FLUX",
            base_flux_unit,
            "H-alpha flux per spaxel"
        ),
        build_image_hdu(
            mom1_map.astype(np.float32),
            "HALPHA_MOM1",
            "km / s",
            "H-alpha LOS velocity"
        ),
        build_image_hdu(
            mom2_map.astype(np.float32),
            "HALPHA_MOM2",
            "km / s",
            "H-alpha velocity dispersion"
        ),
        build_image_hdu(
            nii_map.astype(np.float32),
            "NII6583_SB",
            sb_unit,
            "[N II] surface brightness"
        ),
        build_image_hdu(
            nii_flux_map.astype(np.float32),
            "NII6583_FLUX",
            base_flux_unit,
            "[N II] flux per spaxel"
        ),
        build_image_hdu(
            ratio_lin.astype(np.float32),
            "NII_HA_RATIO",
            "",
            "[N II] / H-alpha flux ratio"
        ),
        build_image_hdu(
            ratio_log.astype(np.float32),
            "LOG_NII_HA",
            "",
            "Log10([N II] / H-alpha)"
        ),
        build_image_hdu(
            mask_int,
            "EXCITATION_MASK",
            "code",
            "Excitation regime code",
            extra_cards=mask_cards
        ),
        build_image_hdu(
            ha_flux_corr_map.astype(np.float32),
            "HALPHA_FLUX_CORR",
            base_flux_unit,
            "Dust-corrected H-alpha flux"
        ),
        build_image_hdu(
            balmer_map.astype(np.float32),
            "BALMER_DEC",
            "",
            "Observed H-alpha / H-beta"
        ),
        build_image_hdu(
            ebv_map.astype(np.float32),
            "EBV_CCM89",
            "mag",
            "CCM89 color excess E(B-V)"
        ),
        build_image_hdu(
            av_map.astype(np.float32),
            "AV_CCM89",
            "mag",
            av_comment
        ),
        build_image_hdu(
            av_err_map.astype(np.float32),
            "AV_CCM89_ERR",
            "mag",
            "1-sigma uncertainty on A_V"
        )
    ]

    summary_tbl = Table.read(csv_path, format="csv")
    for col in [
        "v_narrow_kms", "v_narrow_err_kms", "FWHM_narrow_kms", "FWHM_narrow_err_kms",
        "v_broad_kms", "v_broad_err_kms", "FWHM_broad_kms", "FWHM_broad_err_kms",
        "Delta_v_outflow_kms", "Delta_v_outflow_err_kms"
    ]:
        if col in summary_tbl.colnames:
            summary_tbl[col].unit = "km / s"

    for col in ["Broad_Flux_Frac_Ha_pct", "Broad_Flux_Frac_NII_pct"]:
        if col in summary_tbl.colnames:
            summary_tbl[col].unit = "percent"

    unit_map = {
        "Dist_Mpc": "Mpc",
        "Total_Flux_Ha_cgs": "erg / (s cm**2)",
        "Total_Flux_Ha_err_cgs": "erg / (s cm**2)",
        "Lum_Ha_erg_s": "erg / s",
        "Lum_Ha_err_erg_s": "erg / s",
        "SFR_Ha_Msun_yr": "solMass / yr",
        "SFR_Ha_err_Msun_yr": "solMass / yr",
        "Median_AV_mag": "mag",
        "Lum_Ha_Corr_erg_s": "erg / s",
        "Lum_Ha_Corr_err_erg_s": "erg / s",
        "SFR_Ha_Corr_Msun_yr": "solMass / yr",
        "SFR_Ha_Corr_err_Msun_yr": "solMass / yr"
    }
    for col, unit_str in unit_map.items():
        if col in summary_tbl.colnames:
            summary_tbl[col].unit = unit_str

    hdus.append(fits.BinTableHDU(data=summary_tbl, name="KINEMATIC_SUMMARY"))
    hdul_out = fits.HDUList(hdus)
    hdul_out.writeto(mef_path, overwrite=True)
    print(f"Exported 11-extension MEF FITS ({sb_unit}): {mef_path}")
    hdul_out.info()

def export_summary_to_latex(csv_path: str, tex_path: str):
    """
    Converts a pipeline kinematic summary CSV (single-target or master batch)
    into a publication-ready LaTeX table* (.tex) using booktabs and math-mode +/- errors.
    """
    if not os.path.exists(csv_path):
        print(f"Warning: CSV file not found for LaTeX export: {csv_path}")
        return

    with open(csv_path, mode="r", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))

    if not rows:
        print(f"Warning: No rows found in {csv_path}; skipping LaTeX export.")
        return

    def fmt_sci_latex(val_str: str, err_str: str = None) -> str:
        """Converts '8.128e+40' into '8.13 \\times 10^{40}' with optional +/- error."""
        try:
            val = float(val_str)
            if val == 0.0 or not np.isfinite(val):
                return "0.0"
            exp = int(np.floor(np.log10(abs(val))))
            mant = val / (10.0 ** exp)
            if err_str is not None:
                err = float(err_str)
                err_mant = err / (10.0 ** exp)
                return rf"${mant:.2f} \pm {err_mant:.2f} \times 10^{{{exp}}}$"
            return rf"${mant:.2f} \times 10^{{{exp}}}$"
        except Exception:
            return str(val_str)

    def fmt_pm(val_str: str, err_str: str, dec: int = 2) -> str:
        """Formats value and error as '$val \\pm err$'."""
        try:
            v, e = float(val_str), float(err_str)
            return rf"${v:.{dec}f} \pm {e:.{dec}f}$"
        except Exception:
            return str(val_str)

    regime_display = {
        "HII_Starburst": r"\ion{H}{ii} Starburst",
        "Composite_Interface": "Composite / Interface",
        "Shock_Outflow": "Shock / Outflow",
    }

    latex_lines = [
        r"% Auto-generated by eso_ifs_pipeline.py (requires \usepackage{booktabs})",
        r"\begin{table*}",
        r"\centering",
        r"\caption{Resolved Kinematic Decomposition, CCM89 Extinction, and Star Formation Properties}",
        r"\label{tab:ifs_kinematic_summary}",
        r"\resizebox{\textwidth}{!}{%",
        r"\begin{tabular}{llccccccc}",
        r"\toprule",
        r"Target & Excitation Regime & $N_{\mathrm{spax}}$ & $A_V$ & $\mathrm{SFR}_{\mathrm{corr}}$ & $\mathrm{FWHM}_{\mathrm{narrow}}$ & $\mathrm{FWHM}_{\mathrm{broad}}$ & $([\mathrm{N\,\textsc{ii}}]/\mathrm{H}\alpha)_{\mathrm{broad}}$ & Shock Enh. \\",
        r" & & & [mag] & [$M_\odot\,\mathrm{yr}^{-1}$] & [$\mathrm{km\,s^{-1}}$] & [$\mathrm{km\,s^{-1}}$] & & Factor \\",
        r"\midrule",
    ]

    for row in rows:
        target_tex = str(row.get("Target", "")).replace("_", r"\_")
        raw_regime = str(row.get("Regime_Label", ""))
        regime_tex = regime_display.get(raw_regime, raw_regime.replace("_", " "))
        n_spax = row.get("N_Spaxels", "0")

        av_str = f"${float(row['Median_AV_mag']):.2f}$" if "Median_AV_mag" in row and row["Median_AV_mag"] else "---"
        sfr_str = fmt_pm(row.get("SFR_Ha_Corr_Msun_yr", "0"), row.get("SFR_Ha_Corr_err_Msun_yr", "0"), dec=3)
        fwhm_n_str = fmt_pm(row.get("FWHM_narrow_kms", "0"), row.get("FWHM_narrow_err_kms", "0"), dec=1)
        fwhm_b_str = fmt_pm(row.get("FWHM_broad_kms", "0"), row.get("FWHM_broad_err_kms", "0"), dec=1)
        ratio_b_str = fmt_pm(row.get("NII_Ha_Ratio_Broad", "0"), row.get("NII_Ha_Ratio_Broad_err", "0"), dec=2)
        enh_str = fmt_pm(row.get("Shock_Enhancement_Factor", "0"), row.get("Shock_Enhancement_err", "0"), dec=2)

        latex_lines.append(
            f"{target_tex} & {regime_tex} & {n_spax} & {av_str} & {sfr_str} & "
            f"{fwhm_n_str} & {fwhm_b_str} & {ratio_b_str} & {enh_str} \\\\"
        )

    latex_lines.extend([
        r"\bottomrule",
        r"\end{tabular}%",
        r"}",
        r"\tablecomments{Kinematic parameters derived from simultaneous 6-Gaussian fits to the "
        r"$[\mathrm{N\,\textsc{ii}}]\,\lambda6548\text{--}\mathrm{H}\alpha\text{--}[\mathrm{N\,\textsc{ii}}]\,\lambda6583$ triplet. "
        r"Dust-corrected $\mathrm{SFR}_{\mathrm{corr}}$ adopts the Cardelli, Clayton \& Mathis (1989) extinction law ($R_V = 3.1$) "
        r"and the Chabrier IMF calibration of Murphy et al. (2011).}",
        r"\end{table*}",
    ])

    os.makedirs(os.path.dirname(tex_path), exist_ok=True)
    with open(tex_path, mode="w", encoding="utf-8") as tf:
        tf.write("\n".join(latex_lines) + "\n")

    print(f"Exported publication-ready LaTeX table: {tex_path}")

def process_single_target(
    target_name: str,
    dp_id: str,
    ra_override=None,
    dec_override=None,
    z_override=None,
    radius_arcsec: float = 10.0,
    band_half_nm: float = 5.0,
    snr_thresh: float = 5.0,
    dist_mpc: float = None,
    pa_deg: float = 45.0,
    inc_deg: float = 24.0,
    radial_bin: float = 2.0,
    max_av: float = None,
    outdir: str = "pipeline_outputs",
    data_dir: str = None,
    fig_dir: str = None,
    tab_dir: str = None,
    force_download: bool = False,
    token_cache: dict = None,
    **kwargs
) -> str:
    """
    Executes the full pipeline (SIMBAD lookup -> H-alpha & H-beta SODA cutouts ->
    kinematics, CCM89 dust correction, 6-Gaussian decomposition, CSV & MEF FITS)
    for a single target. Returns the generated per-target CSV path.
    """
    if token_cache is None:
        token_cache = {}

    # Initialize target prefix and output subdirectories inside process_single_target
    safe_prefix = re.sub(r'[^A-Za-z0-9_-]+', '_', target_name.strip().lower())
    data_dir = data_dir or os.path.join(outdir, "data")
    fig_dir = fig_dir or os.path.join(outdir, "figures")
    tab_dir = tab_dir or os.path.join(outdir, "tables")
    os.makedirs(data_dir, exist_ok=True)
    os.makedirs(fig_dir, exist_ok=True)
    os.makedirs(tab_dir, exist_ok=True)

    ha_cube_path = os.path.join(data_dir, f"{safe_prefix}_ha_soda_cutout.fits")
    hb_cube_path = os.path.join(data_dir, f"{safe_prefix}_hb_soda_cutout.fits")
    mef_path = os.path.join(data_dir, f"{safe_prefix}_kinematics_mef.fits")
    csv_path = os.path.join(tab_dir, f"{safe_prefix}_kinematic_summary.csv")
    ha_footprint_fig = os.path.join(fig_dir, f"{safe_prefix}_ha_footprint.png")
    hb_footprint_fig = os.path.join(fig_dir, f"{safe_prefix}_hb_footprint.png")

    ra, dec, redshift = resolve_target_simbad(target_name, ra_override, dec_override, z_override)
    half_band_m = band_half_nm * 1e-9

    obs_ha_m = 656.282e-9 * (1.0 + redshift)
    ha_wmin, ha_wmax = obs_ha_m - half_band_m, obs_ha_m + half_band_m

    obs_hb_m = 486.133e-9 * (1.0 + redshift)
    hb_wmin, hb_wmax = obs_hb_m - half_band_m, obs_hb_m + half_band_m

    need_ha = force_download or not os.path.exists(ha_cube_path)
    need_hb = force_download or not os.path.exists(hb_cube_path)

    if need_ha or need_hb:
        if "token" not in token_cache or not token_cache["token"]:
            token = os.environ.get("ESO_ID_TOKEN") or getpass.getpass("Paste verified ESO id_token: ")
            if not token.strip():
                raise ValueError("ESO id_token is required to download proprietary cutouts.")
            token_cache["token"] = token.strip()

        token = token_cache["token"]
        if need_ha:
            download_soda_cutout(
                dp_id=dp_id, ra=ra, dec=dec, radius_arcsec=radius_arcsec,
                wave_min=ha_wmin, wave_max=ha_wmax, output_fits=ha_cube_path,
                id_token=token, footprint_fig=ha_footprint_fig
            )
        if need_hb:
            download_soda_cutout(
                dp_id=dp_id, ra=ra, dec=dec, radius_arcsec=radius_arcsec,
                wave_min=hb_wmin, wave_max=hb_wmax, output_fits=hb_cube_path,
                id_token=token, footprint_fig=hb_footprint_fig
            )
    else:
        print(f"Using cached H-alpha and H-beta FITS cutouts for {target_name} in: {data_dir}")

    run_kinematic_and_mef_analysis(
        cube_path=ha_cube_path,
        hb_cube_path=hb_cube_path,
        mef_path=mef_path,
        csv_path=csv_path,
        fig_dir=fig_dir,
        prefix=safe_prefix,
        target_name=target_name,
        redshift=redshift,
        snr_thresh=snr_thresh,
        dist_mpc=dist_mpc,
        max_av=max_av
    )

    radial_csv = os.path.join(tab_dir, f"{safe_prefix}_radial_profiles.csv")
    radial_fig = os.path.join(fig_dir, f"{safe_prefix}_radial_profiles.png")

    extract_radial_profiles(
        mef_filepath=mef_path,
        center_ra=ra,
        center_dec=dec,
        dist_mpc=dist_mpc if dist_mpc is not None else 10.0,
        pa_deg=pa_deg,
        inc_deg=inc_deg,
        bin_width_arcsec=radial_bin,
        output_csv_path=radial_csv,
        output_fig_path=radial_fig
    )

    return csv_path

def extract_radial_profiles(
    mef_filepath: str,
    center_ra: float,
    center_dec: float,
    dist_mpc: float,
    pa_deg: float = 45.0,
    inc_deg: float = 24.0,
    bin_width_arcsec: float = 2.0,
    max_radius_arcsec: float = None,
    output_csv_path: str = None,
    output_fig_path: str = None,
    dpi: int = 300
):
    """
    Extracts azimuthally averaged radial profiles from the MEF FITS maps in concentric
    deprojected galactocentric rings (in arcsec and parsecs).
    Computes median + 16th-84th percentile scatter for Sigma_SFR, A_V, log10([NII]/Ha),
    and velocity dispersion (sigma), exporting both a CSV table and a 4-panel figure.
    """
    print("\n--- Extracting Deprojected Galactocentric Radial Profiles ---")

    if not os.path.exists(mef_filepath):
        raise FileNotFoundError(f"MEF FITS file not found: {mef_filepath}")

    with fits.open(mef_filepath) as hdul:
        header = hdul["HALPHA_SB"].header
        target_name = header.get("OBJECT", "Target")
        wcs = WCS(header).celestial

        ha_flux_corr = hdul["HALPHA_FLUX_CORR"].data.astype(np.float64) if "HALPHA_FLUX_CORR" in hdul else hdul["HALPHA_FLUX"].data.astype(np.float64)
        av_map = hdul["AV_CCM89"].data.astype(np.float64) if "AV_CCM89" in hdul else np.full_like(ha_flux_corr, np.nan)
        log_nii_ha = hdul["LOG_NII_HA"].data.astype(np.float64)
        mom2_map = hdul["HALPHA_MOM2"].data.astype(np.float64)

        raw_bunit = header.get("BUNIT", "10**(-20)")
        flux_scale = 1e-20 if ("10**(-20)" in raw_bunit or "1e-20" in raw_bunit.lower()) else 1.0

    # 1. Build pixel coordinate grid and project to sky coordinates
    ny, nx = ha_flux_corr.shape
    yy, xx = np.mgrid[0:ny, 0:nx]
    ra_grid, dec_grid = wcs.pixel_to_world_values(xx, yy)

    # Tangent-plane offsets in arcsec (East positive, North positive)
    cos_dec0 = np.cos(np.deg2rad(center_dec))
    dx_east = (ra_grid - center_ra) * cos_dec0 * 3600.0
    dy_north = (dec_grid - center_dec) * 3600.0

    # Rotate to galaxy major axis (PA measured East of North) & deproject by inclination
    pa_rad = np.deg2rad(pa_deg)
    cos_inc = max(np.cos(np.deg2rad(inc_deg)), 0.1)

    x_major = dx_east * np.sin(pa_rad) + dy_north * np.cos(pa_rad)
    y_minor = -dx_east * np.cos(pa_rad) + dy_north * np.sin(pa_rad)
    r_gal_arcsec = np.sqrt(x_major**2 + (y_minor / cos_inc)**2)

    # Physical scale: pc per arcsec and kpc^2 per spaxel
    pc_per_arcsec = (dist_mpc * 1e6) / 206264.806
    r_gal_pc = r_gal_arcsec * pc_per_arcsec

    from astropy.wcs.utils import proj_plane_pixel_area
    pix_area_arcsec2 = (float(proj_plane_pixel_area(wcs)) * u.deg**2).to(u.arcsec**2).value
    pix_area_kpc2 = pix_area_arcsec2 * ((pc_per_arcsec / 1000.0) ** 2) * cos_inc

    # 2. Compute spaxel SFR surface density [Msun / yr / kpc^2]
    d_l_cm = (dist_mpc * u.Mpc).to(u.cm).value
    geom_area_cm2 = 4.0 * np.pi * (d_l_cm ** 2)
    sfr_coeff = 5.37e-42  # Murphy et al. (2011) Chabrier IMF

    lum_ha_spax = ha_flux_corr * flux_scale * geom_area_cm2
    sfr_spax = sfr_coeff * lum_ha_spax
    sigma_sfr_map = sfr_spax / pix_area_kpc2

    # 3. Define concentric radial bins
    if max_radius_arcsec is None:
        valid_r = r_gal_arcsec[np.isfinite(ha_flux_corr)]
        max_radius_arcsec = float(np.nanpercentile(valid_r, 98.0)) if len(valid_r) > 0 else 45.0

    bin_edges = np.arange(0.0, max_radius_arcsec + bin_width_arcsec, bin_width_arcsec)
    rows = []

    def bin_stats(arr, mask):
        vals = arr[mask & np.isfinite(arr)]
        if len(vals) < 3:
            return np.nan, np.nan, np.nan
        p16, p50, p84 = np.nanpercentile(vals, [16.0, 50.0, 84.0])
        return float(p50), float(p50 - p16), float(p84 - p50)

    for i in range(len(bin_edges) - 1):
        r_in, r_out = bin_edges[i], bin_edges[i + 1]
        r_mid = 0.5 * (r_in + r_out)
        r_mid_pc = r_mid * pc_per_arcsec

        ring_mask = (r_gal_arcsec >= r_in) & (r_gal_arcsec < r_out)
        n_valid = int(np.sum(ring_mask & np.isfinite(ha_flux_corr)))
        if n_valid < 5:
            continue

        sfr_med, sfr_lo, sfr_hi = bin_stats(sigma_sfr_map, ring_mask)
        av_med, av_lo, av_hi = bin_stats(av_map, ring_mask)
        nii_med, nii_lo, nii_hi = bin_stats(log_nii_ha, ring_mask)
        sig_med, sig_lo, sig_hi = bin_stats(mom2_map, ring_mask)

        ring_sfr_total = float(np.nansum(sfr_spax[ring_mask]))

        rows.append({
            "Target": target_name,
            "R_in_arcsec": round(r_in, 2),
            "R_out_arcsec": round(r_out, 2),
            "R_mid_arcsec": round(r_mid, 2),
            "R_mid_pc": round(r_mid_pc, 1),
            "N_Spaxels": n_valid,
            "Ring_SFR_Msun_yr": round(ring_sfr_total, 5),
            "Sigma_SFR_med": round(sfr_med, 4) if np.isfinite(sfr_med) else np.nan,
            "Sigma_SFR_err_lo": round(sfr_lo, 4) if np.isfinite(sfr_lo) else np.nan,
            "Sigma_SFR_err_hi": round(sfr_hi, 4) if np.isfinite(sfr_hi) else np.nan,
            "AV_mag_med": round(av_med, 3) if np.isfinite(av_med) else np.nan,
            "AV_mag_err_lo": round(av_lo, 3) if np.isfinite(av_lo) else np.nan,
            "AV_mag_err_hi": round(av_hi, 3) if np.isfinite(av_hi) else np.nan,
            "Log10_NII_Ha_med": round(nii_med, 4) if np.isfinite(nii_med) else np.nan,
            "Log10_NII_Ha_err_lo": round(nii_lo, 4) if np.isfinite(nii_lo) else np.nan,
            "Log10_NII_Ha_err_hi": round(nii_hi, 4) if np.isfinite(nii_hi) else np.nan,
            "Sigma_vel_kms_med": round(sig_med, 2) if np.isfinite(sig_med) else np.nan,
            "Sigma_vel_kms_err_lo": round(sig_lo, 2) if np.isfinite(sig_lo) else np.nan,
            "Sigma_vel_kms_err_hi": round(sig_hi, 2) if np.isfinite(sig_hi) else np.nan,
        })

    if not rows:
        print("Warning: Not enough valid spaxels to construct radial bins.")
        return None

    # 4. Save Radial Profile CSV
    if output_csv_path:
        os.makedirs(os.path.dirname(output_csv_path), exist_ok=True)
        with open(output_csv_path, mode="w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
            writer.writeheader()
            writer.writerows(rows)
        print(f"Exported radial profile CSV ({len(rows)} annuli): {output_csv_path}")

    # 5. Render 4-Panel Radial Profile Figure
    r_pc = np.array([r["R_mid_pc"] for r in rows])
    r_arc = np.array([r["R_mid_arcsec"] for r in rows])

    fig, axes = plt.subplots(2, 2, figsize=(14.5, 9.5), sharex=True)

    def plot_band(ax, y_key, lo_key, hi_key, color, ylabel, title, log_y=False):
        y = np.array([r[y_key] for r in rows], dtype=float)
        lo = np.array([r[lo_key] for r in rows], dtype=float)
        hi = np.array([r[hi_key] for r in rows], dtype=float)
        valid = np.isfinite(y)
        ax.plot(r_pc[valid], y[valid], color=color, lw=2.2, marker="o", ms=5)
        ax.fill_between(r_pc[valid], y[valid] - lo[valid], y[valid] + hi[valid], color=color, alpha=0.20, label="16th–84th Percentile")
        if log_y:
            ax.set_yscale("log")
        ax.set_ylabel(ylabel, fontsize=10.5)
        ax.set_title(title, fontsize=11.5, fontweight="bold")
        ax.grid(True, ls=":", alpha=0.55)

    # Panel 1: Star Formation Rate Surface Density
    plot_band(
        axes[0, 0], "Sigma_SFR_med", "Sigma_SFR_err_lo", "Sigma_SFR_err_hi",
        "#c0392b", r"$\Sigma_{\mathrm{SFR}}$ [$M_\odot\,\mathrm{yr}^{-1}\,\mathrm{kpc}^{-2}$]",
        "1. Dust-Corrected SFR Surface Density", log_y=True
    )

    # Panel 2: Visual Extinction A_V
    plot_band(
        axes[0, 1], "AV_mag_med", "AV_mag_err_lo", "AV_mag_err_hi",
        "#d95f02", r"Visual Extinction $A_V$ [mag]",
        "2. CCM89 Balmer Decrement Dust Attenuation"
    )

    # Panel 3: Ionization Ratio log10([N II]/Ha)
    plot_band(
        axes[1, 0], "Log10_NII_Ha_med", "Log10_NII_Ha_err_lo", "Log10_NII_Ha_err_hi",
        "#2b5c8f", r"$\log_{10}([\mathrm{N\,II}]\,\lambda6583 / \mathrm{H}\alpha)$",
        "3. Ionization Excitation Gradient"
    )
    axes[1, 0].axhline(-0.30, color="royalblue", ls="--", lw=1.1, label="H II Boundary (-0.30)")
    axes[1, 0].axhline(-0.20, color="crimson", ls=":", lw=1.1, label="Shock Threshold (-0.20)")
    axes[1, 0].legend(loc="best", fontsize=8.5)

    # Panel 4: Velocity Dispersion
    plot_band(
        axes[1, 1], "Sigma_vel_kms_med", "Sigma_vel_kms_err_lo", "Sigma_vel_kms_err_hi",
        "#1b7837", r"Velocity Dispersion $\sigma$ [km s$^{-1}$]",
        r"4. H$\alpha$ Gas Turbulence Profile"
    )

    for ax in axes[1, :]:
        ax.set_xlabel(f"Deprojected Galactocentric Radius [pc] (D = {dist_mpc:.2f} Mpc, PA = {pa_deg:.0f}°, i = {inc_deg:.0f}°)", fontsize=10.5)

    # Add secondary top x-axis in arcseconds
    for ax in axes[0, :]:
        ax_top = ax.twiny()
        ax_top.set_xlim(ax.get_xlim()[0] / pc_per_arcsec, ax.get_xlim()[1] / pc_per_arcsec)
        ax_top.set_xlabel("Galactocentric Radius [arcsec]", fontsize=9.5, color="dimgray")

    fig.suptitle(f"Azimuthally Averaged Radial Profiles: {target_name}", fontsize=14, fontweight="bold")
    plt.tight_layout(rect=[0, 0, 1, 0.95])

    if output_fig_path:
        os.makedirs(os.path.dirname(output_fig_path), exist_ok=True)
        fig.savefig(output_fig_path, dpi=dpi, bbox_inches="tight", facecolor="white")
        print(f"Saved 4-panel radial profile figure: {output_fig_path} ({dpi} DPI)")

    plt.close(fig)

    # 6. Append RADIAL_PROFILES binary table extension into the MEF FITS file
    if output_csv_path and os.path.exists(output_csv_path) and os.path.exists(mef_filepath):
        rad_tbl = Table.read(output_csv_path, format="csv")
        rad_units = {
            "R_in_arcsec": "arcsec",
            "R_out_arcsec": "arcsec",
            "R_mid_arcsec": "arcsec",
            "R_mid_pc": "pc",
            "Ring_SFR_Msun_yr": "solMass / yr",
            "Sigma_SFR_med": "solMass / (yr kpc**2)",
            "Sigma_SFR_err_lo": "solMass / (yr kpc**2)",
            "Sigma_SFR_err_hi": "solMass / (yr kpc**2)",
            "AV_mag_med": "mag",
            "AV_mag_err_lo": "mag",
            "AV_mag_err_hi": "mag",
            "Log10_NII_Ha_med": "",
            "Log10_NII_Ha_err_lo": "",
            "Log10_NII_Ha_err_hi": "",
            "Sigma_vel_kms_med": "km / s",
            "Sigma_vel_kms_err_lo": "km / s",
            "Sigma_vel_kms_err_hi": "km / s",
        }
        for col, u_str in rad_units.items():
            if col in rad_tbl.colnames:
                rad_tbl[col].unit = u_str

        with fits.open(mef_filepath, mode="update") as hdul_mef:
            while "RADIAL_PROFILES" in hdul_mef:
                del hdul_mef["RADIAL_PROFILES"]
            hdul_mef.append(fits.BinTableHDU(data=rad_tbl, name="RADIAL_PROFILES"))
            hdul_mef.flush()
        print(f"Appended 'RADIAL_PROFILES' extension to MEF FITS: {mef_filepath}")

    return output_csv_path

def export_radial_profiles_to_latex(
    radial_csv_filepath: str,
    tex_filepath: str,
    caption: str = "Deprojected Galactocentric Radial Profiles",
    label: str = "tab:radial_profiles"
):
    """
    Converts the pipeline's radial profile CSV into a publication-ready
    AASTeX (ApJ/AJ) deluxetable* LaTeX file.
    """
    if not os.path.exists(radial_csv_filepath):
        raise FileNotFoundError(f"Radial profile CSV not found: {radial_csv_filepath}")

    with open(radial_csv_filepath, mode="r", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))

    if not rows:
        raise ValueError(f"No radial bins found in {radial_csv_filepath}")

    def fmt_asym(med_str, lo_str, hi_str, dec=2):
        if not med_str or med_str.lower() == "nan":
            return r"\nodata"
        med, lo, hi = float(med_str), float(lo_str), float(hi_str)
        if not (np.isfinite(med) and np.isfinite(lo) and np.isfinite(hi)):
            return r"\nodata"
        return rf"${med:.{dec}f}_{{-{lo:.{dec}f}}}^{{+{hi:.{dec}f}}}$"

    data_lines = []
    for r in rows:
        r_arc = f"{float(r['R_mid_arcsec']):.1f}"
        r_pc  = f"{float(r['R_mid_pc']):.0f}"
        n_sp  = r["N_Spaxels"]
        sfr_ring = f"{float(r['Ring_SFR_Msun_yr']):.4f}"
        sig_sfr = fmt_asym(r["Sigma_SFR_med"], r["Sigma_SFR_err_lo"], r["Sigma_SFR_err_hi"], dec=2)
        av_val  = fmt_asym(r["AV_mag_med"], r["AV_mag_err_lo"], r["AV_mag_err_hi"], dec=2)
        nii_ha  = fmt_asym(r["Log10_NII_Ha_med"], r["Log10_NII_Ha_err_lo"], r["Log10_NII_Ha_err_hi"], dec=2)
        vel_sig = fmt_asym(r["Sigma_vel_kms_med"], r["Sigma_vel_kms_err_lo"], r["Sigma_vel_kms_err_hi"], dec=1)

        data_lines.append(
            f"{r_arc} & {r_pc} & {n_sp} & {sfr_ring} & {sig_sfr} & {av_val} & {nii_ha} & {vel_sig} \\\\"
        )

    lines = [
        r"\begin{deluxetable*}{cccccccc}",
        r"\tabletypesize{\footnotesize}",
        r"\tablewidth{0pt}",
        rf"\tablecaption{{{caption} \label{{{label}}}}}",
        r"\tablehead{",
        r"\colhead{$R_{\mathrm{gal}}$} & \colhead{$R_{\mathrm{gal}}$} & \colhead{$N_{\mathrm{spax}}$} & "
        r"\colhead{$\mathrm{SFR}_{\mathrm{ring}}$} & \colhead{$\Sigma_{\mathrm{SFR}}$} & "
        r"\colhead{$A_V$} & \colhead{$\log_{10}([\mathrm{N\,II}]/\mathrm{H}\alpha)$} & \colhead{$\sigma_{\mathrm{gas}}$} \\",
        r"\colhead{(arcsec)} & \colhead{(pc)} & \colhead{} & "
        r"\colhead{($M_\odot\,\mathrm{yr}^{-1}$)} & \colhead{($M_\odot\,\mathrm{yr}^{-1}\,\mathrm{kpc}^{-2}$)} & "
        r"\colhead{(mag)} & \colhead{(dex)} & \colhead{($\mathrm{km\,s^{-1}}$)}",
        r"}",
        r"\startdata",
        *data_lines,
        r"\enddata",
        r"\tablecomments{Azimuthally averaged medians and 16th--84th percentile bounds in concentric deprojected galactocentric annuli.}",
        r"\end{deluxetable*}",
    ]

    out_dir = os.path.dirname(tex_filepath)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    with open(tex_filepath, mode="w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

    print(f"Exported AASTeX radial profile table to: {tex_filepath}")

def main():
    parser = argparse.ArgumentParser(description="ESO TAP + DataLink + SODA + Kinematic Pipeline CLI")
    parser.add_argument("--target", default=None, help="Galaxy or target name for single-target mode (e.g., 'NGC253')")
    parser.add_argument("--dp-id", default=None, help="ESO Dataset ID for single-target mode (e.g., 'ADP.2025-07-06T07:52:11.871')")
    parser.add_argument("--list-only", action="store_true", help="Dry-run: list matching ESO cubes via TAP and exit without downloading")
    parser.add_argument("--list-out", default=None, help="Optional .txt file path for --list-only output")
    parser.add_argument("--batch-csv", default=None, help="Path to CSV manifest for multi-target batch processing")
    parser.add_argument("--ra", type=float, default=None, help="Optional RA override [deg]")
    parser.add_argument("--dec", type=float, default=None, help="Optional Dec override [deg]")
    parser.add_argument("--z", type=float, default=None, help="Optional redshift override (queried via SIMBAD if omitted)")
    parser.add_argument("--dist-mpc", type=float, default=None, help="Optional luminosity distance in Mpc (computed from z if omitted)")
    parser.add_argument("--radius", type=float, default=10.0, help="Default cutout radius in arcsec (default: 10.0)")
    parser.add_argument("--band-half-nm", type=float, default=5.0, help="Half-width around redshifted H-alpha and H-beta in nm (default: 5.0)")
    parser.add_argument("--snr", type=float, default=5.0, help="Minimum SNR threshold for moment and Balmer masking (default: 5.0)")
    parser.add_argument("--outdir", default="pipeline_outputs", help="Base output directory (default: pipeline_outputs)")
    parser.add_argument("--force-download", action="store_true", help="Re-download SODA cutouts even if cached locally")
    parser.add_argument("--max-av", type=float, default=None, help="Optional upper cap on A_V [mag] to prevent noise amplification (e.g., 6.0)")
    parser.add_argument("--pa", type=float, default=45.0, help="Galaxy major-axis Position Angle [deg East of North] (default: 45.0)")
    parser.add_argument("--inc", type=float, default=24.0, help="Galaxy disk inclination angle [deg] (default: 24.0)")
    parser.add_argument("--radial-bin", type=float, default=2.0, help="Radial annulus width in arcsec (default: 2.0)")
    parser.add_argument("--export-latex", action="store_true", help="Export summary and radial profile CSVs as LaTeX (.tex) tables")
    args = parser.parse_args()

    # 1. Dry-run exit: list available cubes via TAP and return without prompting for token
    if args.list_only:
        if not args.target:
            parser.error("--target is required when using --list-only")
        ra, dec, _ = resolve_target_simbad(args.target, args.ra, args.dec, args.z)
        safe_prefix = re.sub(r'[^A-Za-z0-9_-]+', '_', args.target.strip().lower())
        default_txt = os.path.join(args.outdir, "tables", f"{safe_prefix}_eso_cubes.txt")
        txt_path = args.list_out or default_txt
        list_matching_eso_cubes(
            ra=ra,
            dec=dec,
            radius_arcsec=args.radius,
            dp_id=args.dp_id,
            output_txt=txt_path
        )
        return

    # 2. Validate required arguments for single-target vs. batch mode
    if not args.batch_csv and (not args.target or not args.dp_id):
        parser.error("Provide either --batch-csv <file.csv> OR both --target and --dp-id.")

    data_dir = os.path.join(args.outdir, "data")
    fig_dir = os.path.join(args.outdir, "figures")
    tab_dir = os.path.join(args.outdir, "tables")
    os.makedirs(data_dir, exist_ok=True)
    os.makedirs(fig_dir, exist_ok=True)
    os.makedirs(tab_dir, exist_ok=True)

    token_cache = {}

    def export_target_latex_tables(summary_csv_path: str, target_label: str):
        """Helper to export both kinematic summary and radial profile LaTeX tables."""
        out_tex = summary_csv_path.replace(".csv", ".tex")
        export_summary_to_latex(summary_csv_path, out_tex)

        rad_csv = summary_csv_path.replace("_kinematic_summary.csv", "_radial_profiles.csv")
        if os.path.exists(rad_csv):
            rad_tex = rad_csv.replace(".csv", ".tex")
            safe_lbl = re.sub(r'[^a-z0-9]+', '_', target_label.lower())
            export_radial_profiles_to_latex(
                rad_csv,
                rad_tex,
                caption=f"Deprojected Galactocentric Radial Profiles: {target_label}",
                label=f"tab:{safe_lbl}_radial"
            )

    # 3. Batch Processing Mode
    if args.batch_csv:
        if not os.path.exists(args.batch_csv):
            raise FileNotFoundError(f"Batch CSV manifest not found: {args.batch_csv}")

        def parse_opt_float(row_dict, key, fallback=None):
            val = row_dict.get(key, "").strip()
            return float(val) if val else fallback

        generated_csvs = []
        with open(args.batch_csv, mode="r", encoding="utf-8-sig") as f:
            reader = csv.DictReader(f)
            jobs = list(reader)

        print(f"=== Starting Batch Processing ({len(jobs)} targets from {args.batch_csv}) ===")
        for idx, row in enumerate(jobs, start=1):
            t_name = row.get("target", "").strip()
            d_id = row.get("dp_id", "").strip()
            if not t_name or not d_id:
                print(f"[{idx}/{len(jobs)}] Skipping incomplete row: {row}")
                continue

            print(f"\n[{idx}/{len(jobs)}] Processing {t_name} ({d_id})...")
            try:
                out_csv = process_single_target(
                    target_name=t_name,
                    dp_id=d_id,
                    data_dir=data_dir,
                    fig_dir=fig_dir,
                    tab_dir=tab_dir,
                    ra_override=parse_opt_float(row, "ra"),
                    dec_override=parse_opt_float(row, "dec"),
                    z_override=parse_opt_float(row, "z"),
                    dist_mpc=parse_opt_float(row, "dist_mpc"),
                    pa_deg=parse_opt_float(row, "pa", fallback=args.pa),
                    inc_deg=parse_opt_float(row, "inc", fallback=args.inc),
                    radial_bin=parse_opt_float(row, "radial_bin", fallback=args.radial_bin),
                    radius_arcsec=parse_opt_float(row, "radius", fallback=args.radius),
                    band_half_nm=args.band_half_nm,
                    snr_thresh=args.snr,
                    max_av=args.max_av,
                    force_download=args.force_download,
                    token_cache=token_cache
                )

                if args.export_latex:
                    export_target_latex_tables(out_csv, t_name)

                generated_csvs.append(out_csv)

            except Exception as e:
                print(f"Error processing {t_name} ({d_id}): {e}")

        # Merge all individual target CSVs into one master batch table
        if generated_csvs:
            master_csv = os.path.join(tab_dir, "master_batch_kinematic_summary.csv")
            all_rows = []
            master_fields = None
            for c_path in generated_csvs:
                if os.path.exists(c_path):
                    with open(c_path, mode="r", encoding="utf-8") as cf:
                        r = csv.DictReader(cf)
                        if master_fields is None:
                            master_fields = r.fieldnames
                        all_rows.extend(list(r))

            if all_rows and master_fields:
                with open(master_csv, mode="w", newline="", encoding="utf-8") as mf:
                    writer = csv.DictWriter(mf, fieldnames=master_fields)
                    writer.writeheader()
                    writer.writerows(all_rows)
                print(f"\n=== Master Batch Summary Saved: {master_csv} ({len(all_rows)} total rows) ===")

                if args.export_latex:
                    master_tex = os.path.join(tab_dir, "master_batch_kinematic_summary.tex")
                    export_summary_to_latex(master_csv, master_tex)

    # 4. Single-Target Mode
    else:
        out_csv = process_single_target(
            target_name=args.target,
            dp_id=args.dp_id,
            data_dir=data_dir,
            fig_dir=fig_dir,
            tab_dir=tab_dir,
            ra_override=args.ra,
            dec_override=args.dec,
            z_override=args.z,
            dist_mpc=args.dist_mpc,
            pa_deg=args.pa,
            inc_deg=args.inc,
            radial_bin=args.radial_bin,
            radius_arcsec=args.radius,
            band_half_nm=args.band_half_nm,
            snr_thresh=args.snr,
            max_av=args.max_av,
            force_download=args.force_download,
            token_cache=token_cache
        )
        if args.export_latex:
            export_target_latex_tables(out_csv, args.target)

    print("\nPipeline execution finished successfully!")

if __name__ == "__main__":
    main()
