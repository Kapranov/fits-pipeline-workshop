#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Helper helper function to compile and handle compiler errors immediately */
int compile_stage(const char *source_file, const char *output_binary, const char *extra_flags) {
    char compile_cmd[512];
    sprintf(compile_cmd, "gcc %s -o %s -lcfitsio %s", source_file, output_binary, extra_flags);

    printf("[BUILD] Compiling: %s ...\n", source_file);
    int ret = system(compile_cmd);
    if (ret != 0) {
        printf("[FATAL ERROR] Compilation failed for %s with exit code %d\n", source_file, ret);
        return 0;
    }
    return 1;
}

/* Helper function to run binaries and verify their execution handles */
int run_stage(const char *output_binary, const char *stage_name) {
    char run_cmd[256];
    sprintf(run_cmd, "./%s", output_binary);

    printf("\n========================================================================\n");
    printf("[RUNNING] %s\n", stage_name);
    printf("========================================================================\n");

    int ret = system(run_cmd);
    if (ret != 0) {
        printf("[FATAL ERROR] Runtime execution crash during %s! Code: %d\n", stage_name, ret);
        return 0;
    }
    return 1;
}

int main() {
    printf("========================================================================\n");
    printf("         AUTOMATED OBSERVATORY DATA PROCESSING PIPELINE WRAPPER         \n");
    printf("========================================================================\n");
    printf(" Starting pipeline execution on Arch Linux...\n\n");

    /* 1. STAGE 17 - SIMULATION ENGINE GENERATOR */
    if (!compile_stage("stage_17_generate_cube.c", "stage_17_generate_cube", "-lm")) return 1;
    if (!run_stage("stage_17_generate_cube", "STAGE 17: 3D WCS-Time Cube Simulation Layer")) return 1;

    /* 2. STAGE 23 - CCD APERTURE SNR PHOTOMETRY SCANNER */
    if (!compile_stage("stage_23_batch_snr.c", "stage_23_batch_snr", "-lm")) return 1;
    if (!run_stage("stage_23_batch_snr", "STAGE 23: Aperture Photometry Engine with Real-Time SNR Filters")) return 1;

    /* 3. STAGE 25 - MASTER HDU REPOSITORY PACKAGER */
    if (!compile_stage("stage_25_master_archive.c", "stage_25_master_archive", "")) return 1;
    if (!run_stage("stage_25_master_archive", "STAGE 25: Multi-Extension Master Archive Compilation")) return 1;

    /* 4. STAGE 33 - 2D BILINEAR MESH BACKGROUND ESTIMATOR */
    if (!compile_stage("stage_33_mesh_bg.c", "stage_33_mesh_bg", "-lm")) return 1;
    if (!run_stage("stage_33_mesh_bg", "STAGE 33: 2D Spatial Mesh Sky Background Modeler")) return 1;

    /* 5. STAGE 34 - MATHEMATICAL DATA REDUCTION PIPELINE */
    if (!compile_stage("stage_34_reduction.c", "stage_34_reduction", "-lm")) return 1;
    if (!run_stage("stage_34_reduction", "STAGE 34: Pixel-by-Pixel Data Reduction & Background Subtraction")) return 1;

    /* 6. CLEAN UP INTERIM PIPELINE BINARIES FOR COMPACT WORKSPACE ARCHITECTURE */
    printf("\n========================================================================\n");
    printf("[CLEANUP] Purging volatile interim compilation binaries...\n");
    printf("========================================================================\n");
    system("rm -f stage_17_generate_cube stage_23_batch_snr stage_25_master_archive stage_33_mesh_bg stage_34_reduction");

    printf("\n========================================================================\n");
    printf(" SUCCESS: ALL PIPELINE PROCESSES LOGGED AND COMPLETED PERFECTLY!        \n");
    printf(" Outputs Generated:\n");
    printf("   -> stage_25_master_observatory_dataset.fits (WCS-Time Image Cube + SNR Table)\n");
    printf("   -> stage_33_master_background_maps.fits     (Interpolated 2D Mesh Maps)\n");
    printf("   -> stage_34_master_reduced_science.fits     (Flattened Subtracted Arrays)\n");
    printf("========================================================================\n");

    return 0;
}
