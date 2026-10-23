#!/bin/bash
# Run the parser test suite, but only if source or case files changed since
# the last run. Meant to be started once a day from cron.

export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"   # cron's PATH is minimal

REPO="${MINIDB_REPO:-$HOME/minidb}"
cd "$REPO" || exit 2

LOG_DIR="tests/parser_test/logs"
STAMP="build/.last_tested_hash"
OUT="$LOG_DIR/cron.out"
mkdir -p "$LOG_DIR" build

# One hash over every source / case file. The logs directory is excluded,
# otherwise each run would change the hash by writing its own logs.
hash_sources() {
    find . -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.txt' \) \
        -not -path './build/*' -not -path './.git/*' \
        -not -path "./$LOG_DIR/*" -not -name 'commitmsg.txt' \
        | sort | xargs shasum | shasum | cut -d' ' -f1
}

current=$(hash_sources)
previous=$(cat "$STAMP" 2>/dev/null || echo none)

if [ "$current" = "$previous" ]; then
    echo "$(date '+%F %T') no changes since last run, skipped" >> "$OUT"
    exit 0
fi

{
    echo "=== $(date '+%F %T') changes found, building"
    if ! cmake -S . -B build > build/cron_build.log 2>&1 \
       || ! cmake --build build >> build/cron_build.log 2>&1; then
        echo "BUILD FAILED, last lines of build/cron_build.log:"
        tail -15 build/cron_build.log
        exit 2        # stamp not updated: tomorrow tries again
    fi

    ./build/test_script tests/parser_test/cases/cases.txt
    code=$?
    echo "$current" > "$STAMP"
    echo "test_script exit code: $code"
    exit $code
} >> "$OUT" 2>&1