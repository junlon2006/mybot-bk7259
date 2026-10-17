#!/usr/bin/env bash
set -euo pipefail
test_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir="$test_dir/../../../.."
platform_dir="$repo_dir/components/mybot/mybot_sdk/platforms/bk7259"
test_binary=$(mktemp /tmp/bk7259-reset-owner-test.XXXXXX)
trap 'rm -f -- "$test_binary"' EXIT
extra_flags=()
if [[ "${RESET_OWNER_TEST_SANITIZERS:-0}" == 1 ]]; then
    extra_flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror "${extra_flags[@]}" \
    -I"$test_dir/stubs" -I"$platform_dir/include" \
    -I"$repo_dir/components/mybot/mybot_sdk/include" \
    "$test_dir/test_reset_owner.c" -o "$test_binary"
timeout 10s "$test_binary"
