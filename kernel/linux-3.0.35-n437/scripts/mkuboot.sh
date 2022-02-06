#!/bin/bash

#
# Build U-Boot image when `mkimage' tool is available.
#

MKIMAGE=$(type -path "${CROSS_COMPILE}mkimage")

if [ -z "${MKIMAGE}" ]; then
	MKIMAGE=$(type -path mkimage)
	if [ -z "${MKIMAGE}" ]; then
		# Doesn't exist
		echo '"mkimage" command not found - U-Boot images will not be built' >&2
		exit 1;
	fi
fi

SVNREV="$(LANG=C svn info|grep "Last Changed Rev:"|awk '{print $4}')"

if [ "${SVNREV}" ];then
	STATUS="$(svn st|grep "^[!ADCM]"|grep -v ".version"|grep -v "arch/arm/boot/uImage"|grep -v "include/generated/compile.h")"
	if [ "${STATUS}" ];then
		KREV="r${SVNREV}M"
	else
		KREV="r${SVNREV}"
	fi
else
	GITREV="$(git rev-list HEAD -1 |head -c 6)"
	if [ "$(git status |grep "^#[[:blank:]]*modified"|grep -v ".version$"|grep -v "arch/arm/boot/uImage$"|grep -v "include/generated/compile.h$")" ] || 
		[ "$(git status |grep "^#[[:blank:]]*new file")" ] ||
		[ "$(git status |grep "^#[[:blank:]]*deleted")" ]
	then
		KREV="${GITREV}M"
	else
		KREV="${GITREV}"
	fi
fi

if [ -z "${KREV}" ];then
	KREV="?"
fi

UTS_VER="$(cat include/generated/compile.h |grep "UTS_VERSION"|awk -F\" '{print $2}'|awk '{print $1,$4,$5,$6}')"
if [ -z "${UTS_VER}" ];then
	UTS_VER="?"
fi


# Call "mkimage" to create U-Boot image
#${MKIMAGE} "$@"
${MKIMAGE} -n "${KREV}_${UTS_VER}" "$@"

