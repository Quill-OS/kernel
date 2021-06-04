#!/bin/sh

make_nodes() {
	mkdir -p dev
	sudo mknod c 5 1 dev/console
	sudo mknod c 207 16 dev/ttymxc0
	sudo mknod c 1 3 dev/null
}

cd ${GITDIR}/initrd/n705
make_nodes
cd ${GITDIR}/initrd/n905c
make_nodes
cd ${GITDIR}/initrd/n705-diags
make_nodes
cd ${GITDIR}/initrd/n905c-diags
make_nodes
echo "Done making devices nodes."
