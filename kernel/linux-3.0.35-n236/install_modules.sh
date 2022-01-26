#!/bin/bash


WORK_DIR="$(pwd)"
MOD_DIR="${WORK_DIR}/_root_modules"
OUT_MOD_DIR="${WORK_DIR}/root_modules"
if [ ! -d "${MOD_DIR}" ];then
	mkdir -p "${MOD_DIR}"
fi
INSTALL_MOD_PATH_OPT="INSTALL_MOD_PATH=${MOD_DIR}"
make ${INSTALL_MOD_PATH_OPT} modules_install

if [ -d "${OUT_MOD_DIR}" ];then
	pushd "${OUT_MOD_DIR}"

	find -type f |grep -v \.svn |
	while read fname
	do
		rm "${fname}"
	done

	popd

	pushd "${MOD_DIR}"
	cp -a * "${OUT_MOD_DIR}/"
	popd

	rm -fr "${MOD_DIR}"
else
	mv "${MOD_DIR}" "${OUT_MOD_DIR}"
fi

exit 0

