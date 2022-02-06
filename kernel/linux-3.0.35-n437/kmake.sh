#!/bin/bash
function usage() {
	cat >&2 <<- EOF
	******************************************************
	make tool for kernel source .

	Usage: ${0} [OPTIONS] [PLATFORM] [TARGET]
	
	OPTIONS: 
	 -h: this help message .
	
	PLATFORM:
	 target platform : [mx35|mx35-linux|x86|m166e|mx50|mx50-linux|mx6|mx6-linux]

	TARGET:
	 make target of kernel source .

	---------------------
	example:
	 
	  ${0} mx35 
	  ${0} mx35 clean
	  ${0} mx35 menuconfig
	  ${0} mx50 uImage
	  ${0} mx50 menuconfig
	  ${0} mx50 modules
	  ${0} mx50 modules_install

	******************************************************

	EOF
}

. kfunc.sh

WORK_DIR="$(pwd)"
PLATFORM_NAME="${1}"
MAKE_TARGET="${2}"

platform_arch=""
platform_cross=""


_tmp_file="/tmp/${USER}_tmp"

echo -n "" > "${_tmp_file}"
while getopts ":h" opt
do
	case ${opt} in
		h ) 
			usage 
			exit 0 ;;
		* )
			echo -n "${opt} ${OPTARG} " >> "${_tmp_file}"
			;;
	esac
done
shift "$(expr ${OPTIND} - 1)"


setup_platform "${PLATFORM_NAME}"

#obj_dir="$(get_obj_dir)"
#if [ "${obj_dir}" ];then
#	O_OPT="O=${obj_dir}"
#fi
#make ${O_OPT} ARCH="${platform_arch}" CROSS_COMPILE="${platform_cross}" 


if [ "${MAKE_TARGET}" = "modules_install" ];then
	MOD_DIR="${WORK_DIR}/_root_modules"
	OUT_MOD_DIR="${WORK_DIR}/root_modules"
	if [ ! -d "${MOD_DIR}" ];then
		mkdir -p "${MOD_DIR}"
	fi
	INSTALL_MOD_PATH_OPT="INSTALL_MOD_PATH=${MOD_DIR}"
fi



OTHER_MAKE_OPTIONS="$(cat "${_tmp_file}")"
#echo "make ${OTHER_MAKE_OPTIONS} ARCH="${platform_arch}" CROSS_COMPILE="${platform_cross}" ${INSTALL_MOD_PATH_OPT} ${MAKE_TARGET} |tee make.log"
echo "===================================="
echo "PLATFORM=${PLATFORM_NAME}"
echo "CROSS_COMPILE=${platform_cross}"
echo "ARCH=${platform_arch}"
echo "TARGET=${MAKE_TARGET}"
echo "===================================="

if [ "${MAKE_TARGET}" = "menuconfig" ];then
	make ${OTHER_MAKE_OPTIONS} ARCH="${platform_arch}" CROSS_COMPILE="${platform_cross}" ${INSTALL_MOD_PATH_OPT} ${MAKE_TARGET} |tee make.log
else
	make ${OTHER_MAKE_OPTIONS} ARCH="${platform_arch}" CROSS_COMPILE="${platform_cross}" ${INSTALL_MOD_PATH_OPT} ${MAKE_TARGET} 2>&1|tee make.log
fi

if [ "${MAKE_TARGET}" = "modules_install" ];then
	
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

fi



