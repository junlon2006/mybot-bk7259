#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail
test_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(cd -- "$test_dir/../../../.." && pwd)
build_dir=$(mktemp -d /tmp/mybot-wifi-test.XXXXXX)
trap 'rm -f -- "$build_dir/manager" "$build_dir/portal"; rmdir -- "$build_dir"' EXIT
platform="$repo_dir/components/mybot/mybot_sdk/platforms/bk7259"
json="$repo_dir/bk_avdk_smp/ap/components/json"
cc=${CC:-cc}
flags=(-std=c11 -Wall -Wextra -Werror -g -DCONFIG_CJSON_USE=1
       -I"$test_dir/mocks" -I"$platform" -I"$platform/include"
       -I"$repo_dir/components/mybot/mybot_sdk/include" -I"$json")
if [[ ${SANITIZE:-0} == 1 ]]; then
    flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
"$cc" "${flags[@]}" "$test_dir/test_manager.c" "$test_dir/test_env.c" -o "$build_dir/manager"
"$cc" "${flags[@]}" "$test_dir/test_portal.c" "$test_dir/test_env.c" "$json/cJSON.c" -lm \
    -o "$build_dir/portal"
"$build_dir/manager"
"$build_dir/portal"
