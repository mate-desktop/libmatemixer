#!/usr/bin/bash

set -eo pipefail

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Archlinux
requires=(
	ccache # Use ccache to speed up build
	clang  # Build with clang on Archlinux
)

# https://gitlab.archlinux.org/archlinux/packaging/packages/libmatemixer
requires+=(
	alsa-lib
	gcc
	git
	glib2
	glibc
	gtk-doc
	libpulse
	make
	mate-common
	systemd-libs
	which
)

infobegin "Update system"
pacman --noconfirm -Syu
infoend

infobegin "Install dependency packages"
pacman --noconfirm -S ${requires[@]}
infoend
