#!/usr/bin/env bash
set -euo pipefail
source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="${1:-$source_dir/build/release}"
build_dir="$(cd "$build_dir" && pwd)"
app_dir="$build_dir/AppDir"
tools_dir="$build_dir/appimage-tools"
mkdir -p "$tools_dir" "$source_dir/dist"
if [[ "$(uname -m)" != "x86_64" ]]; then
  echo "Ce script AppImage cible x86_64. Utilisez les sources pour les autres architectures." >&2
  exit 1
fi
curl --fail --location --retry 3 https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage -o "$tools_dir/linuxdeploy-x86_64.AppImage"
curl --fail --location --retry 3 https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage -o "$tools_dir/linuxdeploy-plugin-qt-x86_64.AppImage"
chmod +x "$tools_dir/"*.AppImage
if [[ -d "$app_dir" ]]; then rm -rf -- "$app_dir"; fi
DESTDIR="$app_dir" cmake --install "$build_dir" --prefix /usr
install -m755 "$source_dir/packaging/linux/AppRun" "$app_dir/AppRun"
export QMAKE="${QMAKE:-$(command -v qmake6)}"
# Keep optional desktop image extensions (for example KDE's kimg_* plugins)
# out of the bundle. Qt's own image plugins are sufficient for this client.
export GHE_REAL_QMAKE="$QMAKE"
export GHE_PLUGIN_DIR="$tools_dir/qt-plugins"
plugin_source="$("$QMAKE" -query QT_INSTALL_PLUGINS)"
mkdir -p "$GHE_PLUGIN_DIR/imageformats"
for entry in "$plugin_source/"*; do
  [[ "$(basename "$entry")" == imageformats ]] && continue
  ln -sfn "$entry" "$GHE_PLUGIN_DIR/$(basename "$entry")"
done
for entry in "$plugin_source/imageformats/"libq*.so; do
  [[ -f "$entry" ]] && ln -sfn "$entry" "$GHE_PLUGIN_DIR/imageformats/$(basename "$entry")"
done
cat > "$tools_dir/qmake-filtered" <<'WRAPPER'
#!/usr/bin/env bash
set -euo pipefail
if [[ "${1:-}" == -query && "${2:-}" == QT_INSTALL_PLUGINS ]]; then
  printf '%s\n' "$GHE_PLUGIN_DIR"
else
  "$GHE_REAL_QMAKE" "$@" | awk '/^QT_INSTALL_PLUGINS:/ { print "QT_INSTALL_PLUGINS:" ENVIRON["GHE_PLUGIN_DIR"]; next } { print }'
fi
WRAPPER
chmod +x "$tools_dir/qmake-filtered"
export QMAKE="$tools_dir/qmake-filtered"
export EXTRA_PLATFORM_PLUGINS="${EXTRA_PLATFORM_PLUGINS:-libqoffscreen.so}"
for entry in "$plugin_source/platforms/"libqwayland*.so; do
  [[ -f "$entry" ]] && EXTRA_PLATFORM_PLUGINS+=";$(basename "$entry")"
done
export APPIMAGE_EXTRACT_AND_RUN=1
# The bundled strip utility may predate modern ELF RELR sections.
export NO_STRIP=1
export PATH="$tools_dir:$PATH"
export OUTPUT="$source_dir/dist/.GitHub-Enhanced-x86_64.new.AppImage"
# Qt loads OpenSSL dynamically: linuxdeploy cannot discover libssl through
# ELF dependencies. Bundle the matching pair to avoid mixing bundled crypto
# with the host's (potentially newer) SSL library.
openssl_libraries=()
for library in libssl.so.3 libcrypto.so.3; do
  library_path="$("${CXX:-c++}" -print-file-name="$library")"
  if [[ ! -f "$library_path" ]]; then
    echo "Bibliothèque OpenSSL introuvable : $library" >&2
    exit 1
  fi
  openssl_libraries+=(--library "$library_path")
done
"$tools_dir/linuxdeploy-x86_64.AppImage" --appdir "$app_dir" "${openssl_libraries[@]}" --plugin qt --output appimage
for library in libssl.so.3 libcrypto.so.3; do
  [[ -f "$app_dir/usr/lib/$library" ]] || { echo "Bibliothèque absente du paquet : $library" >&2; exit 1; }
done

# Remplacer le fichier atomiquement, même si une ancienne version est montée.
mv -f -- "$OUTPUT" "$source_dir/dist/GitHub-Enhanced-x86_64.AppImage"
