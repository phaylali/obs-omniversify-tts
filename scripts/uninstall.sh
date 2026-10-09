#!/usr/bin/env bash
# Uninstall the local obs-omniversify-multichat-plugin (for clean-install testing).
#
#   ./scripts/uninstall.sh            # remove plugin files, keep config + voices
#   ./scripts/uninstall.sh --purge    # also remove config + downloaded voices
#   ./scripts/uninstall.sh --dry-run  # show what would be removed, change nothing
#
# Covers every install method this machine has seen:
#   * pacman package      (pacman -U of the AUR-built package)
#   * dev install         (./build.sh copies a bare .so into /usr/lib/obs-plugins)
#   * old pre-plugin user install (~/.config/obs-studio/plugins/obs-omniversify-tts*)
#
# OBS is closed first (the backend dies with it via PDEATHSIG), and any stray
# backend process is killed. --purge forces the fresh-install test: the next
# OBS start re-downloads the default voice automatically.

set -euo pipefail

PKG="obs-omniversify-multichat-plugin"
SO="/usr/lib/obs-plugins/$PKG.so"
SHARE="/usr/share/$PKG"
CONF_DIR="$HOME/.config/omniversify"
CONF="$CONF_DIR/$PKG.conf"
DATA_DIR="$HOME/.local/share/obs-omniversify-multichat"
USER_PLUGIN_GLOB="$HOME/.config/obs-studio/plugins/obs-omniversify-*"

PURGE=0
DRY=0
for arg in "$@"; do
  case "$arg" in
    --purge)   PURGE=1 ;;
    --dry-run) DRY=1 ;;
    -h|--help) sed -n '2,15p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "unknown option: $arg (try --help)" >&2; exit 1 ;;
  esac
done

step()  { echo; echo "==> $*"; }
run()   { if [[ $DRY -eq 1 ]]; then echo "  [dry-run] $*"; else "$@"; fi; }
# sudo with password on stdin when piped, normal prompt on a tty
sudo_cmd() {
  if [[ $DRY -eq 1 ]]; then echo "  [dry-run] sudo $*"; return 0; fi
  if [[ -t 0 ]]; then sudo -p '' "$@"; else sudo -S -p '' "$@"; fi
}

# ---- 1. close OBS + stray backend ------------------------------------------
step "Stopping OBS and backend (if running)"
if pgrep -x obs >/dev/null 2>&1; then
  echo "OBS is running — closing it..."
  if [[ $DRY -eq 0 ]]; then
    pkill -x obs || true
    for _ in $(seq 1 20); do pgrep -x obs >/dev/null 2>&1 || break; sleep 0.5; done
    pgrep -x obs >/dev/null 2>&1 && { echo "⚠ OBS would not close — aborting"; exit 1; }
  fi
else
  echo "OBS not running."
fi
# backend normally exits with OBS (PDEATHSIG); kill leftovers of manual starts
run pkill -f "omniversify.*/backend/main\.py" || true

# ---- 2. pacman package ------------------------------------------------------
step "Removing pacman package (if installed)"
if pacman -Q "$PKG" >/dev/null 2>&1; then
  echo "found: $(pacman -Q "$PKG")"
  sudo_cmd pacman -R --noconfirm "$PKG"
else
  echo "not installed as a package."
fi

# ---- 3. leftover / dev-install files ---------------------------------------
step "Removing installed files"
if [[ -e "$SO" ]]; then
  if pacman -Qo "$SO" >/dev/null 2>&1; then
    echo "  $SO — still owned by a package (should be gone after step 2)"
  else
    echo "  $SO (dev install via build.sh)"
    run sudo rm -f "$SO"
  fi
else
  echo "  no $SO"
fi
if [[ -d "$SHARE" ]]; then
  echo "  $SHARE/"
  run sudo rm -rf "$SHARE"
else
  echo "  no $SHARE/"
fi

# old pre-plugin user install (dead weight, was already renamed to .bak)
found_user=0
for d in $USER_PLUGIN_GLOB; do
  [[ -e "$d" ]] || continue
  found_user=1
  echo "  $d (old user install)"
  run rm -rf "$d"
done
[[ $found_user -eq 0 ]] && echo "  no user-level plugin dirs"

# ---- 4. optional purge ------------------------------------------------------
if [[ $PURGE -eq 1 ]]; then
  step "Purging config and voices"
  [[ -e "$CONF" ]] && { echo "  $CONF"; run rm -f "$CONF"; }
  [[ -d "$DATA_DIR" ]] && { echo "  $DATA_DIR/ (voices + voices.json)"; run rm -rf "$DATA_DIR"; }
  [[ -d "$CONF_DIR" ]] && run rmdir "$CONF_DIR" 2>/dev/null || true
else
  step "Keeping config + voices (--purge to remove)"
  echo "  $CONF"
  echo "  $DATA_DIR/ ($(du -sh "$DATA_DIR" 2>/dev/null | cut -f1 || echo '?'))"
fi

# ---- 5. verify --------------------------------------------------------------
if [[ $DRY -eq 1 ]]; then
  step "Verification (skipped — dry run, nothing was removed)"
  echo
  echo "(dry run — nothing was changed)"
  exit 0
fi
step "Verification"
leftovers=0
[[ -e "$SO" ]] && { echo "  STILL PRESENT: $SO"; leftovers=1; }
[[ -d "$SHARE" ]] && { echo "  STILL PRESENT: $SHARE"; leftovers=1; }
pacman -Q "$PKG" >/dev/null 2>&1 && { echo "  STILL INSTALLED: package"; leftovers=1; }
for d in $USER_PLUGIN_GLOB; do
  [[ -e "$d" ]] && { echo "  STILL PRESENT: $d"; leftovers=1; }
done
[[ $leftovers -eq 0 ]] && echo "  ✅ plugin fully removed from the system"

echo
echo "Reinstall for testing:"
echo "  ./build.sh                                  # dev build"
echo "  sudo pacman -U packaging/aur/$PKG-*.pkg.tar.zst   # packaged"
if [[ $PURGE -eq 1 ]]; then
  echo "Fresh-install test: first OBS start downloads the default voice (~61MB)."
else
  echo "Voices kept — first OBS start skips the download."
fi
