# Scalable Heap Metadata Directory

## Problem

The heap metadata page previously stored every data-page ID in one 4 KiB page. Each ID consumes eight bytes, so a heap table exhausted its metadata page after roughly 510 data pages. Larger workloads failed with `HeapFile: metadata page full` even when disk and buffer-pool capacity remained available.

## Change

- Added a linked metadata-directory format (V2) with a per-page count and next-page ID.
- Data-page IDs are stored in directory pages with bounded chunks; overflow pages are allocated as needed and reused on subsequent updates.
- Existing V1 single-page metadata remains readable. The first subsequent metadata write migrates it to V2.
- Heap data pages remain distinct from metadata overflow pages; only IDs listed in the directory are returned by `HeapFile::scan`.
- The heap record/page format is unchanged.
- Directory parsing validates per-page entry counts and bounds chain traversal to protect against malformed/cyclic metadata.

## Regression coverage

`heap_metadata_directory_overflow` inserts 520 large records to force more than 509 heap data pages, scans and reads the first and last records, closes the database, reopens it, and repeats the checks. It also constructs a legacy V1 directory, forces migration by allocating another data page, then verifies that records survive another reopen.

## Scaling validation

Release benchmark runs completed successfully at 50,000, 100,000, 200,000, and 500,000 rows with 256, 512, 1,024, and 2,048 buffer-pool pages respectively. All four benchmark validation counters passed for every workload. See `benchmark-results/heap-metadata-scaling.csv` for the single-run timings. These are scaling checks, not five-run performance claims.
