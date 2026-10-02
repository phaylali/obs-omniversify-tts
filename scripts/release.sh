#!/usr/bin/env bash
# One-command release: GitHub tag + AUR update.
#
#   ./scripts/release.sh <version> [pkgrel] [--dry-run]
#
#   ./scripts/release.sh 0.3.0          # bump, tag v0.3.0, push GitHub, update AUR
#   ./scripts/release.sh 0.3.0 2        # same, but pkgrel=2 (re-release of 0.3.0)
#   ./scripts/release.sh 0.2.0 --dry-run  # everything except git push / AUR push
#
# Steps it performs:
#   1. Verify a clean git tree and that the tag doesn't already exist
#   2. Bump project VERSION in CMakeLists.txt, commit
#   3. Tag vX.Y.Z, push commit + tag to GitHub
#   4. Download the tag's tarball from GitHub, compute sha256 (retry until available)
#   5. Update pkgver/pkgrel/sha256sums in packaging/aur/PKGBUILD, regen .SRCINFO
#   6. Test-build the package with makepkg
#   7. Commit the packaging bump, push to GitHub
#   8. Fresh-clone the AUR repo, copy PKGBUILD + .SRCINFO, push to the AUR

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PKGNAME="obs-omniversify-multichat-plugin"
GH_REPO="phaylali/obs-omniversify-tts"
AUR_DIR="$REPO_ROOT/packaging/aur"

die() { echo "❌ $*" >&2; exit 1; }
step() { echo; echo "==> $*"; }

# ---- args ------------------------------------------------------------------
VERSION="${1:-}"
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || die "usage: $0 <X.Y.Z> [pkgrel] [--dry-run]"
shift
PKGREL=1
DRY_RUN=0
for arg in "$@"; do
  case "$arg" in
    --dry-run) DRY_RUN=1 ;;
    [0-9]*) PKGREL="$arg" ;;
    *) die "unknown argument: $arg" ;;
  esac
done

TAG="v$VERSION"
cd "$REPO_ROOT"

# ---- 1. preflight ----------------------------------------------------------
step "Preflight"
[[ -z "$(git status --porcelain)" ]] || die "working tree is not clean — commit or stash first"
git fetch origin --tags -q || die "cannot fetch GitHub"
TAG_EXISTS=0
git rev-parse -q --verify "refs/tags/$TAG" >/dev/null && TAG_EXISTS=1
if [[ $TAG_EXISTS -eq 1 && $DRY_RUN -eq 0 ]]; then
  die "tag $TAG already exists"
fi
[[ -f "$AUR_DIR/PKGBUILD" ]] || die "missing $AUR_DIR/PKGBUILD"
command -v makepkg >/dev/null || die "makepkg not found"
echo "clean tree, pkgrel=$PKGREL$([[ $DRY_RUN -eq 1 ]] && echo ' (dry run: no commits/pushes)')"

# ---- 2. bump CMakeLists version -------------------------------------------
step "Bumping CMakeLists.txt VERSION -> $VERSION"
sed -i -E "s/(project\($PKGNAME VERSION )[0-9.]+/\1$VERSION/" CMakeLists.txt
grep -E "project\($PKGNAME VERSION" CMakeLists.txt || die "VERSION bump failed — check project() line"
if [[ $DRY_RUN -eq 0 ]]; then
  git add CMakeLists.txt
  git commit -q -m "chore: release $TAG"
fi

# ---- 3. tag + push GitHub --------------------------------------------------
if [[ $DRY_RUN -eq 1 ]]; then
  echo "[dry-run] would: git tag -a $TAG, push commit + tag to GitHub"
else
  step "Tagging $TAG and pushing to GitHub"
  git tag -a "$TAG" -m "Release $TAG"
  git push origin HEAD -q
  git push origin "$TAG" -q
  echo "pushed commit + tag to GitHub"
fi

# ---- 4. fetch tarball + sha256 --------------------------------------------
step "Fetching GitHub tarball for $TAG"
TARBALL="$AUR_DIR/$PKGNAME-$VERSION.tar.gz"
URL="https://github.com/$GH_REPO/archive/refs/tags/$TAG.tar.gz"
SHA=""
for attempt in $(seq 1 10); do
  if curl -fsSL -o "$TARBALL" "$URL" 2>/dev/null; then
    # GitHub may serve a stale/placeholder archive right after a push;
    # verify it actually contains our version before trusting the hash.
    if tar -tzf "$TARBALL" | head -1 | grep -q "$GH_REPO\|-$VERSION/"; then
      SHA=$(sha256sum "$TARBALL" | cut -d' ' -f1)
      break
    fi
  fi
  echo "  not ready (attempt $attempt) — waiting 6s for GitHub to generate the archive"
  sleep 6
done
[[ -n "$SHA" ]] || die "could not fetch a valid tarball from $URL"
echo "sha256: $SHA"

# ---- 5. update PKGBUILD + .SRCINFO ----------------------------------------
step "Updating PKGBUILD (pkgver=$VERSION pkgrel=$PKGREL)"
sed -i -E "s/^pkgver=.*/pkgver=$VERSION/" "$AUR_DIR/PKGBUILD"
sed -i -E "s/^pkgrel=.*/pkgrel=$PKGREL/" "$AUR_DIR/PKGBUILD"
sed -i -E "s/^sha256sums=.*/sha256sums=('$SHA')/" "$AUR_DIR/PKGBUILD"
grep -E "^(pkgver|pkgrel|sha256sums)" "$AUR_DIR/PKGBUILD"

# makepkg runs from a clean PATH: without this, python3 resolves to uv's
# interpreter and module checks fail (see DEV_NOTES.md).
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin
(cd "$AUR_DIR" && makepkg --printsrcinfo > .SRCINFO)
echo ".SRCINFO regenerated"

# ---- 6. test build ---------------------------------------------------------
step "Test-building the package"
rm -rf "$AUR_DIR/src" "$AUR_DIR/pkg"
(cd "$AUR_DIR" && makepkg -f --noconfirm >/tmp/opencode/release-build.log 2>&1) \
  || { tail -20 /tmp/opencode/release-build.log; die "makepkg failed — see log above"; }
echo "package builds: $(ls "$AUR_DIR"/$PKGNAME-$VERSION-$PKGREL-*.pkg.tar.zst)"

# ---- 7. commit packaging bump (GitHub) ------------------------------------
if [[ $DRY_RUN -eq 1 ]]; then
  echo "[dry-run] would: commit CMakeLists.txt + PKGBUILD bump, push to GitHub"
else
  step "Committing packaging bump"
  git add CMakeLists.txt "$AUR_DIR/PKGBUILD"
  if [[ -n "$(git status --porcelain CMakeLists.txt "$AUR_DIR/PKGBUILD")" ]]; then
    git commit -q -m "aur: update PKGBUILD to $VERSION-$PKGREL"
  fi
  git push origin HEAD -q
  echo "pushed packaging bump to GitHub"
fi

# ---- 8. push to AUR --------------------------------------------------------
step "Pushing to the AUR"
AUR_CLONE="$(mktemp -d)/aur"
if [[ $DRY_RUN -eq 1 ]]; then
  echo "[dry-run] would: git clone ssh://aur@aur.archlinux.org/$PKGNAME.git, copy PKGBUILD+.SRCINFO, push master"
else
  git clone -q "ssh://aur@aur.archlinux.org/$PKGNAME.git" "$AUR_CLONE" \
    || die "AUR clone failed (SSH key not registered?)"
  cp "$AUR_DIR/PKGBUILD" "$AUR_DIR/.SRCINFO" "$AUR_CLONE/"
  (
    cd "$AUR_CLONE"
    git add PKGBUILD .SRCINFO
    if [[ -n "$(git status --porcelain)" ]]; then
      git -c user.name="phaylali" -c user.email="phaylali@users.noreply.github.com" \
          commit -q -m "Update to $VERSION-$PKGREL"
      git push origin master -q
      echo "AUR updated: $PKGNAME $VERSION-$PKGREL"
    else
      echo "AUR already up to date"
    fi
  )
  rm -rf "$(dirname "$AUR_CLONE")"
fi

echo
if [[ $DRY_RUN -eq 1 ]]; then
  git checkout -q CMakeLists.txt "$AUR_DIR/PKGBUILD" 2>/dev/null || true
  echo "✅ Dry run complete — nothing was pushed, working tree restored"
else
  echo "✅ Release $TAG ($VERSION-$PKGREL) complete"
  echo "   GitHub: https://github.com/$GH_REPO/releases/tag/$TAG"
  echo "   AUR:    https://aur.archlinux.org/packages/$PKGNAME"
fi
