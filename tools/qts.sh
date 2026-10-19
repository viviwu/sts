#!/bin/bash
# QTS one-click dev entry: build, test, drill, demo, admin queries.
# C++ stays the project language; this is only a local convenience wrapper
# (bash is accepted for aux scripts). Usage: ./tools/qts.sh <cmd> [args].
#   build        configure + build (Debug, warnings as errors, like CI)
#   test         build + ctest (31 tests, incl. throughput bench)
#   drill        build + one-shot paper drill (default config)
#   demo [ticks [interval_ms]]
#                fresh long-run demo: wipe bin/db, run gateway,
#                then print orders/positions/reconcile/db summary
#   dual [ticks [interval_ms]]
#                same over the dual-paper routed config (simulated+alternate)
#   ashare [ticks [interval_ms]]
#                strict A-share business demo (master + T+1 + fees + inventory)
#   multi [ticks [interval_ms]]
#                multi-symbol multi-strategy demo (AMA feed, 2 symbols × 3 strategies)
  #   live-sim [ticks [interval_ms]]
  #                strategy auto-fire against the SIMULATION counter over the
  #                production QuantAPI path (needs a vendor-gated build; the
  #                enabled strategy needs a counter session, so automation
  #                env (QTS_QUANTAPI_* identity+password) pre-logs in at boot
  #                — same login the gateway would do. File feed, so it runs
  #                after close)
#   replay <report|verify> [qts_replay flags...]
#                offline replay over the demo ledger (report or M2 verify)
#   debug-demo   demo with log.level=debug (full quote->intent->fill trace)
#   admin <qts_admin args...>
#   backup                 timestamped backup of bin/db + bin/log with manifest
#   recover-drill          backup -> wipe -> restore -> verify (server must be stopped)
#                qts_admin with --ledger/--sqlite defaulted to demo files
#   format       clang-format --dry-run --Werror (needs 18.1.8 for CI parity)
#   secrets      secret scan (same as CI secrets job)
#   clean        remove build/
#   all          build + test + drill (the pre-push gate)
#   doctor       preflight: report every missing build/run input (toolchain,
#                oss pins, vendor SDKs, env credentials, gateway reachability)
#                with the exact fix for each gap; exits 1 when anything is
#                missing so a fresh machine tells you in seconds what to
#                complete instead of failing halfway through a build.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
BIN="$ROOT/bin"
CFG="$BIN/cfg"
DEMO_YAML="$CFG/demo.yaml"
PAPER_YAML="$CFG/paper.example.yaml"
LEDGER="$BIN/db/qts-demo-ledger.jsonl"
DB="$BIN/db/qts-demo.db"
LOGDIR="$BIN/log"

# This box defaults to GCC 8, which cannot link std::filesystem: prefer
# gcc-toolset-11 when the current compiler is older (matches dev env).
if [ -f /opt/rh/gcc-toolset-11/enable ]; then
  major=$(g++ -dumpversion 2>/dev/null | cut -d. -f1 || echo 0)
  if [ "$major" -lt 11 ]; then
    # shellcheck disable=SC1091
    source /opt/rh/gcc-toolset-11/enable
  fi
fi

JOBS=$(nproc 2>/dev/null || echo 4)

build() {
  cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Debug -DQTS_ENABLE_WARNINGS_AS_ERRORS=ON
  cmake --build "$BUILD" -j"$JOBS"
}

# Binaries land in ./bin (QTS_RUNTIME_DIR); run them from there so relative
# artifact paths stay inside the unified layout.
SERVER="$BIN/qts_server"
ADMIN="$BIN/qts_admin"
REPLAY="$BIN/qts_replay"

# Shared long-run driver: temp config lives in /tmp, so artifacts are
# anchored with absolute paths (ledger, db, log pattern, state file). Ends with admin + replay summary.
run_demo() {
  base_cfg=$1
  ledger=$2
  db=$3
  log_pattern=$4
  log_glob=$5
  ticks=$6
  interval=$7
  state_file=$8
  cfg=$(mktemp /tmp/qts-demo-XXXXXX.yaml)
  fee_dir=$(dirname "$base_cfg")
  sed -e "s|^ledger_file: .*|ledger_file: $ledger|" -e "s|^sqlite_file: .*|sqlite_file: $db|" -e "s|^  file: .*|  file: $log_pattern|" -e "s|^state_file: .*|state_file: $state_file|" -e 's|^  fee_file: \([^/"].*\)|  fee_file: '"$fee_dir"'/\1|' -e "s/^  ticks: .*/  ticks: $ticks/" -e "s/^  interval_ms: .*/  interval_ms: $interval/" "$base_cfg" > "$cfg"
  "$SERVER" "$cfg"
  rm -f "$cfg"
  echo "=== admin summary ==="
  "$ADMIN" --ledger "$ledger" reconcile
  "$ADMIN" --ledger "$ledger" positions
  "$ADMIN" --sqlite "$db" db
  "$REPLAY" verify --ledger "$ledger"
  "$ADMIN" state --state-file "$state_file"
  echo "=== newest log ==="
  ls -t "$LOGDIR"/$log_glob | head -n 1
}

cmd=${1:-all}
shift || true

case "$cmd" in
  build)
    build
    ;;
  test)
    build
    ctest --test-dir "$BUILD" --output-on-failure
    ;;
  drill)
    build
    "$SERVER" "$PAPER_YAML"
    ;;
  demo)
    ticks=${1:-500}
    interval=${2:-100}
    build
    # Fresh demo: ledger + db are wiped so counts start from zero, but log
    # history is kept (each startup resolves to a unique file name anyway).
    rm -f "$LEDGER" "$DB"
    run_demo "$DEMO_YAML" "$LEDGER" "$DB" "$LOGDIR/qts-demo.log" "qts-demo-*.log" "$ticks" "$interval" "$BIN/db/live-state.json"
    ;;
  dual)
    ticks=${1:-500}
    interval=${2:-100}
    build
    rm -f "$BIN/db/dual-ledger.jsonl" "$BIN/db/dual.db"
    run_demo "$CFG/dual-paper.yaml" "$BIN/db/dual-ledger.jsonl" "$BIN/db/dual.db" "$LOGDIR/dual-paper.log" "dual-paper-*.log" "$ticks" "$interval" "$BIN/db/dual-state.json"
    ;;
  ashare)
    ticks=${1:-200}
    interval=${2:-0}
    build
    rm -f "$BIN/db/ashare-ledger.jsonl" "$BIN/db/ashare.db"
    run_demo "$CFG/demo-ashare.yaml" "$BIN/db/ashare-ledger.jsonl" "$BIN/db/ashare.db" "$LOGDIR/qts-ashare.log" "qts-ashare-*.log" "$ticks" "$interval" "$BIN/db/ashare-state.json"
    ;;
  multi)
    ticks=${1:-200}
    interval=${2:-0}
    build
    rm -f "$BIN/db/multi-ledger.jsonl" "$BIN/db/multi.db"
    run_demo "$CFG/demo-multi.yaml" "$BIN/db/multi-ledger.jsonl" "$BIN/db/multi.db" "$LOGDIR/qts-multi.log" "qts-multi-*.log" "$ticks" "$interval" "$BIN/db/multi-state.json"
    ;;
  live-sim)
    # No build here: live-sim needs a vendor-gated server (unknown broker
    # quantapi fails closed otherwise). Its enabled strategy needs a counter
    # session, so automation env pre-logs in at boot (C/S traders log in via
    # the gateway instead and need no env at the server).
    ticks=${1:-12}
    interval=${2:-500}
    rm -f "$BIN/db/live-sim-ledger.jsonl" "$BIN/db/live-sim.db"
    cfg=$(mktemp /tmp/qts-live-sim-XXXXXX.yaml)
    sed -e "s|^ledger_file: .*|ledger_file: $BIN/db/live-sim-ledger.jsonl|" -e "s|^sqlite_file: .*|sqlite_file: $BIN/db/live-sim.db|" -e "s|^state_file: .*|state_file: $BIN/db/live-sim-state.json|" -e "s/^  ticks: .*/  ticks: $ticks/" -e "s/^  interval_ms: .*/  interval_ms: $interval/" -e "s|^  file: live-sim-quotes.csv|  file: $CFG/live-sim-quotes.csv|" -e "/^log:/{n;s|^  file: .*|  file: $LOGDIR/qts-live-sim.log|}" "$CFG/live-sim.yaml" > "$cfg"
    "$SERVER" "$cfg"
    rm -f "$cfg"
    echo "=== admin summary ==="
    "$ADMIN" --ledger "$BIN/db/live-sim-ledger.jsonl" reconcile
    "$ADMIN" --ledger "$BIN/db/live-sim-ledger.jsonl" positions
    "$ADMIN" --sqlite "$BIN/db/live-sim.db" db
    "$REPLAY" verify --ledger "$BIN/db/live-sim-ledger.jsonl"
    "$ADMIN" state --state-file "$BIN/db/live-sim-state.json"
    ;;
  replay)
    build
    if [[ "$*" == *"--ledger"* ]]; then
      "$REPLAY" "$@"
    else
      "$REPLAY" "$@" --ledger "$LEDGER"
    fi
    ;;
  debug-demo)
    build
    rm -f "$BIN"/db/qts-dbg-ledger.jsonl "$BIN"/db/qts-dbg.db "$LOGDIR"/qts-dbg-*.log
    cfg=$(mktemp /tmp/qts-debug-demo-XXXXXX.yaml)
    sed -e "s|^ledger_file: .*|ledger_file: $BIN/db/qts-dbg-ledger.jsonl|" -e "s|^sqlite_file: .*|sqlite_file: $BIN/db/qts-dbg.db|" -e "s|^  file: .*|  file: $LOGDIR/qts-dbg.log|" -e "s|^state_file: .*|state_file: $BIN/db/qts-dbg-state.json|" -e 's/level: info/level: debug/' "$DEMO_YAML" > "$cfg"
    "$SERVER" "$cfg" | grep -c '"level":"debug"'
    rm -f "$cfg"
    ;;
  admin)
    build
    if [ "${1:-}" = "db" ]; then
      "$ADMIN" db --sqlite "$DB" "${@:2}"
    elif [ "${1:-}" = "state" ]; then
      if [[ "$*" == *"--state-file"* ]]; then
        "$ADMIN" "${@:1}"
      else
        "$ADMIN" state --state-file "$BIN/db/live-state.json" "${@:2}"
      fi
    else
      "$ADMIN" "$@" --ledger "$LEDGER"
    fi
    ;;
  format)
    fmt=clang-format
    [ -x "$HOME/.local/bin/clang-format" ] && fmt="$HOME/.local/bin/clang-format"
    # shellcheck disable=SC2046
    "$fmt" --dry-run --Werror $(cd "$ROOT" && git ls-files '*.cpp' '*.hpp') && echo "format clean"
    ;;
  secrets)
    bash "$ROOT/tools/secret_scan.sh"
    ;;
  backup)
    # Timestamped backup of the full runtime state set (ledger + db + seq +
    # snapshot + logs) with a manifest (watermark, git rev). Restore = stop
    # server, extract over bin/, start (boot recovery runs automatically).
    stamp=$(date +%Y%m%d-%H%M%S)
    out="$ROOT/backup/qts-backup-$stamp.tgz"
    mkdir -p "$ROOT/backup"
    watermark=$("$ADMIN" --sqlite "$DB" db 2>/dev/null | head -n 1 || echo "db: unreachable")
    {
      echo "stamp=$stamp"
      echo "rev=$(cd "$ROOT" && git rev-parse --short HEAD 2>/dev/null || echo unknown)"
      echo "$watermark"
    } > /tmp/qts-manifest.txt
    cp /tmp/qts-manifest.txt "$BIN/db/MANIFEST.txt"
    tar czf "$out" -C "$ROOT" bin/db bin/log
    rm -f "$BIN/db/MANIFEST.txt" /tmp/qts-manifest.txt
    echo "backup: $out"
    tar tzf "$out" | head -n 8
    ;;
  recover-drill)
    # Automated disaster-recovery exercise: backup -> wipe -> restore ->
    # verify. Prints PASS/FAIL; save the output as the drill record.
    # The server must be stopped first (refuses otherwise).
    if pgrep -f "qts_server" > /dev/null; then
      echo "recover-drill FAILED: stop the server first" >&2
      exit 1
    fi
    build
    before_ledger="$LEDGER"
    before_watermark=$("$ADMIN" --sqlite "$DB" db 2>/dev/null | head -n 1 || echo "db: unreachable")
    stamp=$(date +%Y%m%d-%H%M%S)
    mkdir -p "$ROOT/backup"
    tar czf "$ROOT/backup/qts-drill-$stamp.tgz" -C "$ROOT" bin/db
    echo "drill backup: $ROOT/backup/qts-drill-$stamp.tgz ($before_watermark)"
    rm -f "$BIN"/db/* "$BIN"/log/*.log
    tar xzf "$ROOT/backup/qts-drill-$stamp.tgz" -C "$ROOT"
    after_watermark=$("$ADMIN" --sqlite "$DB" db 2>/dev/null | head -n 1 || echo "db: unreachable")
    echo "after restore: $after_watermark"
    "$REPLAY" verify --ledger "$before_ledger"
    if [ "$before_watermark" = "$after_watermark" ] && [ -n "$before_ledger" ]; then
      echo "recover-drill PASS: watermark identical, ledger verifies"
    else
      echo "recover-drill FAILED: watermark mismatch" >&2
      exit 1
    fi
    ;;
  clean)
    rm -rf "$BUILD"
    ;;
  doctor)
    # Preflight for a fresh machine: every check is local and fast (the only
    # network touch is a 2s TCP probe per gateway). Missing items print the
    # exact completion step; values of secrets are never printed.
    fail=0
    say() { [ "$2" = "ok" ] && echo "  [ok] $1" || { echo "  [MISS] $1"; fail=1; }; }
    fix() { echo "        fix: $1"; }
    echo "== toolchain =="
    command -v cmake > /dev/null && say "cmake $(cmake --version 2>/dev/null | head -n 1)" ok || { say "cmake not found" miss; fix "install cmake >= 3.20"; }
    if command -v g++ > /dev/null; then
      major=$(g++ -dumpversion 2>/dev/null | cut -d. -f1 || echo 0)
      say "g++ $(g++ --version 2>/dev/null | head -n 1)" ok
      [ "$major" -lt 11 ] && { echo "        note: g++ < 11 cannot link std::filesystem; use gcc-toolset-11 (see runbook) or link -lstdc++fs"; }
    else
      say "g++ not found" miss; fix "install a C++17 compiler (gcc 11+ preferred)"
    fi
    echo "== oss pins (offline builds; else first configure downloads them) =="
    OSS="$ROOT/../vendor/oss"
    [ -f "$OSS/yaml-cpp-0.8.0/CMakeLists.txt" ] && say "yaml-cpp 0.8.0" ok || { say "yaml-cpp 0.8.0 ($OSS/yaml-cpp-0.8.0)" miss; fix "extract the 0.8.0 release there (exact pin, see ../vendor/README.md)"; }
    [ -f "$OSS/json-3.11.3/CMakeLists.txt" ] && say "nlohmann/json v3.11.3" ok || { say "nlohmann/json v3.11.3 ($OSS/json-3.11.3)" miss; fix "extract the v3.11.3 release there (exact pin, see ../vendor/README.md)"; }
    [ -f "$OSS/sqlite-3510100/sqlite3.c" ] && say "sqlite amalgamation 3.51.1" ok || { say "sqlite amalgamation 3.51.1 ($OSS/sqlite-3510100/sqlite3.c)" miss; fix "extract sqlite-amalgamation-3510100.zip there (exact pin, see ../vendor/README.md)"; }
    echo "== vendor SDKs (QTS_VENDOR_DIR=${QTS_VENDOR_DIR:-$ROOT/vendor}, in-repo since ADR-040) =="
    VENDOR="${QTS_VENDOR_DIR:-$ROOT/vendor}"
    [ -f "$VENDOR/ama/include/ama.h" ] && say "ama/include/ama.h" ok || { say "ama/include/ama.h" miss; fix "copy <ama-sdk>/include/ whole tree to $VENDOR/ama/include (V4.5.8.250331, see vendor/README.md)"; }
    for lib in libama.so librum.so librmm.so libllmi.so libfmdutil.so; do
      [ -f "$VENDOR/ama/lib/Lnx64/$lib" ] && say "ama/lib/Lnx64/$lib" ok || { say "ama/lib/Lnx64/$lib" miss; fix "copy from <ama-sdk>/lib/Lnx64/ (demo/.lic/passwords never enter the repo)"; }
    done
    for hdr in atp_quant_api.h atp_quant_constant.h atp_quant_msg.h; do
      [ -f "$VENDOR/quantapi/include/$hdr" ] && say "quantapi/include/$hdr" ok || { say "quantapi/include/$hdr" miss; fix "copy from <quantapi-sdk>/include/ (V2.7.1.2 verified, see vendor/README.md)"; }
    done
    [ -f "$VENDOR/quantapi/lib/libatpquantapi.so" ] && say "quantapi/lib/libatpquantapi.so" ok || { say "quantapi/lib/libatpquantapi.so" miss; fix "copy from <quantapi-sdk>/lib/"; }
    echo "== env credentials, automation pre-login only (names only, values never printed) =="
    echo "   (trader-driven C/S needs none at the server: the trader logs in via the gateway)"
    for var in QTS_QUANTAPI_CUST_ID QTS_QUANTAPI_FUND_ACCOUNT_ID QTS_QUANTAPI_PASSWORD QTS_QUANTAPI_CLIENT_FEATURE_CODE; do
      [ -n "${!var:-}" ] && say "$var=set" ok || { say "$var is empty" miss; fix "export $var=<value> (sim-lab values live outside the repo)"; }
    done
    for var in QTS_QUANTAPI_AGW_USER QTS_QUANTAPI_AGW_PASSWORD; do
      [ -n "${!var:-}" ] && say "$var=set" ok || echo "  [--] $var is empty (optional, but must be set as a pair)"
    done
    for var in QTS_AMA_CUST_ID QTS_AMA_PASSWORD QTS_AMA_FUND_ACCOUNT_ID; do
      [ -n "${!var:-}" ] && say "$var=set" ok || { say "$var is empty" miss; fix "export $var=<value> (sim-lab values live outside the repo)"; }
    done
    echo "== gateway TCP (2s probe) =="
    eps=$(echo "${QTS_QUANTAPI_LOCATIONS:-172.16.109.17:32001} ${QTS_AMA_UMS:-172.16.109.14:8200 172.16.109.15:8200}" | tr ',' ' ')
    for ep in $eps; do
      h=${ep%:*}
      p=${ep##*:}
      if python3 -c "import socket,sys; socket.create_connection(('$h',$p),timeout=2).close()" 2>/dev/null; then
        say "$h:$p open" ok
      else
        say "$h:$p unreachable" miss; fix "check VPN/route to the sim lab (order flow needs the gateway, not just files)"
      fi
    done
    [ "$fail" = "0" ] && echo "doctor: READY" || { echo "doctor: MISSING INPUTS (see fix lines above)" >&2; exit 1; }
    ;;
  all)
    build
    ctest --test-dir "$BUILD" --output-on-failure
    "$SERVER" "$PAPER_YAML"
    ;;
  *)
    echo "usage: $0 <build|test|drill|demo [ticks [interval_ms]]|dual [ticks [interval_ms]]|ashare [ticks [interval_ms]]|multi [ticks [interval_ms]]|live-sim [ticks [interval_ms]]|replay <report|verify>|debug-demo|admin ...|backup|recover-drill|format|secrets|clean|all|doctor>" >&2
    exit 2
    ;;
esac
