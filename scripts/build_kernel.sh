#!/bin/bash

build_id_gen() {
	if [ -z "${1}" ]; then
		echo "You must specify a file."
	else
		BUILD_ID=$(tr -dc A-Za-z0-9 </dev/urandom | head -c 8)
		GIT_COMMIT=$(git rev-parse HEAD | head -c 7)
		echo "---- Kernel build ID is: ${BUILD_ID} ----"
		echo "---- Kernel commit is: ${GIT_COMMIT} ----"
		sudo su -c "echo ${BUILD_ID} > '${1}/build_id'"
		sudo su -c "echo ${GIT_COMMIT} > '${1}/commit'"
	fi
}

mkimage -V > /dev/null
if [ "$?" != 0 ]; then
	echo "mkimage (u-boot-tools) missing! Please install it."
	exit 1
fi

sudo --version > /dev/null
if [ "$?" != 0 ]; then
	echo "sudo missing! Please install it."
	exit 1
fi

if [ "$GITDIR" == "" ]; then
	echo "You must specify the path of the Git repository in the GITDIR environment variable."
	exit 1
elif [ "$TOOLCHAINDIR" == "" ]; then
	echo "You must specify the path of the toolchain in the TOOLCHAINDIR environment variable."
	echo "Usually, a good location for it would be in $GITDIR/toolchain/gcc-4.8"
	echo "Make sure that from TOOLCHAINDIR, there's a 'bin' folder in the first level that gives executables such as arm-linux-gnueabihf-gcc"
	exit 1
elif [ "$TARGET" == "" ]; then
	echo "You must specify a target in the TARGET environment variable."
	echo "Usually, it's something like 'arm-linux-gnueabihf', 'arm-kobo-linux-gnuabihf' or 'arm-nickel-linux-gnueabihf'."
	exit 1
elif [ "$THREADS" == "" ]; then
	echo "---- Warning: no THREADS environment variable. The kernel will be built with -j1 ----"
	THREADS=1
fi

# Environment
cd $TOOLCHAINDIR/bin
export PATH=$PATH:$PWD
cd - &> /dev/null

if [ "$1" == "n705" ]; then
	echo "---- Building Kobo Mini (N705) kernel ----"
elif [ "$1" == "n905c" ]; then
	echo "---- Building Kobo Touch model C (N905C) kernel ----"
elif [ "$1" == "n613" ]; then
	echo "---- Building Kobo Glo (N613) kernel ----"
elif [ "$1" == "n873" ]; then
	echo "---- Building Kobo Libra (N873) kernel ----"
elif [ "$1" == "n905b" ]; then
	echo "---- Building Kobo Touch model B (N905B) kernel ----"
elif [ "$1" == "n236" ]; then
	echo "---- Building Kobo Aura 2 (N236) kernel ----"
elif [ "$1" == "n437" ]; then
	echo "---- Building Kobo Glo HD (N437) kernel ----"
elif [ "$1" == "n306" ]; then
	echo "---- Building Kobo Nia (N306) kernel ----"
elif [ "$1" == "n249" ]; then
	echo "---- Building Kobo Clara HD (N249) kernel ----"
elif [ "$1" == "kt" ]; then
	echo "---- Building Kindle Touch (KT) kernel ----"
elif [ "$1" == "emu" ]; then
	echo "---- Building Emulator (EMU) kernel ----"
elif [ "$1" == "bpi" ]; then
	echo "---- Building Banana Pi M2 Zero (BPI) kernel ----"
else
	echo "You must specify a target to build for."
	echo "Available targets are: n705, n905c, n905b, n613, n236, n437, n306, n249, kt, n873, emu, bpi"
	exit 1
fi

if [ "$2" == "std" ]; then
	echo "---- Building STANDARD kernel for $1 ----"
elif [ "$2" == "root" ]; then
	echo "---- Building ROOT kernel for $1 ----"
elif [ "$2" == "diags" ]; then
	echo "---- Building DIAGNOSTICS kernel for $1 ----"
elif [ "$2" == "spl" ] && [ "$1" == "n873" ]; then
	echo "---- Building SPL kernel for $1 ----"
else
	echo "You must specify a valid kernel type."
	echo "Available types are: std, root, diags, spl"
	exit 1
fi

cd "${GITDIR}"
echo "---- Making device nodes ----"
env "GITDIR=${PWD}" scripts/make_devicenodes.sh

if [ "$1" == "n705" ]; then
	cd $GITDIR/kernel/linux-2.6.35.3
	make ARCH=arm CROSS_COMPILE=$TARGET- mrproper
	if [ "$2" == "diags" ]; then
		cp $GITDIR/kernel/config/config-n705-diags $GITDIR/kernel/linux-2.6.35.3/.config
	else
		cp $GITDIR/kernel/config/config-n705 $GITDIR/kernel/linux-2.6.35.3/.config
	fi
elif [ "$1" == "n905c" ]; then
	cd $GITDIR/kernel/linux-2.6.35.3
	make ARCH=arm CROSS_COMPILE=$TARGET- mrproper
	if [ "$2" == "diags" ]; then
		cp $GITDIR/kernel/config/config-n905c-diags $GITDIR/kernel/linux-2.6.35.3/.config
	else
		cp $GITDIR/kernel/config/config-n905c $GITDIR/kernel/linux-2.6.35.3/.config
	fi
elif [ "$1" == "n613" ]; then
	cd $GITDIR/kernel/linux-2.6.35.3
	make ARCH=arm CROSS_COMPILE=$TARGET- mrproper
	if [ "$2" == "diags" ]; then
		cp $GITDIR/kernel/config/config-n613-diags $GITDIR/kernel/linux-2.6.35.3/.config
	else
		cp $GITDIR/kernel/config/config-n613 $GITDIR/kernel/linux-2.6.35.3/.config
	fi
elif [ "$1" == "n873" ]; then
	cd $GITDIR/kernel/linux-4.1.15-libra
	make ARCH=arm CROSS_COMPILE=$TARGET- mrproper
	if [ "$2" == "diags" ]; then
		cp $GITDIR/kernel/config/config-n873-diags $GITDIR/kernel/linux-4.1.15-libra/.config
	elif [ "$2" == "spl" ]; then
		cp $GITDIR/kernel/config/config-n873-spl $GITDIR/kernel/linux-4.1.15-libra/.config
	else
		cp $GITDIR/kernel/config/config-n873 $GITDIR/kernel/linux-4.1.15-libra/.config
	fi
elif [ "$1" == "n905b" ]; then
	cd $GITDIR/kernel/linux-2.6.35.3-n905b
	make ARCH=arm CROSS_COMPILE=$TARGET- mrproper
	if [ "$2" == "diags" ]; then
		cp $GITDIR/kernel/config/config-n905b-diags $GITDIR/kernel/linux-2.6.35.3-n905b/.config
	else
		cp $GITDIR/kernel/config/config-n905b $GITDIR/kernel/linux-2.6.35.3-n905b/.config
	fi
elif [ "$1" == "emu" ]; then
	cd $GITDIR/kernel/linux-5.15.10
	make ARCH=arm CROSS_COMPILE=$TARGET- mrproper
	if [ "$2" == "diags" ]; then
		cp $GITDIR/kernel/config/config-emu-diags $GITDIR/kernel/linux-5.15.10/.config
	else
		cp $GITDIR/kernel/config/config-emu $GITDIR/kernel/linux-5.15.10/.config
	fi
elif [ "$1" == "bpi" ]; then
	cd $GITDIR/kernel/linux-5.10.89
	make ARCH=arm CROSS_COMPILE=$TARGET- mrproper
	if [ "$2" == "diags" ]; then
		cp $GITDIR/kernel/config/config-bpi-diags $GITDIR/kernel/linux-5.10.89/.config
	else
		cp $GITDIR/kernel/config/config-bpi $GITDIR/kernel/linux-5.10.89/.config
	fi
elif [ "$1" == "n236" ]; then
	cd "${GITDIR}/kernel/linux-3.0.35-n236"
	make ARCH=arm CROSS_COMPILE=$TARGET- mrproper
	cp "${GITDIR}/kernel/config/config-n236" "${GITDIR}/kernel/linux-3.0.35-n236/.config"
elif [ "$1" == "n437" ]; then
	cd "${GITDIR}/kernel/linux-3.0.35-n437"
	make ARCH=arm CROSS_COMPILE=$TARGET- mrproper
	cp "${GITDIR}/kernel/config/config-n437" "${GITDIR}/kernel/linux-3.0.35-n437/.config"
elif [ "$1" == "n306" ]; then
	cd "${GITDIR}/kernel/linux-4.1.15-n306"
	make ARCH=arm CROSS_COMPILE=$TARGET- mrproper
	cp "${GITDIR}/kernel/config/config-n306" "${GITDIR}/kernel/linux-4.1.15-n306/.config"
elif [ "$1" == "n249" ]; then
	cd "${GITDIR}/kernel/linux-5.16-n249"
	make ARCH=arm CROSS_COMPILE=$TARGET- mrproper
	cp "${GITDIR}/kernel/config/config-n249" "${GITDIR}/kernel/linux-5.16-n249/.config"
elif [ "$1" == "kt" ]; then
	cd "${GITDIR}/kernel/linux-2.6.31-kt"
	make ARCH=arm CROSS_COMPILE=$TARGET- mrproper
	if [ "$2" == "diags" ]; then
		cp "${GITDIR}/kernel/config/config-kt-diags" "${GITDIR}/kernel/linux-2.6.31-kt/.config"
	else
		cp "${GITDIR}/kernel/config/config-kt" "${GITDIR}/kernel/linux-2.6.31-kt/.config"
	fi
fi

mkdir -p $GITDIR/kernel/out/$1

# Build kernel
if [ "$2" == "std" ]; then
	sudo mkdir -p "${GITDIR}/initrd/${1}/etc/init.d"
	sudo mkdir -p "${GITDIR}/initrd/${1}/opt/bin"
	sudo su -c "echo 'noroot' > $GITDIR/initrd/$1/opt/root"
	sudo cp $GITDIR/initrd/common/rcS-std $GITDIR/initrd/$1/etc/init.d/rcS
	sudo cp $GITDIR/initrd/common/init $GITDIR/initrd/$1/etc/init.d/init
	sudo cp $GITDIR/initrd/common/startx $GITDIR/initrd/$1/etc/init.d/startx
	sudo cp $GITDIR/initrd/common/inkbox-splash $GITDIR/initrd/$1/etc/init.d/inkbox-splash
	sudo cp $GITDIR/initrd/common/developer-key $GITDIR/initrd/$1/etc/init.d/developer-key
	sudo cp $GITDIR/initrd/common/overlay-mount $GITDIR/initrd/$1/etc/init.d/overlay-mount
	sudo cp $GITDIR/initrd/common/initrd-fifo $GITDIR/initrd/$1/etc/init.d/initrd-fifo
	sudo cp $GITDIR/initrd/common/uidgen $GITDIR/initrd/$1/opt/bin/uidgen
	sudo cp $GITDIR/initrd/common/checksum-verify $GITDIR/initrd/$1/bin/checksum-verify
	sudo cp $GITDIR/initrd/common/setup-wifi $GITDIR/initrd/$1/sbin/setup-wifi
	mkdir -p $GITDIR/kernel/out/$1
	build_id_gen $GITDIR/initrd/$1/opt/

	if [ "$1" == "n705" ] || [ "$1" == "n905c" ] || [ "$1" == "n613" ]; then
		cd $GITDIR/kernel/linux-2.6.35.3
		make ARCH=arm CROSS_COMPILE=$TARGET- uImage -j$THREADS
	elif [ "$1" == "n873" ]; then
		cd $GITDIR/kernel/linux-4.1.15-libra
		make ARCH=arm CROSS_COMPILE=$TARGET- zImage -j$THREADS
	elif [ "$1" == "n905b" ]; then
		cd $GITDIR/kernel/linux-2.6.35.3-n905b
		make ARCH=arm CROSS_COMPILE=$TARGET- uImage -j$THREADS
	elif [ "$1" == "emu" ]; then
		cd $GITDIR/kernel/linux-5.15.10
		make ARCH=arm CROSS_COMPILE=$TARGET- zImage dtbs -j$THREADS
	elif [ "$1" == "bpi" ]; then
		cd $GITDIR/kernel/linux-5.10.89
		make ARCH=arm CROSS_COMPILE=$TARGET- zImage dtbs -j$THREADS
	elif [ "$1" == "n236" ]; then
		cd "${GITDIR}/kernel/linux-3.0.35-n236"
		make ARCH=arm CROSS_COMPILE=$TARGET- uImage -j$THREADS
	elif [ "$1" == "n437" ]; then
		cd "${GITDIR}/kernel/linux-3.0.35-n437"
		make ARCH=arm CROSS_COMPILE=$TARGET- uImage -j$THREADS
	elif [ "$1" == "n306" ]; then
		cd "${GITDIR}/kernel/linux-4.1.15-n306"
		make ARCH=arm CROSS_COMPILE=$TARGET- zImage -j$THREADS
	elif [ "$1" == "n249" ]; then
		cd "${GITDIR}/kernel/linux-5.16-n249"
		make ARCH=arm CROSS_COMPILE=$TARGET- zImage dtbs -j$THREADS
	elif [ "$1" == "kt" ]; then
		cd "${GITDIR}/kernel/linux-2.6.31-kt"
		make ARCH=arm CROSS_COMPILE=$TARGET- uImage -j$THREADS
	else
		cd $GITDIR/kernel/linux-2.6.35.3
		make ARCH=arm CROSS_COMPILE=$TARGET- uImage -j$THREADS
	fi

	if [ "$?" == 0 ]; then
		echo "---- STANDARD kernel compiled. ----"
		if [ "$1" == "n705" ] || [ "$1" == "n905c" ] || [ "$1" == "n613" ] || [ "$1" == "n905b" ] || [ "$1" == "n236" ] || [ "$1" == "n437" ] || [ "$1" == "kt" ]; then
			cp "arch/arm/boot/uImage" "$GITDIR/kernel/out/$1/uImage-std"
			echo "---- Output was saved in $GITDIR/kernel/out/$1/uImage-std ----"
		elif [ "$1" == "n873" ] || [ "$1" == "n306" ] || [ "$1" == "n249" ] || [ "$1" == "emu" ] || [ "$1" == "bpi" ]; then
			cp "arch/arm/boot/zImage" "$GITDIR/kernel/out/$1/zImage-std"
			echo "---- Output was saved in $GITDIR/kernel/out/$1/zImage-std ----"
		else
			cp "arch/arm/boot/uImage" "$GITDIR/kernel/out/$1/uImage-std"
			echo "---- Output was saved in $GITDIR/kernel/out/$1/uImage-std ----"
		fi
		exit 0
	else
		echo "---- There was an error during the build process, aborting... ----"
		exit 1
	fi

elif [ "$2" == "root" ]; then
	sudo mkdir -p "${GITDIR}/initrd/${1}/etc/init.d"
	sudo mkdir -p "${GITDIR}/initrd/${1}/opt/bin"
	sudo su -c "echo 'rooted' > $GITDIR/initrd/$1/opt/root"
	sudo cp $GITDIR/initrd/common/rcS-root $GITDIR/initrd/$1/etc/init.d/rcS
	sudo cp $GITDIR/initrd/common/init $GITDIR/initrd/$1/etc/init.d/init
	sudo cp $GITDIR/initrd/common/startx $GITDIR/initrd/$1/etc/init.d/startx
	sudo cp $GITDIR/initrd/common/inkbox-splash $GITDIR/initrd/$1/etc/init.d/inkbox-splash
	sudo cp $GITDIR/initrd/common/developer-key $GITDIR/initrd/$1/etc/init.d/developer-key
	sudo cp $GITDIR/initrd/common/overlay-mount $GITDIR/initrd/$1/etc/init.d/overlay-mount
	sudo cp $GITDIR/initrd/common/initrd-fifo $GITDIR/initrd/$1/etc/init.d/initrd-fifo
	sudo cp $GITDIR/initrd/common/uidgen $GITDIR/initrd/$1/opt/bin/uidgen
	sudo cp $GITDIR/initrd/common/checksum-verify $GITDIR/initrd/$1/bin/checksum-verify
	sudo cp $GITDIR/initrd/common/setup-wifi $GITDIR/initrd/$1/sbin/setup-wifi
	mkdir -p $GITDIR/kernel/out/$1
	build_id_gen $GITDIR/initrd/$1/opt/

	if [ "$1" == "n705" ] || [ "$1" == "n905c" ] || [ "$1" == "n613" ]; then
		cd $GITDIR/kernel/linux-2.6.35.3
		make ARCH=arm CROSS_COMPILE=$TARGET- uImage -j$THREADS
	elif [ "$1" == "n873" ]; then
		cd $GITDIR/kernel/linux-4.1.15-libra
		make ARCH=arm CROSS_COMPILE=$TARGET- zImage -j$THREADS
	elif [ "$1" == "n905b" ]; then
		cd $GITDIR/kernel/linux-2.6.35.3-n905b
		make ARCH=arm CROSS_COMPILE=$TARGET- uImage -j$THREADS
	elif [ "$1" == "emu" ]; then
		cd $GITDIR/kernel/linux-5.15.10
		make ARCH=arm CROSS_COMPILE=$TARGET- zImage dtbs -j$THREADS
	elif [ "$1" == "bpi" ]; then
		cd $GITDIR/kernel/linux-5.10.89
		make ARCH=arm CROSS_COMPILE=$TARGET- zImage dtbs -j$THREADS
	elif [ "$1" == "n236" ]; then
		cd "${GITDIR}/kernel/linux-3.0.35-n236"
		make ARCH=arm CROSS_COMPILE=$TARGET- uImage -j$THREADS
	elif [ "$1" == "n437" ]; then
		cd "${GITDIR}/kernel/linux-3.0.35-n437"
		make ARCH=arm CROSS_COMPILE=$TARGET- uImage -j$THREADS
	elif [ "$1" == "n306" ]; then
		cd "${GITDIR}/kernel/linux-4.1.15-n306"
		make ARCH=arm CROSS_COMPILE=$TARGET- zImage -j$THREADS
	elif [ "$1" == "n249" ]; then
		cd "${GITDIR}/kernel/linux-5.16-n249"
		make ARCH=arm CROSS_COMPILE=$TARGET- zImage dtbs -j$THREADS
	elif [ "$1" == "kt" ]; then
		cd "${GITDIR}/kernel/linux-2.6.31-kt"
		make ARCH=arm CROSS_COMPILE=$TARGET- uImage -j$THREADS
	else
		cd $GITDIR/kernel/linux-2.6.35.3
		make ARCH=arm CROSS_COMPILE=$TARGET- uImage -j$THREADS
	fi

	if [ "$?" == 0 ]; then
		echo "---- ROOT kernel compiled. ----"
		if [ "$1" == "n705" ] || [ "$1" == "n905c" ] || [ "$1" == "n613" ] || [ "$1" == "n905b" ] || [ "$1" == "n236" ] || [ "$1" == "n437" ] || [ "$1" == "kt" ]; then
			cp "arch/arm/boot/uImage" "$GITDIR/kernel/out/$1/uImage-root"
			echo "---- Output was saved in $GITDIR/kernel/out/$1/uImage-root ----"
		elif [ "$1" == "n873" ] || [ "$1" == "n306" ] || [ "$1" == "n249" ] || [ "$1" == "emu" ] || [ "$1" == "bpi" ]; then
			cp "arch/arm/boot/zImage" "$GITDIR/kernel/out/$1/zImage-root"
			echo "---- Output was saved in $GITDIR/kernel/out/$1/zImage-root ----"
		else
			cp "arch/arm/boot/uImage" "$GITDIR/kernel/out/$1/uImage-root"
			echo "---- Output was saved in $GITDIR/kernel/out/$1/uImage-root ----"
		fi
		exit 0
	else
		echo "---- There was an error during the build process, aborting... ----"
		exit 1
	fi

elif [ "$2" == "diags" ]; then
	if [ "$1" == "n705" ]; then
		mkdir -p $GITDIR/kernel/out/n705
	elif [ "$1" == "n905c" ]; then
		mkdir -p $GITDIR/kernel/out/n905c
	elif [ "$1" == "n613" ]; then
		mkdir -p $GITDIR/kernel/out/n613
	elif [ "$1" == "n905b" ]; then
		mkdir -p $GITDIR/kernel/out/n905b
	elif [ "$1" == "kt" ]; then
		mkdir -p $GITDIR/kernel/out/kt
	elif [ "$1" == "n873" ]; then
		mkdir -p $GITDIR/kernel/out/n873
	elif [ "$1" == "emu" ]; then
		mkdir -p $GITDIR/kernel/out/emu
	elif [ "$1" == "bpi" ]; then
		mkdir -p $GITDIR/kernel/out/bpi
	fi

	if [ "$1" == "n705" ] || [ "$1" == "n905c" ] || [ "$1" == "n613" ]; then
		cd $GITDIR/kernel/linux-2.6.35.3
		make ARCH=arm CROSS_COMPILE=$TARGET- uImage -j$THREADS
	elif [ "$1" == "n873" ]; then
		cd $GITDIR/kernel/linux-4.1.15-libra
		make ARCH=arm CROSS_COMPILE=$TARGET- zImage -j$THREADS
	elif [ "$1" == "n905b" ]; then
		cd $GITDIR/kernel/linux-2.6.35.3-n905b
		make ARCH=arm CROSS_COMPILE=$TARGET- uImage -j$THREADS
	elif [ "$1" == "kt" ]; then
		cd $GITDIR/kernel/linux-2.6.31-kt
		make ARCH=arm CROSS_COMPILE=$TARGET- uImage -j$THREADS
	elif [ "$1" == "emu" ]; then
		cd $GITDIR/kernel/linux-5.15.10
		make ARCH=arm CROSS_COMPILE=$TARGET- zImage dtbs -j$THREADS
	elif [ "$1" == "bpi" ]; then
		cd $GITDIR/kernel/linux-5.10.89
		make ARCH=arm CROSS_COMPILE=$TARGET- zImage dtbs -j$THREADS
	else
		cd $GITDIR/kernel/linux-2.6.35.3
		make ARCH=arm CROSS_COMPILE=$TARGET- uImage -j$THREADS
	fi

	if [ "$?" == 0 ]; then
		echo "---- DIAGNOSTICS kernel compiled. ----"
		if [ "$1" == "n705" ] || [ "$1" == "n905c" ] || [ "$1" == "n613" ] || [ "$1" == "n905b" ] || [ "$1" == "kt" ]; then
			cp "arch/arm/boot/uImage" "$GITDIR/kernel/out/$1/uImage-diags"
			echo "---- Output was saved in $GITDIR/kernel/out/$1/uImage-diags ----"
		elif [ "$1" == "n873" ] || [ "$1" == "emu" ] || [ "$1" == "bpi" ]; then
			cp "arch/arm/boot/zImage" "$GITDIR/kernel/out/$1/zImage-diags"
			echo "---- Output was saved in $GITDIR/kernel/out/$1/zImage-diags ----"
		else
			cp "arch/arm/boot/uImage" "$GITDIR/kernel/out/$1/uImage-diags"
			echo "---- Output was saved in $GITDIR/kernel/out/$1/uImage-diags ----"
		fi
		exit 0
	else
		echo "---- There was an error during the build process, aborting... ----"
		exit 1
	fi

elif [ "$2" == "spl" ]; then
	if [ "$1" == "n873" ]; then
		cd $GITDIR/kernel/linux-4.1.15-libra
		make ARCH=arm CROSS_COMPILE=$TARGET- zImage -j$THREADS
	fi

	if [ "$?" == 0 ]; then
		echo "---- SPL kernel compiled. ----"
		cp "arch/arm/boot/zImage" "$GITDIR/kernel/out/$1/zImage-spl"
		echo "---- Output was saved in $GITDIR/kernel/out/$1/zImage ----"
		exit 0
	else
		echo "---- There was an error during the build process, aborting... ----"
		exit 1
	fi
fi
