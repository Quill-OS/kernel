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
RAMSIZE=""
RAM_TYPE=""
########################

select_ramtype() {
	LANG=C dialog --stdout --ascii-lines --title "mx6sl u-boot select" --menu "RAMTYPE selection" 15 70 13 \
		"LPDDR2" "RAMTYPE=LPDDR2" \
		"DDR3" "RAMTYPE=DDR3" >"${tmpresult}2"
	retval=$?
	if [ "${retval}" = "0" ];then
		RAMTYPE="$(cat "${tmpresult}2")"
		if [ "${RAMTYPE}" = "LPDDR2" ];then
			RAM_TYPE="lpddr2"
		elif [ "${RAMTYPE}" = "DDR3" ];then
			RAM_TYPE="ddr3"
		else
			echo "unkown RAMTYPE \"${RAMTYPE}\""
			return 2
		fi
	else
		return 1
	fi
	return 0
}


select_pcba() {
	LANG=C dialog --stdout --ascii-lines --title "mx6sl u-boot select" --menu "PCBA selection" 20 70 13 \
		"E60Q00" "PCBA=E60Q00,DDR3 256MB" \
		"E60Q10" "PCBA=E60Q10,LPDDR2 Elpida 512MB" \
		"E60Q20" "PCBA=E60Q20,LPDDR2 Micron MT42L128M32D1 512MB" \
		"E60Q30" "PCBA=E60Q30,LPDDR2 Micron MT42L128M32D1 512MB" \
		"E60Q50" "PCBA=E60Q30,LPDDR2 Micron MT42L128M32D1 512MB" \
		"E60Q60" "PCBA=E60Q30,LPDDR2 Micron MT42L128M32D1 512MB" \
		"E60QB0" "PCBA=E60QB0,LPDDR2 Micron MT42L128M32D1 256MB" \
		"E60QC0" "PCBA=E60QB0,LPDDR2 Micron MT42L128M32D1 512MB" \
		"E60Q80" "PCBA=E60Q80,LPDDR2 512MB" \
		"E60Q90" "PCBA=E60Q90,LPDDR2 512/256 MB" \
		"E60QA0" "PCBA=E60QA0,LPDDR2 256MB" \
		"ED0Q00" "PCBA=ED0Q00,LPDDR2 512MB" \
	>"${tmpresult}2"
	retval=$?
	if [ "${retval}" = "0" ];then
		PCBA="$(cat "${tmpresult}2")"
	else
		return 1
	fi
	return 0
}
select_ram_size() {
	LANG=C dialog --stdout --ascii-lines --title "mx6sl u-boot select" --menu "RAM size selection" 15 70 13 \
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

select_pcba
[ $? != 0 ] && echo "select pcba fail !" && exit 1

#select_ramtype
#[ $? != 0 ] && echo "select ramtype fail !" && exit 1

if [ "${PCBA}" = "E60Q00" ];then
	RAMSIZE="256"
	RAM_TYPE="ddr3"
	RAMTYPE="DDR3"

	make distclean
	make ARCH=arm CROSS_COMPILE=/opt/freescale/usr/local/gcc-4.6.2-glibc-2.13-linaro-multilib-2011.12/fsl-linaro-toolchain/bin/arm-fsl-linux-gnueabi- mx6sl_ntx_config
elif [ "${PCBA}" = "E60Q10" ];then
	RAMSIZE="512"
	RAM_TYPE="lpddr2"
	RAMTYPE="LPDDR2"

	make distclean
	make ARCH=arm CROSS_COMPILE=/opt/freescale/usr/local/gcc-4.6.2-glibc-2.13-linaro-multilib-2011.12/fsl-linaro-toolchain/bin/arm-fsl-linux-gnueabi- mx6sl_ntx_lpddr2_config
elif [ "${PCBA}" = "E60Q20" ] || [ "${PCBA}" = "E60Q30" ] || [ "${PCBA}" = "E60QB0" ] || \
	 [ "${PCBA}" = "E60Q50" ] || [ "${PCBA}" = "E60Q60" ] || [ "${PCBA}" = "E60Q80" ] || \
	 [ "${PCBA}" = "E60QC0" ] || [ "${PCBA}" = "E60Q90" ] || [ "${PCBA}" = "ED0Q00" ] || \
	 [ "${PCBA}" = "E60QA0" ];then


	if [ "${PCBA}" = "E60QB0" ] || [ "${PCBA}" = "E60QA0" ];then
		#select_ram_size
		RAMSIZE="256"
	else
		RAMSIZE="512"
	fi

	if [ "${PCBA}" = "E60Q80" ];then
		pushd "board/freescale/mx6sl_ntx"
		rm "flash_header.S"
		ln -s "20140731_Netronix_MX6SL_LPDDR2_512MB_000.inc.flash_header.S" "flash_header.S"
		popd 
	elif [ "${PCBA}" = "ED0Q00" ];then
		pushd "board/freescale/mx6sl_ntx"
		rm "flash_header.S"
		ln -s "MX6SL_MMDC_LPDDR2_register_programming_aid_v0.9.inc.flash_header.S" "flash_header.S"
		popd 
	elif [ "${PCBA}" = "E60Q90" ];then
		select_ram_size
		[ $? != 0 ] && echo "select ram size fail !" && exit 1
		pushd "board/freescale/mx6sl_ntx"
		rm "flash_header.S"
		ln -s "flash_header.E60Q90.S" "flash_header.S"
		popd 
	else
		pushd "board/freescale/mx6sl_ntx"
		rm "flash_header.S"
		ln -s "flash_header.S_mp" "flash_header.S"
		popd
	fi

	if [ "${RAMSIZE}" = "256" ];then
		UBOOT_CFG="mx6sl_ntx_lpddr2-MT42L128M32D1-256MB_config"
	else
		UBOOT_CFG="mx6sl_ntx_lpddr2-MT42L128M32D1_config"
	fi

	RAMSIZE_BYTES="$(echo "${RAMSIZE}"|awk '{print $1*1024*1024}')"
	RAM_TYPE="lpddr2"
	RAMTYPE="LPDDR2"

	make distclean
	make ARCH=arm CROSS_COMPILE=/opt/freescale/usr/local/gcc-4.6.2-glibc-2.13-linaro-multilib-2011.12/fsl-linaro-toolchain/bin/arm-fsl-linux-gnueabi- "${UBOOT_CFG}"
elif [ "${PCBA}" = "E60Q20-256MB" ];then
        RAMSIZE="256"
        RAM_TYPE="lpddr2"
        RAMTYPE="LPDDR2"

        make distclean
        make ARCH=arm CROSS_COMPILE=/opt/freescale/usr/local/gcc-4.6.2-glibc-2.13-linaro-multilib-2011.12/fsl-linaro-toolchain/bin/arm-fsl-linux-gnueabi- mx6sl_ntx_lpddr2_256m_config
else
	echo "unkown PCBA \"${PCBA}\""
	exit 1
fi
make ARCH=arm CROSS_COMPILE=/opt/freescale/usr/local/gcc-4.6.2-glibc-2.13-linaro-multilib-2011.12/fsl-linaro-toolchain/bin/arm-fsl-linux-gnueabi- #|tee make.log

mv "u-boot.bin" "u-boot_${RAM_TYPE}_${RAMSIZE}-${PCBA}-${RAMTYPE}.bin"
rm -f ${tmpresult}* 

exit 0

