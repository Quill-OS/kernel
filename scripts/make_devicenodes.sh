#!/bin/sh

if [ -z "${GITDIR}" ]; then
	echo "You must specify the GITDIR environment variable."
	exit 1
else
	make_nodes() {
		mkdir -p dev
		sudo mknod dev/console c 5 1
		sudo mknod dev/ttymxc0 c 207 16
		sudo mknod dev/null c 1 3
		if [ "${1}" == "emu" ]; then
			sudo mknod dev/ttyAMA0 c 204 64
		fi
	}

	cd ${GITDIR}/initrd/n705
	make_nodes 2>/dev/null
	cd ${GITDIR}/initrd/n905c
	make_nodes 2>/dev/null
	cd ${GITDIR}/initrd/n613
	make_nodes 2>/dev/null
	cd ${GITDIR}/initrd/n705-diags
	make_nodes 2>/dev/null
	cd ${GITDIR}/initrd/n905c-diags
	make_nodes 2>/dev/null
	cd ${GITDIR}/initrd/n613-diags
	make_nodes 2>/dev/null
	cd ${GITDIR}/initrd/n873-spl
	make_nodes 2>/dev/null
	cd ${GITDIR}/initrd/n873
	make_nodes 2>/dev/null
	cd ${GITDIR}/initrd/n873-diags
	make_nodes 2>/dev/null
	cd ${GITDIR}/initrd/n905b
	make_nodes 2>/dev/null
	cd ${GITDIR}/initrd/n905b-diags
	make_nodes 2>/dev/null
	cd ${GITDIR}/initrd/emu
	make_nodes emu 2>/dev/null
	cd ${GITDIR}/initrd/emu-diags
	make_nodes emu 2>/dev/null
	echo "Done making devices nodes."
fi
