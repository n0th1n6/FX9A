#!/usr/bin/env bash
# Reproducible standalone FX-9A CMSIS/LL dependencies.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

checkout() {
    local path="$1" url="$2" tag="$3"
    if [[ -d "$root/$path/.git" ]]; then
        git -C "$root/$path" fetch --depth 1 origin "refs/tags/$tag:refs/tags/$tag"
        git -C "$root/$path" checkout --detach "tags/$tag"
    else
        git clone --quiet --depth 1 --branch "$tag" "$url" "$root/$path"
    fi
    printf '%s %s %s\n' "$path" "$tag" "$(git -C "$root/$path" rev-parse HEAD)"
}

checkout cmsis-core https://github.com/ARM-software/CMSIS_5.git 5.9.0
checkout cmsis-device-f1 https://github.com/STMicroelectronics/cmsis-device-f1.git v4.3.5
checkout stm32f1xx-hal-driver https://github.com/STMicroelectronics/stm32f1xx-hal-driver.git v1.1.10
