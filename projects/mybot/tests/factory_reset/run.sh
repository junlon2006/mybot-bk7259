#!/usr/bin/env bash
set -euo pipefail
test_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir="$test_dir/../../../.."
platform_dir="$repo_dir/components/mybot/mybot_sdk/platforms/bk7259"
easyflash_dir="$repo_dir/bk_avdk_smp/ap/components/easy_flash/easy_flash_V4.X/inc"
test_binary=$(mktemp /tmp/bk7259-factory-reset-test.XXXXXX)
trap 'rm -f -- "$test_binary"' EXIT
extra_flags=()
if [[ "${FACTORY_RESET_TEST_SANITIZERS:-0}" == 1 ]]; then
    extra_flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror "${extra_flags[@]}" \
    -I"$test_dir/stubs" -I"$platform_dir/include" -I"$easyflash_dir" \
    "$platform_dir/bk7259_factory_reset.c" "$test_dir/test_factory_reset.c" \
    -o "$test_binary"
timeout 10s "$test_binary"
