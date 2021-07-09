#!/bin/bash

if [ -z "$GITDIR" ]; then
	echo "Please specify the GITDIR environment variable."
	return 1
else
	cd "${GITDIR}/toolchain/gcc-4.8/bin/"
	export PATH="${PATH}:${PWD}"
	cd -
	cd "${GITDIR}/toolchain/armv7l-linux-musleabihf-cross/bin"
	export PATH="${PATH}:${PWD}"
	cd -
	cd "${GITDIR}/toolchain/arm-nickel-linux-gnueabihf/bin"
	export PATH="${PATH}:${PWD}"
	cd -

	echo "Environment set up."
fi
