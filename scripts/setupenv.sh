#!/bin/bash

if [ -z "${GITDIR}" ]; then
	echo "Please specify the GITDIR environment variable."
	return 1
else
	pushd "${GITDIR}/toolchain/gcc-4.8/bin/"
	export PATH="${PATH}:${PWD}"
	popd
	pushd "${GITDIR}/toolchain/armv7l-linux-musleabihf-cross/bin"
	export PATH="${PATH}:${PWD}"
	popd
	pushd "${GITDIR}/toolchain/arm-nickel-linux-gnueabihf/bin"
	export PATH="${PATH}:${PWD}"
	popd
	pushd "${GITDIR}/toolchain/arm-kobo-linux-gnueabihf/bin"
	export PATH="${PATH}:${PWD}"
	popd

	echo "Environment set up."
fi
