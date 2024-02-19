#!/bin/bash

cleanup() {
	rm -f ${TMP_PREFIX}*
	return 0
}

usage() {

	cat >&2 <<- EOF
	**************************************************************************
	release kernel : help you to release kernel source tree .

	Usage: ${0} [Options] [URL_FROM] [URL_TO_PATH] 
	
	
	[URL_FROM] : SVN URL path which you want to release to 
	  (folder with revision & timestamp will be created automatically).
		default is "${URL_RELEASE_PATH}"

	[URL_TO_PATH] : SVN URL which you want to release from .
		default is "${URL_RELEASE_FROM}"


	[Options]:
	 -r <revision> : release revision of [URL_FROM] .

	example:

	 release kernel source revision HEAD from "http://ebkrd2sw-desktop/svn/mx50/trunk/ltib/rpm/linux-2.6.35.3" to 
	   "http://ebkrd2sw-desktop/svn/mx50/tags/kernel/release" :
	  ${0} "http://ebkrd2sw-desktop/svn/mx50/trunk/ltib/rpm/linux-2.6.35.3" "http://ebkrd2sw-desktop/svn/mx50/tags/kernel/release"

	 release kernel source revision 1234 from "http://ebkrd2sw-desktop/svn/mx50/trunk/ltib/rpm/linux-2.6.35.3" to 
	   "http://ebkrd2sw-desktop/svn/mx50/tags/kernel/release" :
	  ${0} -r 1234 "http://ebkrd2sw-desktop/svn/mx50/trunk/ltib/rpm/linux-2.6.35.3" "http://ebkrd2sw-desktop/svn/mx50/tags/kernel/release"

	 release kernel source from working folder to "http://ebkrd2sw-desktop/svn/mx50/tags/kernel/release" :
	  ${0} . "http://ebkrd2sw-desktop/svn/mx50/tags/kernel/release"


	**************************************************************************
	EOF
	return 0
}

_GetURL_SVN_MRev() {
	URL="${1}"
	REV="${2}"
	if [ -z "${REV}" ];then
		REV="HEAD"
	fi
	if [ "$(echo "${URL}"|grep "^.*://")" ];then
		querry_local="0"
	else
		querry_local="1"
	fi

	if [ "${querry_local}" = "1" ];then
		REVISION="$(LANG=C svn info "${URL}"|grep "Last\ Changed\ Rev"|sed -e "s/Last\ Changed\ Rev:\ //")"
	else
		REVISION="$(LANG=C svn info -r "${REV}" "${URL}"|grep "Last\ Changed\ Rev"|sed -e "s/Last\ Changed\ Rev:\ //")"
	fi
	echo "${REVISION}"
	return 0
}


load_kernel_info() {
	echo "loading kernel information .... "

	KRev_file="$1"
	
	if [ -f "${KRev_file}" ];then
		echo -n ""
	else
		echo "\"${KRev_file}\" cannot found !"
		return 1
	fi

	_kernel_url="$(cat "${KRev_file}" |grep "^URL:"|sed -e "s/URL:[[:blank:]]*//")"
	#_kernel_rev="$(cat "${KRev_file}" |grep "^Revision:"|sed -e "s/Revision:[[:blank:]]*//")"
	_kernel_rev="$(cat "${KRev_file}" |grep "^Last Changed Rev:"|sed -e "s/Last Changed Rev:[[:blank:]]*//")"
	echo "Kernel URL:\"${_kernel_url}\""
	echo "Kernel REV:${_kernel_rev}"

	_kernel_root_URL="$(echo "${_kernel_url}"|sed -e "s/\/[[:alnum:]]*\/[[:alnum:]]*\/[[:alnum:]]*\/[[:alnum:]]*$//")"
	

	#if [ "${HWCFG_CPU_NAME}" = "m166e" ] && [ "${HWCFG_CPU_NAME}" = "mx35" ];then
	#	_kernel_compile_h_URL="${_kernel_root_URL}/include/linux/compile.h"
	#else
		_kernel_compile_h_URL="${_kernel_root_URL}/include/generated/compile.h"
	#fi

	echo "_kernel_compile_h_URL=\"${_kernel_compile_h_URL}\""
	if [ "${_kernel_rev}" ] && [ "${_kernel_compile_h_URL}" ];then
		svn export -r "${_kernel_rev}" "${_kernel_compile_h_URL}" "/tmp/sdmk_${ID_STRING}_kcompile.h"
		KERNEL_INFO_STRING="r${_kernel_rev} ($(cat "/tmp/sdmk_${ID_STRING}_kcompile.h"|grep "^#define UTS_VERSION"|sed -e "s/^#define\ UTS_VERSION //"|sed -e "s/\"//g"))"
		echo "kernel info=\"${KERNEL_INFO_STRING}\""

		rm -f "/tmp/sdmk_${ID_STRING}_kcompile.h"
	fi

	return 0
}






PURPOSE="release"

LAST_RELEASE_INFO=".last_${PURPOSE}" # last release url ,you can cat this file ,it is a string .
URL_BASE="http://ebkrd2sw-desktop/svn/mx50"
#URL_BASE="file:///work/test/svn/testrepo"
TIMESTAMP_STR="$(date +%Y%m%d_%H%M%S)" # release name : the name you want to release into URL path .
ID_STRING="${TIMESTAMP_STR}"
TMP_PREFIX=".tmp-${TIMESTAMP_STR}"
COMMIT_MSG_FILE="${TMP_PREFIX}-commit_msg"
REL_FROM_SVNINFO="${TMP_PREFIX}-svninfo"


URL_RELEASE_PATH="${URL_BASE}/tags/kernel/${PURPOSE}" # URL path : where you want to release from your working folder .
URL_RELEASE_FROM="${URL_BASE}/trunk/ltib/rpm/BUILD/linux-2.6.35.3" # URL path : where you want to release from your working folder .
#URL_RELEASE_FROM="${URL_BASE}/trunk" # URL path : where you want to release from your working folder .


REV_RELEASE_FROM="$(_GetURL_SVN_MRev "${URL_RELEASE_FROM}")"
while getopts "r:" opt
do
	case ${opt} in
		r )
			REV_RELEASE_FROM="${OPTARG}" 
			REV_OPTION="-r ${REV_RELEASE_FROM}"
			;;
		\? ) 
			unknown_opt=1
			;;
	esac
done
shift "$(expr ${OPTIND} - 1)"

if [ "$unknown_opt" = 1 ];then
	usage
	exit 0
fi

if [ "$2" ];then
	URL_RELEASE_PATH="$2"
fi

if [ "$1" ];then
	URL_RELEASE_FROM="$1"
fi


LANG=C svn info ${REV_OPTION} "${URL_RELEASE_FROM}" > "${REL_FROM_SVNINFO}"
load_kernel_info "${REL_FROM_SVNINFO}"
#if [ ! -f "${LAST_RELEASE_INFO}" ];then
#	echo "\"${LAST_RELEASE_INFO}\" do not exist !!"
#	cleanup ; exit 1
#fi

if [ -z "$(LANG=C svn info ${REV_OPTION} "${URL_RELEASE_PATH}")" ];then
	echo "svn path \"${URL_RELEASE_PATH}\" do not exist , please create it first !"
	cleanup ; exit 2
fi

#if [ -z "$(LANG=C svn info "${URL_RELEASE_PATH}/${LAST_RELEASE_INFO}")" ];then
#	echo "svn path \"${URL_RELEASE_PATH}\${LAST_RELEASE_INFO}" do not exist , please create it first !"
#	cleanup ; exit 3
#fi
if [ -z "$(echo "${URL_RELEASE_FROM}"|grep "^.*://")" ] && [ "$(LANG=C svn st "${URL_RELEASE_FROM}"|grep "^[MA!~C]")" ];then
	# local working folder with local modification .
	URL_RELEASE_NAME="r${REV_RELEASE_FROM}_M${TIMESTAMP_STR}" # release name : the name you want to release into URL path .
else
	URL_RELEASE_NAME="r${REV_RELEASE_FROM}_${TIMESTAMP_STR}" # release name : the name you want to release into URL path .
fi
input="${TMP_PREFIX}-edboxdefault"

cat << EOF > $input
[Purpose]
 
[Author]
 
[Test Report]
 ---- test device information ------
 model name : N/A
 sdmaker model no : N/A
 sdmaker model revision : N/A
 tester : N/A
 ---------- basic test items -----------
 boot into rootfs : N/A
 insmod Wifi : N/A
 insmod USB Gadget : N/A
 Suspend/Resume : N/A
 EPD logo : N/A
 EPD ebrmain main menu : N/A
 EPD QT : N/A
 Power Off : N/A
 --------- special test items ----------


[Release information]
 Release kernel source and binary by "$0" : 
 From : ${URL_RELEASE_FROM}
 To : ${URL_RELEASE_PATH}/${URL_RELEASE_NAME}
 KernelInfo : ${KERNEL_INFO_STRING}

[SVN info of ${URL_RELEASE_FROM}]
$(cat "${REL_FROM_SVNINFO}")

EOF

LANG=C dialog --stdout --ascii-lines --title "svn commit :" \
	--fixed-font --editbox ${input} 0 0 > "${COMMIT_MSG_FILE}"

case $? in
  0)
    echo "OK"

		# commit current working folder status into tag ...
		if [ -z "${REV_RELEASE_FROM}" ];then
			echo "there is no revision infomation @ ${URL_RELEASE_FROM} ..."
			cleanup ; exit 5
		fi

		LANG=C svn cp ${REV_OPTION} -F "${COMMIT_MSG_FILE}" "${URL_RELEASE_FROM}" "${URL_RELEASE_PATH}/${URL_RELEASE_NAME}"
		if [ $? != 0 ];then
			echo "svn tag making fail !"
			cleanup ; exit 6
		fi


		# commit into trunk ...
		echo "RELEASE_URL=${URL_RELEASE_PATH}/${URL_RELEASE_NAME}" > "${LAST_RELEASE_INFO}"
		echo "RELEASE_FROM_URL=${URL_RELEASE_FROM}" >> "${LAST_RELEASE_INFO}"
		echo "RELEASE_FROM_REV=${REV_RELEASE_FROM}" >> "${LAST_RELEASE_INFO}"
		if [ -z "$(svn st "${LAST_RELEASE_INFO}" | grep "^?")" ];then
			svn add "${LAST_RELEASE_INFO}"
		fi

		if [ -z "$(cat "${LAST_RELEASE_INFO}"|grep "RELEASE_FROM_REV")" ] || [ "${REV_RELEASE_FROM}" -gt "$(cat "${LAST_RELEASE_INFO}"|grep "RELEASE_FROM_REV"|sed -e "s/RELEASE_FROM_REV=//")" ];then
			# if there is no RELEASE_FROM_REV field in last release info .
			LANG=C svn ci -F "${COMMIT_MSG_FILE}" "${LAST_RELEASE_INFO}"
		fi

    ;;
  1)
    echo "Button 1 (Cancel) pressed"
		cleanup ; exit 4
		;;
  2)
    echo "Button 2 (Help) pressed"
		cleanup ; exit 4
		;;
  3)
    echo "Button 3 (Extra) pressed"
		cleanup ; exit 4
		;;
  255)
    echo "ESC pressed."
		cleanup ; exit 4
		;;
esac

cleanup ;exit 0

