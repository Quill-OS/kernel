#!/bin/bash
make distclean

IDSTR="uboot-build-$(date +%Y%m%d_%H%M%S)"
tmpresult="/tmp/${IDSTR}.tmp"


#MODELS=("E60610D" "E50610" "E606B0" "E606A0" "E60612")
#RAMTYPES=("C" "D")


########################
##### GLOBAL #######

PCBA=""
RAMTYPE=""

########################


select_uboot() {
	LANG=C dialog --stdout --ascii-lines --title "mx50 u-boot select(PCBA of MDDR 256MB)" --menu "uboot PCBA select" 10 50 8 \
		"E60610" "PCBA=E60610" \
		"E50610" "PCBA=E50610" \
		"E60610D" "PCBA=E60610D" \
		"E606A0" "PCBA=E606A0" \
		"E606B0" "PCBA=E606B0" >"${tmpresult}2"
	retval=$?
	if [ "${retval}" = "0" ];then
		LANG=C dialog --stdout --ascii-lines --title "mx50 u-boot select(RAMTYPE of MDDR 256MB)" --menu "uboot RAMTYPE select" 10 50 4 \
			"K4X2G323PC" "RAM C-Die" \
			"K4X2G323PD" "RAM D-Die" >"${tmpresult}3"
		retval=$?
		if [ "${retval}" = "0" ];then
			PCBA="$(cat "${tmpresult}2")"
			RAMTYPE="$(cat "${tmpresult}3")"
		fi
	fi
	return 0
}


prepare_flash_header_S() {
	errorno=0

	select_uboot

	echo "PCBA=${PCBA}"
	echo "RAMTYPE=${RAMTYPE}"

	cd board/freescale/mx50_rdp

	if [ "${PCBA}" = "E60612" ];then
			rm flash_header.S;sync;sleep 1
			ln -s flash_header-NTX_RAM_PARMS_DSorg.S flash_header.S
			errorno=$?
	elif [ "${PCBA}" = "E50610" ];then
			rm flash_header.S;sync;sleep 1
			ln -s flash_header-20120622_FSL_RAM_PARMS_DSadd2.S flash_header.S
			errorno=$?
	elif [ "${PCBA}" = "E60610D" ] || [ "${PCBA}" = "E606A0" ];then
			rm flash_header.S;sync;sleep 1
			ln -s flash_header-20120622_FSL_RAM_PARMS_DSadd2.S flash_header.S
			#if [ "${RAMTYPE}" = "K4X2G323PC" ];then
			#	rm flash_header.S
			#	ln -s flash_header-20120622_FSL_RAM_PARMS_DSadd2.S flash_header.S
			#elif [ "${RAMTYPE}" = "K4X2G323PD" ];then
			#	rm flash_header.S
			#	ln -s flash_header-NTX_RAM_PARMS.S flash_header.S
			#else
			#	echo "Wrong RAMTYPE !!"
			#	errorno=1
			#fi
			errorno=$?
	elif [ "${PCBA}" = "E606B0" ];then
		rm flash_header.S;sync;sleep 1
		ln -s flash_header-20120622_FSL_RAM_PARMS_DSadd2.S flash_header.S
		errorno=$?
	else
			#ln -s flash_header-20120622_FSL_RAM_PARMS_DSadd1.S flash_header.S
			echo "Wrong PCBA !!"
			errorno=2
	fi

	cd -
	return ${errorno}
}


prepare_flash_header_S
[ $? != 0 ] && echo "prepare flash header fail !!" && exit 1


make ARCH=arm CROSS_COMPILE=/opt/freescale/usr/local/gcc-4.4.4-glibc-2.11.1-multilib-1.0/arm-fsl-linux-gnueabi/bin/arm-fsl-linux-gnueabi- mx50_rdp_config
make ARCH=arm CROSS_COMPILE=/opt/freescale/usr/local/gcc-4.4.4-glibc-2.11.1-multilib-1.0/arm-fsl-linux-gnueabi/bin/arm-fsl-linux-gnueabi- #|tee make.log
#if [ -f u-boot.bin ];then
#	sudo dd if=u-boot.bin of=u-boot-no-padding.bin bs=512 skip=2
#fi


mv "u-boot.bin" "u-boot_mddr_256-${PCBA}-${RAMTYPE}.bin"

rm -f ${tmpresult}* 

exit 0

