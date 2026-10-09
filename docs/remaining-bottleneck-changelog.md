# Remaining bottleneck changes

## 2026-10-09

- Cache the database file size in `DiskManager`; initialize it once at open and advance it when `writePage` extends the file.
- Remove per-call filesystem metadata queries from page validation and page-count lookup.
- Stop flushing the C++ file stream after every page write. Explicit page flushes flush the disk manager; flushing all dirty pages batches writes and flushes once after the batch. Destructor finalization still flushes.
- Add a free-frame cursor to `BufferPoolManager` to avoid rescanning the occupied prefix from frame zero on each page allocation. `deletePage` rewinds the cursor when it creates a hole.
- Record five-trial, one-million-row benchmark data and summarize medians in `benchmark-results/`.

The three code changes were benchmarked as one combined patch; the current metrics do not isolate individual patch contributions.
