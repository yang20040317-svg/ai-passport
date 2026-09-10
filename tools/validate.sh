#!/usr/bin/env bash
set -euo pipefail

mode="${1:---all}"
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    echo "Usage: $0 [--all|--static|--firmware]" >&2
}

run_static_checks() {
    local actionlint_bin
    local test_dir

    python3 tools/check_repo.py

    actionlint_bin="${ACTIONLINT_BIN:-}"
    if [[ -z "${actionlint_bin}" ]]; then
        actionlint_bin="$(command -v actionlint || true)"
    fi
    if [[ -z "${actionlint_bin}" || ! -x "${actionlint_bin}" ]]; then
        actionlint_bin="$(./tools/install-actionlint.sh)"
    fi
    "${actionlint_bin}" -color .github/workflows/*.yml

    test_dir="$(mktemp -d /tmp/ai-passport-host-tests.XXXXXX)"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_ui_pixel_math.c main/ui_pixel_math.c \
        -o "${test_dir}/test_ui_pixel_math"
    "${test_dir}/test_ui_pixel_math"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_muyu_merit.c main/muyu_merit.c \
        -o "${test_dir}/test_muyu_merit"
    "${test_dir}/test_muyu_merit"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_muyu_inbox.c \
        -o "${test_dir}/test_muyu_inbox"
    "${test_dir}/test_muyu_inbox"
    python3 tests/test_verify_firmware.py
    rm -rf "${test_dir}"
    echo "Host tests: PASS"
}

run_firmware_checks() (
    local validation_build_dir
    local extra_defines=()

    if ! command -v idf.py >/dev/null 2>&1; then
        echo "ERROR: idf.py is not available; activate ESP-IDF 5.5.3 first." >&2
        return 1
    fi

    validation_build_dir="$(mktemp -d /tmp/ai-passport-firmware.XXXXXX)"
    trap 'case "${validation_build_dir}" in /tmp/ai-passport-firmware.*) rm -rf -- "${validation_build_dir}" ;; esac' EXIT

    # 把 muyu_* 凭据从 env 透传到 idf.py 的 -D 参数,避免在源码或 sdkconfig 里硬编码。
    # 这些 env 由 CI workflow 注入(repo secrets),或本地开发者 export 后跑 validate.sh。
    [[ -n "${MUYU_INBOX_TOKEN:-}" ]] && extra_defines+=("-DMUYU_INBOX_TOKEN=${MUYU_INBOX_TOKEN}")
    [[ -n "${MUYU_WIFI_SSID:-}"   ]] && extra_defines+=("-DMUYU_WIFI_SSID=${MUYU_WIFI_SSID}")
    [[ -n "${MUYU_WIFI_PASS:-}"   ]] && extra_defines+=("-DMUYU_WIFI_PASS=${MUYU_WIFI_PASS}")

    if [[ ${#extra_defines[@]} -gt 0 ]]; then
        echo "Injecting build defines: ${extra_defines[*]}"
    else
        echo "WARNING: no MUYU_* env vars; Muyu page will fail to compile"
        echo "         (MUYU_INBOX_TOKEN / MUYU_WIFI_SSID / MUYU_WIFI_PASS required)."
    fi

    SDKCONFIG_DEFAULTS="${repo_root}/sdkconfig.defaults" \
        idf.py -B "${validation_build_dir}" \
        -D "SDKCONFIG=${validation_build_dir}/sdkconfig" \
        "${extra_defines[@]}" build
    idf.py -B "${validation_build_dir}" merge-bin \
        -o "${validation_build_dir}/FoloToy-AI-Passport-full.bin"
    python3 tools/verify_firmware.py "${validation_build_dir}"
    mkdir -p "${repo_root}/build"
    install -m 0644 \
        "${validation_build_dir}/FoloToy-AI-Passport-full.bin" \
        "${repo_root}/build/FoloToy-AI-Passport-full.bin"
    echo "Firmware build: PASS"
)

cd "${repo_root}"
case "${mode}" in
    --all)
        run_static_checks
        run_firmware_checks
        ;;
    --static)
        run_static_checks
        ;;
    --firmware)
        run_firmware_checks
        ;;
    *)
        usage
        exit 2
        ;;
esac
