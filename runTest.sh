#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "Usage: $(basename "$0") <path/to/file.csv>"
  exit 1
fi
test -f "$1" || (echo "\"$1\": No such file or directory" && exit 1)

# Prepare the data
data/splitCSV.sh "$1"

# Spawn the coordinator process
cmake-build-debug/coordinator "file://$(dirname "$(realpath "$1")")/filelist.csv" 12345 &
# cmake-build-debug/coordinator "./data/filelist.csv" 4242 &
# cmake-build-debug/coordinator "file:///home/accio/Study/assignment-4-scalable-and-distributed-data-processing/data/filelist.csv" 12345 &

# sleep 1 
# cmake-build-debug/worker localhost 12345 &
# cmake-build-debug/worker localhost 12345 &
# cmake-build-debug/worker localhost 12345 &
# cmake-build-debug/worker localhost 12345 &
# Spawn some workers
for _ in {1..4}; do
  cmake-build-debug/worker localhost 12345 &
done

# And wait for completion
time wait
