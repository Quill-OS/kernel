#!/bin/bash
make distclean

IDSTR="uboot-build-$(date +%Y%m%d_%H%M%S)"
tmpresult="/tmp/${IDSTR}.tmp"


#MODELS=("E60610D" "E50610" "E606B0" "E606A0" "E60612")
#RAMTYPES=("C" "D")
_cross_compile="/opt/freescale/usr/local/gcc-4.4.4-glibc-2.11.1-multilib-1.0/arm-fsl-linux-gnueabi/bin/arm-fsl-linux-gnueabi-"

########################
##### GLOBAL #######

PCBA=""
RAMTYPE=""
RAMSIZE=""
RAM_TYPE=""
########################

select_ramtype() {
	LANG=C dialog --stdout --ascii-lines --title "mx50 u-boot select" --menu "RAMTYPE selection" 15 70 13 \
		"LPDDR2" "RAMTYPE=LPDDR2" \
		"DDR2" "RAMTYPE=LPDDR2" \
		"MDDR" "RAMTYPE=MDDR" \
		"DDR3" "RAMTYPE=DDR3" >"${tmpresult}2"
	retval=$?
	if [ "${retval}" = "0" ];then
		RAMTYPE="$(cat "${tmpresult}2")"
		if [ "${RAMTYPE}" = "LPDDR2" ];then
			RAM_TYPE="lpddr2"
		elif [ "${RAMTYPE}" = "DDR3" ];then
			RAM_TYPE="ddr3"
		elif [ "${RAMTYPE}" = "DDR2" ];then
			RAM_TYPE="ddr2"
		elif [ "${RAMTYPE}" = "MDDR" ];then
			RAM_TYPE="mddr"
		else
			echo "unkown RAMTYPE \"${RAMTYPE}\""
			return 2
		fi
	else
		return 1
	fi
	return 0
}


select_ram_size() {
	LANG=C dialog --stdout --ascii-lines --title "mx50 u-boot select" --menu "RAM size selection" 15 70 13 \
		"128" "128MB RAM size" \
		"256" "256MB RAM size" \
		"512" "512MB RAM size" \
	>"${tmpresult}2"
	retval=$?
	if [ "${retval}" = "0" ];then
		RAMSIZE="$(cat "${tmpresult}2")"
	else
		return 1
	fi
	return 0
}

select_pcba() {
	LANG=C dialog --stdout --ascii-lines --title "mx50 u-boot select" --menu "PCBA selection" 15 70 13 \
		"E606G0" "PCBA=E606G0,LPDDR2 512MB" \
		"E606C0" "PCBA=E606C0,MDDR 512MB" \
	>"${tmpresult}2"
	retval=$?
	if [ "${retval}" = "0" ];then
		PCBA="$(cat "${tmpresult}2")"
	else
		return 1
	fi
	return 0
}

select_pcba
[ $? != 0 ] && echo "select pcba fail !" && exit 1

#select_ramtype
#[ $? != 0 ] && echo "select ramtype fail !" && exit 1

case "${PCBA}" in 
	"E606G0")
		RAMSIZE="512"
		RAM_TYPE="lpddr2"
		RAMTYPE="LPDDR2"

		make distclean
		make ARCH=arm CROSS_COMPILE=${_cross_compile} mx50_rdp_lpddr2_512_config

		cd board/freescale/mx50_rdp
		rm flash_header.S;sync;sleep 1
		ln -s flash_header-LPDDR2.S flash_header.S
		cd -
		;;
	"E606C0")
		RAMSIZE="512"
		RAM_TYPE="mddr"
		RAMTYPE="K4X2G323PC"

		make distclean
		make ARCH=arm CROSS_COMPILE=${_cross_compile} mx50_rdp_mddr_512_config

		cd board/freescale/mx50_rdp
		rm flash_header.S;sync;sleep 1
		ln -s flash_header-20120622_FSL_RAM_PARMS_DSadd2.S flash_header.S
		cd -
		;;

	*)
		echo "unkown PCBA \"${PCBA}\" !"
		exit 1 
		;;

esac

make ARCH=arm CROSS_COMPILE=${_cross_compile} #|tee make.log

mv "u-boot.bin" "u-boot_${RAM_TYPE}_${RAMSIZE}-${PCBA}-${RAMTYPE}.bin"
rm -f ${tmpresult}* 

exit 0

