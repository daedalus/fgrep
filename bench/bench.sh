#!/bin/bash
set -e

FGREP="./build/fgrep"
GREP="grep"
RG="rg"
BENCH_DIR="/tmp/fgrep_bench"
RESULTS="/tmp/fgrep_bench_results.txt"

mkdir -p "$BENCH_DIR"
echo "=== FGrep Benchmark Suite ===" | tee "$RESULTS"
echo "" | tee -a "$RESULTS"

generate_log() {
    local size_mb=$1
    local file="$BENCH_DIR/log_${size_mb}mb.txt"
    if [ -f "$file" ]; then
        echo "Reusing $file"
        return
    fi
    echo "Generating ${size_mb}MB log file..."
    python3 -c "
import random, string
for i in range(${size_mb} * 1024 * 64):
    line = ''.join(random.choices(string.ascii_lowercase, k=random.randint(20, 100)))
    if random.random() < 0.1:
        line += ' ERROR ' + ''.join(random.choices(string.digits, k=6))
    print(line)
" > "$file" 2>/dev/null || {
        dd if=/dev/urandom bs=1M count=$size_mb 2>/dev/null | base64 > "$file"
    }
    echo "Generated: $file ($(du -h "$file" | cut -f1))"
}

bench() {
    local desc="$1"
    local pattern="$2"
    local file="$3"
    local flags="$4"
    local size
    size=$(du -h "$file" | cut -f1)

    echo "--- $desc ($size) ---" | tee -a "$RESULTS"

    if [ -x "$GREP" ]; then
        t1=$( { time $GREP $flags "$pattern" "$file" > /dev/null; } 2>&1 )
        echo "  grep:     $t1" | tee -a "$RESULTS"
    fi

    if [ -x "$RG" ]; then
        t2=$( { time $RG $flags "$pattern" "$file" > /dev/null; } 2>&1 )
        echo "  rg:       $t2" | tee -a "$RESULTS"
    fi

    if [ -x "$FGREP" ]; then
        t3=$( { time $FGREP $flags "$pattern" "$file" > /dev/null; } 2>&1 )
        echo "  fgrep:    $t3" | tee -a "$RESULTS"
    fi

    echo "" | tee -a "$RESULTS"
}

generate_log 10

echo "=== Fixed String Search ===" | tee -a "$RESULTS"
bench "Fixed string in 10MB log" "ERROR" "$BENCH_DIR/log_10mb.txt" "-F"

echo "=== Regex Search ===" | tee -a "$RESULTS"
bench "Regex [0-9]+ in 10MB log" "[0-9]+" "$BENCH_DIR/log_10mb.txt" ""

echo "=== Case Insensitive ===" | tee -a "$RESULTS"
bench "Case-insensitive in 10MB log" "error" "$BENCH_DIR/log_10mb.txt" "-i -F"

echo "=== Line Number ===" | tee -a "$RESULTS"
bench "Line numbers in 10MB log" "ERROR" "$BENCH_DIR/log_10mb.txt" "-n -F"

echo "=== Count ===" | tee -a "$RESULTS"
bench "Count matches in 10MB log" "ERROR" "$BENCH_DIR/log_10mb.txt" "-c -F"

if [ -d /usr/src/linux ] || [ -d /usr/src/linux-* ]; then
    KERNEL_DIR=$(ls -d /usr/src/linux-* 2>/dev/null | head -1)
    if [ -n "$KERNEL_DIR" ] && [ -d "$KERNEL_DIR" ]; then
        echo "=== Source Code Search ===" | tee -a "$RESULTS"
        bench "Fixed string in kernel source" "CONFIG_DEBUG" "$KERNEL_DIR" "-r -F"
    fi
fi

echo "" | tee -a "$RESULTS"
echo "=== Summary ===" | tee -a "$RESULTS"
echo "Benchmark completed at $(date)" | tee -a "$RESULTS"
echo "Full results saved to: $RESULTS"
