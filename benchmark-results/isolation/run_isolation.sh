#!/usr/bin/env bash
# ForgeDB change-isolation experiment (Linux / macOS / WSL; needs g++ with C++20, python3, patch).
#
# Builds four variants from your repo and measures per-call point-lookup and range-scan latency:
#   baseline     = pre-patch DiskManager/BufferPoolManager (from benchmark-results/latency-ab/baseline-source)
#   cursor_only  = baseline + free-frame cursor only        (cursor-only.patch)
#   size_only    = baseline + cached file length only       (size-only.patch)
#   shipped      = the repo as it is now (all three changes)
#
# Usage:  ./run_isolation.sh /path/to/forgedb-repo
# Env:    TRIALS=5  ROWS=1000000  POOL=4096  SAMPLES=10000  KEEP=1 (keep work dir)
#
# Note: "batched flush only" is intentionally NOT a variant. Removing the per-write flush while the
# file size is still read from the filesystem corrupts page allocation ("record bounds are invalid"),
# because buffered writes are invisible to the OS-reported size. Batching is only safe with cached length.
set -euo pipefail

REPO=$(cd "${1:?usage: run_isolation.sh /path/to/forgedb-repo}" && pwd)
HERE=$(cd "$(dirname "$0")" && pwd)
TRIALS=${TRIALS:-5}; ROWS=${ROWS:-1000000}; POOL=${POOL:-4096}; SAMPLES=${SAMPLES:-10000}
WORK=$(mktemp -d)
BASE_SRC="$REPO/benchmark-results/latency-ab/baseline-source"
CHANGED=(src/buffer/buffer_pool_manager.cpp include/forgedb/buffer/buffer_pool_manager.h
         src/storage/disk_manager.cpp include/forgedb/storage/disk_manager.h)
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)

[ -d "$BASE_SRC" ] || { echo "missing $BASE_SRC (baseline-source folder not found)"; exit 1; }
[ -f "$REPO/benchmarks/forgedb_benchmark.cpp" ] || { echo "missing benchmark source"; exit 1; }
grep -q latency_samples "$REPO/benchmarks/forgedb_benchmark.cpp" || { echo "benchmark is not the latency-instrumented version"; exit 1; }

make_tree() {  # $1 = variant name
  local v=$1 d="$WORK/$1"; mkdir -p "$d"
  cp -r "$REPO/src" "$REPO/include" "$d/"
  if [ "$v" != shipped ]; then
    for f in "${CHANGED[@]}"; do tr -d '\r' < "$BASE_SRC/$f" > "$d/$f"; done   # baseline versions, LF endings
    case $v in
      cursor_only) (cd "$d" && patch -p1 -s < "$HERE/cursor-only.patch") ;;
      size_only)   (cd "$d" && patch -p1 -s < "$HERE/size-only.patch") ;;
    esac
  fi
}

build() {
  local v=$1 d="$WORK/$1"; mkdir -p "$d/obj"
  ( cd "$d/obj"
    ls ../src/*/*.cpp | xargs -P "$JOBS" -I{} sh -c \
      'g++ -std=c++20 -O2 -DNDEBUG -I../include -c "$1" -o "$(echo "$1" | sed "s|../src/||; s|/|_|g; s|.cpp|.o|")"' _ {}
    ar rcs ../libf.a ./*.o )
  g++ -std=c++20 -O2 -DNDEBUG -I"$d/include" "$REPO/benchmarks/forgedb_benchmark.cpp" "$d/libf.a" -o "$d/bm" -pthread
  echo "built $v"
}

ORDER=(baseline cursor_only size_only shipped)
for v in "${ORDER[@]}"; do make_tree "$v"; build "$v"; done

for t in $(seq 1 "$TRIALS"); do
  for v in "${ORDER[@]}"; do          # interleaved to spread machine noise across variants
    r="$WORK/runs/${v}_$t"; mkdir -p "$r"; ( cd "$r"
      "$WORK/$v/bm" "$ROWS" "$POOL" "$SAMPLES" > out.txt 2>&1 || { echo "RUN FAILED: $v trial $t"; tail -3 out.txt; exit 1; }
      grep -q "completed successfully" out.txt || { echo "VALIDATION FAILED: $v trial $t"; exit 1; } )
    echo "ran $v trial $t"
  done
done

WORK="$WORK" TRIALS="$TRIALS" python3 - <<'PY'
import csv, math, os, statistics as st
W=os.environ['WORK']; T=int(os.environ['TRIALS'])
order=['baseline','cursor_only','size_only','shipped']
label={'baseline':'baseline','cursor_only':'+ free-frame cursor only','size_only':'+ cached file length only','shipped':'all three (shipped)'}
def nr(xs,p): xs=sorted(xs); return xs[math.ceil(p/100*len(xs))-1]
def load(v,t):
    d={}
    with open(f'{W}/runs/{v}_{t}/forgedb_latency_samples.csv') as f:
        for r in csv.DictReader(f): d.setdefault(r['operation'],[]).append(int(r['latency_ns'])/1000)
    return d
data={v:[load(v,t) for t in range(1,T+1)] for v in order}
rows=[]
for op in ('B+ tree point lookup','B+ tree range scan'):
    print(f'\n{op}  (median of {T} per-run percentiles, microseconds)')
    print(f"{'variant':28s}{'P50':>9s}{'P95':>9s}{'P99':>9s}{'P95 red.':>10s}{'P99 red.':>10s}   P95 run range")
    base={p:st.median([nr(x[op],p) for x in data['baseline']]) for p in (50,95,99)}
    for v in order:
        m={p:st.median([nr(x[op],p) for x in data[v]]) for p in (50,95,99)}
        r95=[nr(x[op],95) for x in data[v]]
        red={p:(base[p]-m[p])/base[p]*100 for p in (95,99)}
        print(f"{label[v]:28s}{m[50]:9.2f}{m[95]:9.2f}{m[99]:9.2f}{red[95]:9.1f}%{red[99]:9.1f}%   {min(r95):.2f}-{max(r95):.2f}")
        rows.append([op,v,round(m[50],3),round(m[95],3),round(m[99],3),round(red[95],1),round(red[99],1),round(min(r95),3),round(max(r95),3)])
out=os.path.join(os.getcwd(),'isolation-results.csv')
with open(out,'w',newline='') as f:
    w=csv.writer(f); w.writerow(['operation','variant','p50_us','p95_us','p99_us','p95_reduction_pct','p99_reduction_pct','p95_run_min_us','p95_run_max_us']); w.writerows(rows)
print('\nwrote',out)
PY
[ "${KEEP:-0}" = 1 ] && echo "work dir kept: $WORK" || rm -rf "$WORK"
