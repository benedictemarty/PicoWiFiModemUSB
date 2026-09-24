#!/bin/sh
# release.sh — build the release of the version in the VERSION file.
#   tools/release.sh                       check, host tests, tag vX.Y.Z (local),
#                                          build TWICE and require identical UF2s
#                                          → dist/wifi_modem-vX.Y.Z.uf2 (+ .sha256)
#   tools/release.sh --publish NOTES.md    same, then push main + tag to the
#                                          "benedictemarty" remote and create the
#                                          GitHub release (UF2, sha256, and the zip
#                                          bundle if dist/ holds one)
# Prerequisites: clean tree on main, "## [X.Y.Z] — date" section in CHANGELOG.md.
# The user bundle (PicoWiFiModemUSB-vX.Y.Z.zip: UF2 + wificonf.bas + manuals)
# is assembled from the workspace (../oric, ../docs) into dist/ before --publish.
set -eu
cd "$(dirname "$0")/.."
V=$(cat VERSION)
TAG="v$V"
REMOTE=benedictemarty
die() { echo "release: $*" >&2; exit 1; }

[ "$(git rev-parse --abbrev-ref HEAD)" = main ] || die "not on main"
[ -z "$(git status --porcelain)" ] || die "working tree not clean"
grep -q "^## \[$V\] — " CHANGELOG.md || die "CHANGELOG.md: no \"## [$V] — date\" section"
if git rev-parse -q --verify "refs/tags/$TAG" >/dev/null; then
      [ "$(git rev-list -n1 "$TAG")" = "$(git rev-parse HEAD)" ] || die "$TAG already exists on another commit"
fi

echo "== host tests"
./validation/host-tests/run.sh >/dev/null || die "host tests failed (run validation/host-tests/run.sh)"

echo "== tag $TAG"
git rev-parse -q --verify "refs/tags/$TAG" >/dev/null || \
      git tag -a "$TAG" -m "$TAG"

echo "== build (twice, two directories)"
B1=$(mktemp -d); B2=$(mktemp -d)
trap 'rm -rf "${B1:?}" "${B2:?}"' EXIT
for b in "$B1" "$B2"; do
      cmake -S src -B "$b" >/dev/null
      cmake --build "$b" -j8 >/dev/null
done
grep -q "\"$TAG\"" "$B1/build_id.h" || die "unexpected build id: $(cat "$B1/build_id.h")"
cmp -s "$B1/wifi_modem.uf2" "$B2/wifi_modem.uf2" || die "build not reproducible: the two UF2s differ"
mkdir -p dist
cp "$B1/wifi_modem.uf2" "dist/wifi_modem-$TAG.uf2"
(cd dist && sha256sum "wifi_modem-$TAG.uf2" > "wifi_modem-$TAG.uf2.sha256")
echo "dist/wifi_modem-$TAG.uf2: $(cut -d' ' -f1 "dist/wifi_modem-$TAG.uf2.sha256")"

[ "${1:-}" = "--publish" ] || { echo "== done (not published: rerun with --publish NOTES.md)"; exit 0; }
NOTES=${2:-}
[ -f "$NOTES" ] || die "--publish needs a release-notes file"

echo "== publish"
git push -q "$REMOTE" main
git push -q "$REMOTE" "$TAG"
set -- "dist/wifi_modem-$TAG.uf2" "dist/wifi_modem-$TAG.uf2.sha256"
[ -f "dist/PicoWiFiModemUSB-$TAG.zip" ] && set -- "$@" "dist/PicoWiFiModemUSB-$TAG.zip"
gh release create "$TAG" -R benedictemarty/PicoWiFiModemUSB "$@" --title "$TAG" --notes-file "$NOTES"
echo "== published: $TAG"
