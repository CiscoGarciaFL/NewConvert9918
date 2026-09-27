#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 4 ]]; then
  echo "usage: $0 <deployed-root> <deb-root> <version> <output.deb>" >&2
  exit 2
fi

workspace="$(pwd -P)"
deployed_root="$(realpath "$1")"
deb_root="$(realpath -m "$2")"
release_version="$3"
output_deb="$(realpath -m "$4")"

case "${deb_root}" in
  "${workspace}"/*) ;;
  *)
    echo "Debian staging root must be inside ${workspace}: ${deb_root}" >&2
    exit 2
    ;;
esac
if [[ "${deb_root}" == "${workspace}" ]]; then
  echo "Refusing to use the workspace itself as the Debian staging root." >&2
  exit 2
fi

for required in \
  bin/NewConvert9918 \
  bin/newconvert9918-cli \
  bin/qt.conf \
  lib/libQt6Core.so.6 \
  plugins/platforms/libqxcb.so \
  share/applications/io.github.ciscogarciafl.NewConvert9918.desktop \
  share/icons/hicolor/256x256/apps/io.github.ciscogarciafl.NewConvert9918.png \
  LICENSE NOTICE.md THIRD_PARTY_NOTICES.md \
  LICENSES/LGPL-3.0.txt LICENSES/GPL-3.0.txt; do
  if [[ ! -e "${deployed_root}/${required}" ]]; then
    echo "Deployed runtime is missing ${required}" >&2
    exit 1
  fi
done

rm -rf -- "${deb_root}"
install -d \
  "${deb_root}/DEBIAN" \
  "${deb_root}/opt/newconvert9918" \
  "${deb_root}/usr/bin" \
  "${deb_root}/usr/share/applications" \
  "${deb_root}/usr/share/icons" \
  "${deb_root}/usr/share/doc/newconvert9918"

# linuxdeploy's self-contained tree is safe only when it remains private to
# the application. Its qt.conf, libraries, plugins, and QML modules must never
# be installed into global /usr paths.
cp -a "${deployed_root}/." "${deb_root}/opt/newconvert9918/"
rm -rf -- \
  "${deb_root}/opt/newconvert9918/share/applications" \
  "${deb_root}/opt/newconvert9918/share/doc" \
  "${deb_root}/opt/newconvert9918/share/icons"

install -m 0755 packaging/linux/newconvert9918-gui \
  "${deb_root}/usr/bin/NewConvert9918"
install -m 0755 packaging/linux/newconvert9918-cli \
  "${deb_root}/usr/bin/newconvert9918-cli"
install -m 0644 \
  "${deployed_root}/share/applications/io.github.ciscogarciafl.NewConvert9918.desktop" \
  "${deb_root}/usr/share/applications/io.github.ciscogarciafl.NewConvert9918.desktop"
cp -a "${deployed_root}/share/icons/hicolor" \
  "${deb_root}/usr/share/icons/"

install -m 0644 "${deployed_root}/LICENSE" \
  "${deb_root}/usr/share/doc/newconvert9918/LICENSE"
install -m 0644 "${deployed_root}/NOTICE.md" \
  "${deb_root}/usr/share/doc/newconvert9918/NOTICE.md"
install -m 0644 "${deployed_root}/THIRD_PARTY_NOTICES.md" \
  "${deb_root}/usr/share/doc/newconvert9918/THIRD_PARTY_NOTICES.md"
cp -a "${deployed_root}/LICENSES" \
  "${deb_root}/usr/share/doc/newconvert9918/"

if [[ "${release_version}" == *-* ]]; then
  version_core="${release_version%%-*}"
  version_suffix="${release_version#*-}"
  deb_version="${version_core}~${version_suffix}"
else
  deb_version="${release_version}"
fi
installed_size="$(du -sk "${deb_root}" | awk '{print $1}')"
if [[ ! "${installed_size}" =~ ^[1-9][0-9]*$ ]]; then
  echo "Unable to determine a positive Installed-Size." >&2
  exit 1
fi

sed \
  -e "s|@VERSION@|${deb_version}|g" \
  -e "s|@INSTALLED_SIZE@|${installed_size}|g" \
  packaging/linux/control.in > "${deb_root}/DEBIAN/control"
chmod 0644 "${deb_root}/DEBIAN/control"

mkdir -p "$(dirname "${output_deb}")"
dpkg-deb --root-owner-group --build "${deb_root}" "${output_deb}"
chmod 0644 "${output_deb}"
