#!/bin/bash

if [ "$GITDIR" == "" ]; then
	echo "You must specify the path of the Git repository in the GITDIR environment variable."
	exit 1
elif [ "$TOOLCHAINDIR" == "" ]; then
	echo "You must specify the path of the toolchain in the TOOLCHAINDIR environment variable."
	echo "Usually, a good location for it would be in $GITDIR/toolchain"
	echo "Make sure that from TOOLCHAINDIR, there's a 'bin' folder in the first level that gives executables such as arm-linux-gnueabihf-gcc"
	exit 1
elif [ "$TARGET" == "" ]; then
	echo "You must specify a target in the TARGET environment variable."
	echo "Usually, it's something like 'arm-linux-gnueabihf', 'arm-kobo-linux-gnuabihf' or 'arm-nickel-linux-gnueabihf'."
	exit 1
elif [ "$THREADS" == "" ]; then
	echo "---- Warning: no THREADS environment variable. The kernel will be built with -j1 ----"
	THREADS=1
fi

$GITDIR/scripts/build_kernel.sh n705 std
$GITDIR/scripts/build_kernel.sh n705 root
$GITDIR/scripts/build_kernel.sh n905c std
$GITDIR/scripts/build_kernel.sh n905c root
