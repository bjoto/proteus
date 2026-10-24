#!/usr/bin/env bash
# Called after both platform workflows pass; requires GitHub CLI and GH_TOKEN.
set -euo pipefail

tag="${1:?Usage: publish_release.sh TAG ASSET_DIRECTORY}"
asset_dir="${2:?Usage: publish_release.sh TAG ASSET_DIRECTORY}"
case "$tag" in
    v*) ;;
    *) echo "Expected a version tag starting with v" >&2; exit 1 ;;
esac

assets=(
    "$asset_dir/wake_velocity-${tag}-macos-arm64.zip"
    "$asset_dir/wake_velocity-${tag}-macos-arm64.zip.sha256"
    "$asset_dir/wake_velocity-${tag}-windows-x64.zip"
    "$asset_dir/wake_velocity-${tag}-windows-x64.zip.sha256"
)
for asset in "${assets[@]}"; do
    test -s "$asset"
done

# Keep a new release in draft until every upload succeeds. Reruns reuse the release
# and replace same-named assets, preserving its title, notes, and prerelease status.
if ! gh release view "$tag" >/dev/null 2>&1; then
    options=(--verify-tag --generate-notes --draft)
    if [[ "$tag" == *-* ]]; then
        options+=(--prerelease)
    fi
    gh release create "$tag" "${options[@]}"
fi
gh release upload "$tag" "${assets[@]}" --clobber
gh release edit "$tag" --draft=false
