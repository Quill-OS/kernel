#!/bin/bash -e

cd "$(dirname '${0}')"
GITDIR="${PWD}"

[ -z "${TOOLCHAINDIR}" ] && printf "You must specify the 'TOOLCHAINDIR' environment variable.\n" && exit 1
[ -z "${TARGET}" ] && printf "You must specify the 'TARGET' environment variable. Example: 'arm-linux-gnueabihf'\n" && exit 1
[ -z "${THREADS}" ] && THREADS=1
[ -z "${1}" ] && printf "You must specify the 'device' argument. Available options are: n705, n905b, n905c, n613, n236, n437, n306, n249, n367, n418, n428, kt\n" && exit 1
DEVICE="${1}"

mkdir -p "${GITDIR}/bootloader/out/"
pushd "${TOOLCHAINDIR}/bin" && PATH="${PATH}:${PWD}" && popd

if [ "${DEVICE}" == "n705" ] || [ "${DEVICE}" == "n905c" ] || [ "${DEVICE}" == "n613" ]; then
	pushd "${GITDIR}/bootloader/imx507"
	
	pushd "board/freescale/mx50_rdp"
	rm flash_header.S && sync
	ln -s flash_header-20120622_FSL_RAM_PARMS_DSadd2.S flash_header.S && sync
	popd
	
	make ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS} distclean
	make ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS} mx50_rdp_config
	make ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS}
	cp "u-boot.bin" "${GITDIR}/bootloader/out/u-boot_inkbox.${DEVICE}.bin"

	popd
elif [ "${DEVICE}" == "n905b" ]; then
	pushd "${GITDIR}/bootloader/imx508"

	make ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS} distclean
	make ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS} mx50_rdp_config
	make ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS}
	cp "u-boot.bin" "${GITDIR}/bootloader/out/u-boot_inkbox.${DEVICE}.bin"

	popd
elif [ "${DEVICE}" == "n236" ]; then
	pushd "${GITDIR}/bootloader/mx6sl-n236"

	make ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS} distclean
	make ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS} mx6sl_ntx_lpddr2_256m_config
	make ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS}
	cp "u-boot.bin" "${GITDIR}/bootloader/out/u-boot_inkbox.${DEVICE}.bin"

	popd
elif [ "${DEVICE}" == "n437" ]; then
	pushd "${GITDIR}/bootloader/mx6sl-n437"

	make ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS} distclean
	CONFIG="include/configs/mx6sl_ntx_lpddr2.h"
	git restore "${CONFIG}"
	if [ "${2}" == "usb-boot" ]; then
		sed -i '3i #define CONFIG_USB_BOOT' "${CONFIG}"
	fi
	make ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS} mx6sl_ntx_lpddr2_config
	make ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS}
	cp "u-boot.bin" "${GITDIR}/bootloader/out/u-boot_inkbox.${DEVICE}.bin"

	popd
elif [ "${DEVICE}" == "n306" ] || [ "${DEVICE}" == "n306c" ]; then
	pushd "${GITDIR}/bootloader/mx6ull-n306"

	make ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS} distclean
	make ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS} mx6ull_ntx_lpddr2_256m_defconfig
	make ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS}
	cp "u-boot.imx" "${GITDIR}/bootloader/out/u-boot_inkbox.${DEVICE}.imx"

	popd
elif [ "${DEVICE}" == "n249" ]; then
	pushd "${GITDIR}/bootloader/u-boot-fslc"

	make ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS} distclean
	make ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS} mx6sllclarahd_defconfig
	scripts/config --set-str BOOTCOMMAND "detect_clara_rev ; run distro_bootcmd ; fastboot 0"
        scripts/config -e ENV_IS_IN_EXT4
        scripts/config -d ENV_IS_IN_MMC
        scripts/config --set-str ENV_EXT4_INTERFACE "mmc"
        scripts/config --set-str ENV_EXT4_DEVICE_AND_PART "0:1"
        scripts/config --set-str ENV_EXT4_FILE "/uboot.env"
	make ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS}
	cp "u-boot-dtb.imx" "${GITDIR}/bootloader/out/u-boot_inkbox.${DEVICE}.imx"

	popd
elif [ "${DEVICE}" == "n418" ]; then
	pushd "${GITDIR}/bootloader/mx6sll-n418"

	make ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS} distclean
	CONFIG="include/configs/mx6sll_ntx.h"
	git restore "${CONFIG}"
	if [ "${2}" == "usb-boot" ]; then
		sed -i '11i #define CONFIG_USB_BOOT' "${CONFIG}"
	fi
	make ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS} mx6sll_ntx_lpddr2_512m_E70K10_defconfig
	make ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS}
	cp "u-boot.imx" "${GITDIR}/bootloader/out/u-boot_inkbox.${DEVICE}.imx"

	popd
elif [ "${DEVICE}" == "n428" ] || [ "${DEVICE}" == "n367" ]; then
	pushd "${GITDIR}/bootloader/mt8113-${DEVICE}"

	make ARCH=arm CROSS_COMPILE="${TARGET}-" distclean
	make ARCH=arm CROSS_COMPILE="${TARGET}-" mt8113_tp1_emmc_defconfig
	make ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS}
	mkimage -f mt8113-u-boot.its u-boot.itb
	cp "u-boot.itb" "${GITDIR}/bootloader/out/u-boot_inkbox.${DEVICE}.itb"

	popd
elif [ "${DEVICE}" == "kt" ]; then
	pushd "${GITDIR}/bootloader/imx508-kt"

	make TYPE=prod ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS} distclean
	make TYPE=prod ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS} imx50_yoshi_config
	make TYPE=prod ARCH=arm CROSS_COMPILE="${TARGET}-" -j${THREADS}
	cp "u-boot.bin" "${GITDIR}/bootloader/out/u-boot_inkbox.${DEVICE}.bin"

	popd
fi
