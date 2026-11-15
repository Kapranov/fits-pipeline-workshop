# ==================================================================
#           OBSERVATORY PIPELINE MASTER BUILD UTILITY
# ==================================================================

# Define compiler and universal linking libraries flag markers
CC = gcc
CFLAGS = -Wall -O2
LIBS = -lcfitsio -lm

# List of target executable binaries to build
TARGETS = stage_17_generate_cube \
          stage_23_batch_snr \
          stage_25_master_archive \
          stage_27_dump_all_headers \
          stage_28_read_specifics

# Default rule: Compiles all binaries sequentially
all: $(TARGETS)

# Rules to build individual processing modules
stage_17_generate_cube: stage_17_generate_cube.c
	$(CC) $(CFLAGS) stage_17_generate_cube.c -o stage_07_generate_cube $(LIBS)

stage_23_batch_snr: stage_23_batch_snr.c
	$(CC) $(CFLAGS) stage_23_batch_snr.c -o stage_23_batch_snr $(LIBS)

stage_25_master_archive: stage_25_master_archive.c
	$(CC) $(CFLAGS) stage_25_master_archive.c -o stage_25_master_archive $(LIBS)

stage_27_dump_all_headers: stage_27_dump_all_headers.c
	$(CC) $(CFLAGS) stage_27_dump_all_headers.c -o stage_27_dump_all_headers $(LIBS)

stage_28_read_specifics: stage_28_read_specifics.c
	$(CC) $(CFLAGS) stage_28_read_specifics.c -o stage_28_read_specifics $(LIBS)

# Operational execution rule: Runs the full generation and merge sequence in one shot
pipeline: all
	@echo "=================================================================="
	@echo "🚀    EXECUTING DATA PROCESSING PIPELINE SEQUENCE RUN             "
	@echo "=================================================================="
	./stage_17_generate_cube
	./stage_23_batch_snr
	./stage_25_master_archive
	@echo "=================================================================="
	@echo "✅    PIPELINE COMPLETE: master_observatory_dataset.fits Ready!    "
	@echo "=================================================================="

# Clean rule: Purges compiled binary footprints out of the folder
clean:
	rm -f $(TARGETS)
	@echo "Workspace binaries wiped clean. Source code safe. 🧹"
