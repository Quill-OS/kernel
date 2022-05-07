#!/bin/bash
#
# Script to apply the kernel patches in the order specified in the series file.
#

#
# How to do it:
#
# First untar the kernel source downloaded from kernel.org:
# tar xjf linux-[version].bz2
#
# Cd to the base of the linux source tree:
# cd linux-[version]
#
# Then untar the patches package that is provided in the release:
# tar xjf ../linux-[version]-[release].bz2
#
# And run this script from the base of the linux source tree:
# ./patches/patch-kernel.sh
#

ls patches/*.patch | while read i; do
	echo "--- applying $i"
	patch -p1 --no-backup-if-mismatch < "$i"
	echo
done

if [ -f patches/localversion ]; then
	cp -f patches/localversion .
fi
