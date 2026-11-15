#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Global variables capturing parameter overrides */
float snr_threshold = 5.0f;
int num_stars = 50;

/* Helper function to parse arguments securely */
void parse_arguments(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--snr") == 0 && i + 1 < argc) {
            snr_threshold = (float)atof(argv[++i]);
        } else if (strcmp(argv[i], "--stars") == 0 && i + 1 < argc) {
            num_stars = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: ./pipeline_runner [options]\n");
            printf("Options:\n");
            printf("  --snr <float>   Set minimum acceptable quality filter threshold (default: 5.0)\n");
            printf("  --stars <int>   Set number of random Gaussian stars to inject (default: 50)\n");
            printf("  -h, --help      Display this command configuration help manual layout\n");
            exit(0);
        } else {
            printf("[WARNING] Unknown command flag option parameter ignored: %s\n", argv[i]);
        }
    }
}

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

int run_stage(const char *output_binary, const char *stage_name, const char *args) {
    char run_cmd[512];
    if (args != NULL) {
        sprintf(run_cmd, "./%s %s", output_binary, args);
    } else {
        sprintf(run_cmd, "./%s", output_binary);
    }

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

int main(int argc, char *argv[]) {
    /* 1. Intercept argument configurations */
    parse_arguments(argc, argv);

    printf("========================================================================\n");
    printf("         AUTOMATED OBSERVATORY DATA PROCESSING PIPELINE WRAPPER         \n");
    printf("========================================================================\n");
    printf(" Active Run Threshold Profiles:\n");
    printf("  -> Target Injected Stars: %d \n", num_stars);
    printf("  -> Selective SNR Cutoff : %0.2f \n", snr_threshold);
    printf("========================================================================\n\n");

    char pass_args[128];

    /* 2. STAGE 17 - SIMULATION ENGINE GENERATOR */
    if (!compile_stage("stage_17_generate_cube.c", "stage_17_generate_cube", "-lm")) return 1;
    // Pass custom star counts straight to the modified image generator binary file
    sprintf(pass_args, "%d", num_stars);
    if (!run_stage("stage_17_generate_cube", "STAGE 17: 3D WCS-Time Cube Simulation Layer", pass_args)) return 1;

    /* 3. STAGE 23 - CCD APERTURE SNR PHOTOMETRY SCANNER */
    if (!compile_stage("stage_23_batch_snr.c", "stage_23_batch_snr", "-lm")) return 1;
    // Pass custom SNR constraints straight to the photometry processing engine block
    sprintf(pass_args, "%f", snr_threshold);
    if (!run_stage("stage_23_batch_snr", "STAGE 23: Aperture Photometry with Real-Time SNR Filters", pass_args)) return 1;

    /* 4. STAGE 25 - MASTER HDU REPOSITORY PACKAGER */
    if (!compile_stage("stage_25_master_archive.c", "stage_25_master_archive", "")) return 1;
    if (!run_stage("stage_25_master_archive", "STAGE 25: Multi-Extension Master Archive Compilation", NULL)) return 1;

    /* 5. STAGE 33 - 2D BILINEAR MESH BACKGROUND ESTIMATOR */
    if (!compile_stage("stage_33_mesh_bg.c", "stage_33_mesh_bg", "-lm")) return 1;
    if (!run_stage("stage_33_mesh_bg", "STAGE 33: 2D Spatial Mesh Sky Background Modeler", NULL)) return 1;

    /* 6. STAGE 34 - MATHEMATICAL DATA REDUCTION PIPELINE */
    if (!compile_stage("stage_34_reduction.c", "stage_34_reduction", "-lm")) return 1;
    if (!run_stage("stage_34_reduction", "STAGE 34: Pixel-by-Pixel Data Reduction & Background Subtraction", NULL)) return 1;

    /* 7. CLEAN UP INTERIM PIPELINE BINARIES FOR COMPACT WORKSPACE ARCHITECTURE */
    printf("\n========================================================================\n");
    printf("[CLEANUP] Purging volatile interim compilation binaries...\n");
    printf("========================================================================\n");
    system("rm -f stage_17_generate_cube stage_23_batch_snr stage_25_master_archive stage_33_mesh_bg stage_34_reduction");

    printf("\n========================================================================\n");
    printf(" SUCCESS: ALL CUSTOM PIPELINE PROCESSES EXECUTE PERFECTLY!               \n");
    printf("========================================================================\n");

    return 0;
}
