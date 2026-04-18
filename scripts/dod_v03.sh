#!/usr/bin/env bash
# Definition-of-done gate for the zephlet v0.3 refactor.
#
# Verifies, top to bottom:
#   1. Each per-zephlet integration suite passes (tick, ui, tampering).
#   2. A freshly scaffolded zephlet (via Copier, from a location that is
#      NOT under src/zephlets/) builds and passes its own integration
#      tests — proves the framework is layout-agnostic.
#   3. The full example app builds and runs end-to-end on mps2/an385;
#      tick events fire ui_blink via adapters.c; tampering's
#      force_tampering also fires ui_blink.
#
# Intended to run from the app repo root. Assumes west workspace is
# initialized and the Zephyr SDK + Python deps are installed.

set -euo pipefail

APP_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
INFRA_ROOT="$(cd "${APP_ROOT}/../modules/lib/zephlet" && pwd)"
BOARD="mps2/an385"
SCAFFOLD_BASE="/tmp/zephlet_dod_scaffold"

log()  { printf '\033[1;34m[DoD]\033[0m %s\n' "$*"; }
pass() { printf '\033[1;32m[PASS]\033[0m %s\n' "$*"; }
fail() { printf '\033[1;31m[FAIL]\033[0m %s\n' "$*" >&2; exit 1; }

run_zephlet_suite() {
    local name=$1
    local test_dir="${APP_ROOT}/src/${name}/tests/integration"
    local build_dir="/tmp/dod_${name}_build"

    log "Building ${name} integration test..."
    (cd "${APP_ROOT}" && \
        west build -b "${BOARD}" -p always "${test_dir}" -d "${build_dir}" >/dev/null 2>&1) \
        || fail "${name} integration test did not build"

    log "Running ${name} integration test..."
    local output
    output=$(cd "${APP_ROOT}" && timeout 30 west build -d "${build_dir}" -t run 2>&1 || true)
    if ! grep -q "SUITE PASS.*\[${name}_v03\]" <<<"${output}"; then
        echo "${output}" | tail -40
        fail "${name} integration suite did not report SUITE PASS"
    fi
    pass "${name} integration suite"
}

run_scaffold_check() {
    log "Scaffolding a fresh zephlet outside src/zephlets/..."
    rip "${SCAFFOLD_BASE}" 2>/dev/null || rm -rf "${SCAFFOLD_BASE}" 2>/dev/null || true
    mkdir -p "${SCAFFOLD_BASE}"
    copier copy --defaults --data zephlet_name=Dodcheck \
        --data author_name="DoD" \
        "${INFRA_ROOT}/codegen/zephyr_zephlet_template" "${SCAFFOLD_BASE}" \
        >/dev/null 2>&1 \
        || fail "copier scaffold failed"

    local test_dir="${SCAFFOLD_BASE}/dodcheck/tests/integration"
    local build_dir="/tmp/dod_scaffold_build"
    (cd "${APP_ROOT}" && \
        west build -b "${BOARD}" -p always "${test_dir}" -d "${build_dir}" >/dev/null 2>&1) \
        || fail "scaffolded zephlet did not build at ${SCAFFOLD_BASE}/dodcheck"

    local output
    output=$(cd "${APP_ROOT}" && timeout 30 west build -d "${build_dir}" -t run 2>&1 || true)
    grep -q "SUITE PASS.*\[dodcheck_integration\]" <<<"${output}" \
        || { echo "${output}" | tail -40; fail "scaffolded zephlet suite did not pass"; }
    pass "scaffolded zephlet at unusual location (${SCAFFOLD_BASE}/dodcheck)"
}

run_full_app() {
    log "Building the full app..."
    (cd "${APP_ROOT}" && just c b >/dev/null 2>&1) \
        || fail "full app did not build with 'just c b'"

    log "Running the full app under qemu..."
    local output
    output=$(cd "${APP_ROOT}" && timeout 15 just r 2>&1 || true)

    grep -q "tick event @.*-> ui_blink"              <<<"${output}" \
        || { echo "${output}" | tail -40; fail "tick -> ui_blink adapter did not fire"; }
    grep -q "tampering proximity @.*-> ui_blink"     <<<"${output}" \
        || { echo "${output}" | tail -40; fail "tampering -> ui_blink adapter did not fire"; }
    grep -q "blink #[1-9]"                            <<<"${output}" \
        || { echo "${output}" | tail -40; fail "ui_blink handler did not run"; }
    pass "full app boot + adapters (tick + tampering → ui_blink)"
}

log "Zephlet v0.3 Definition-of-Done gate"
log "App root  : ${APP_ROOT}"
log "Infra root: ${INFRA_ROOT}"

run_zephlet_suite tick
run_zephlet_suite ui
run_zephlet_suite tampering
run_scaffold_check
run_full_app

echo
pass "All DoD checks green."
