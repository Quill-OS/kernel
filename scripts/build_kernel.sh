#!/bin/bash

build_id_gen() {
	if [ -z "${1}" ]; then
		echo "You must specify a file."
	else
		BUILD_ID=$(tr -dc A-Za-z0-9 </dev/urandom | head -c 8)
		echo "---- Build ID is: ${BUILD_ID} ----"
		echo ${BUILD_ID} > "${1}"
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
	echo "Usually, a good location for it would be in $GITDIR/toolchain"
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
else
	echo "You must specify a kernel configuration to build for."
	echo "Available configurations are: n705, n905c"
	exit 1
fi

if [ "$2" == "std" ]; then
	echo "---- Building STANDARD kernel for $1 ----"
elif [ "$2" == "root" ]; then
	echo "---- Building ROOT kernel for $1 ----"
elif [ "$2" == "diags" ]; then
	echo "---- Building DIAGNOSTICS kernel for $1 ----"
else
	echo "You must specify a valid kernel type."
	echo "Available types are: std, root, diags"
	exit 1
fi

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
fi

# Build kernel
if [ "$2" == "std" ]; then
	if [ "$1" == "n705" ]; then
		sudo mkdir -p $GITDIR/initrd/n705/etc/init.d
		sudo mkdir -p $GITDIR/initrd/n705/opt/bin
		sudo cp $GITDIR/initrd/common/rcS-std $GITDIR/initrd/n705/etc/init.d/rcS
		sudo cp $GITDIR/initrd/common/startx $GITDIR/initrd/n705/etc/init.d/startx
		sudo cp $GITDIR/initrd/common/inkbox-splash $GITDIR/initrd/n705/etc/init.d/inkbox-splash
		sudo cp $GITDIR/initrd/common/developer-key $GITDIR/initrd/n705/etc/init.d/developer-key
		sudo cp $GITDIR/initrd/common/overlay-mount $GITDIR/initrd/n705/etc/init.d/overlay-mount
		sudo cp $GITDIR/initrd/common/initrd-fifo $GITDIR/initrd/n705/etc/init.d/initrd-fifo
		sudo cp $GITDIR/initrd/common/uidgen $GITDIR/initrd/n705/opt/bin/uidgen
		build_id_gen $GITDIR/initrd/n705/opt/build_id
		mkdir -p $GITDIR/kernel/out/n705
	elif [ "$1" == "n905c" ]; then
		sudo mkdir -p $GITDIR/initrd/n905c/etc/init.d
		sudo mkdir -p $GITDIR/initrd/n905c/opt/bin
		sudo cp $GITDIR/initrd/common/rcS-std $GITDIR/initrd/n905c/etc/init.d/rcS
		sudo cp $GITDIR/initrd/common/startx $GITDIR/initrd/n905c/etc/init.d/startx
		sudo cp $GITDIR/initrd/common/inkbox-splash $GITDIR/initrd/n905c/etc/init.d/inkbox-splash
		sudo cp $GITDIR/initrd/common/developer-key $GITDIR/initrd/n905c/etc/init.d/developer-key
		sudo cp $GITDIR/initrd/common/overlay-mount $GITDIR/initrd/n905c/etc/init.d/overlay-mount
		sudo cp $GITDIR/initrd/common/initrd-fifo $GITDIR/initrd/n905c/etc/init.d/initrd-fifo
		sudo cp $GITDIR/initrd/common/uidgen $GITDIR/initrd/n905c/opt/bin/uidgen
		mkdir -p $GITDIR/kernel/out/n905c
		build_id_gen $GITDIR/initrd/n905c/opt/build_id
	fi
	cd $GITDIR/kernel/linux-2.6.35.3
	make ARCH=arm CROSS_COMPILE=$TARGET- uImage
	if [ "$?" == 0 ]; then
		echo "---- STANDARD kernel compiled. ----"
		cp "arch/arm/boot/uImage" "$GITDIR/kernel/out/$1/uImage-std"
		echo "---- Output was saved in $GITDIR/kernel/out/$1/uImage-std ----"
		exit 0
	else
		echo "---- There was an error during the build process, aborting... ----"
		exit 1
	fi

elif [ "$2" == "root" ]; then
	if [ "$1" == "n705" ]; then
		sudo mkdir -p $GITDIR/initrd/n705/etc/init.d
		sudo mkdir -p $GITDIR/initrd/n705/opt/bin
		sudo cp $GITDIR/initrd/common/rcS-root $GITDIR/initrd/n705/etc/init.d/rcS
		sudo cp $GITDIR/initrd/common/startx $GITDIR/initrd/n705/etc/init.d/startx
		sudo cp $GITDIR/initrd/common/inkbox-splash $GITDIR/initrd/n705/etc/init.d/inkbox-splash
		sudo cp $GITDIR/initrd/common/developer-key $GITDIR/initrd/n705/etc/init.d/developer-key
		sudo cp $GITDIR/initrd/common/overlay-mount $GITDIR/initrd/n705/etc/init.d/overlay-mount
		sudo cp $GITDIR/initrd/common/initrd-fifo $GITDIR/initrd/n705/etc/init.d/initrd-fifo
		sudo cp $GITDIR/initrd/common/uidgen $GITDIR/initrd/n705/opt/bin/uidgen
		mkdir -p $GITDIR/kernel/out/n705
		build_id_gen $GITDIR/initrd/n705/opt/build_id
	elif [ "$1" == "n905c" ]; then
		sudo mkdir -p $GITDIR/initrd/n905c/etc/init.d
		sudo mkdir -p $GITDIR/initrd/n905c/opt/bin
		sudo cp $GITDIR/initrd/common/rcS-root $GITDIR/initrd/n905c/etc/init.d/rcS
		sudo cp $GITDIR/initrd/common/startx $GITDIR/initrd/n905c/etc/init.d/startx
		sudo cp $GITDIR/initrd/common/inkbox-splash $GITDIR/initrd/n905c/etc/init.d/inkbox-splash
		sudo cp $GITDIR/initrd/common/developer-key $GITDIR/initrd/n905c/etc/init.d/developer-key
		sudo cp $GITDIR/initrd/common/overlay-mount $GITDIR/initrd/n905c/etc/init.d/overlay-mount
		sudo cp $GITDIR/initrd/common/initrd-fifo $GITDIR/initrd/n905c/etc/init.d/initrd-fifo
		sudo cp $GITDIR/initrd/common/uidgen $GITDIR/initrd/n905c/opt/bin/uidgen
		mkdir -p $GITDIR/kernel/out/n905c
		build_id_gen $GITDIR/initrd/n905c/opt/build_id
	fi
	cd $GITDIR/kernel/linux-2.6.35.3
	make ARCH=arm CROSS_COMPILE=$TARGET- uImage -j$THREADS
	if [ "$?" == 0 ]; then
		echo "---- ROOT kernel compiled. ----"
		cp "arch/arm/boot/uImage" "$GITDIR/kernel/out/$1/uImage-root"
		echo "---- Output was saved in $GITDIR/kernel/out/$1/uImage-root ----"
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
	fi
	cd $GITDIR/kernel/linux-2.6.35.3
	make ARCH=arm CROSS_COMPILE=$TARGET- uImage -j$THREADS
	if [ "$?" == 0 ]; then
		echo "---- DIAGNOSTICS kernel compiled. ----"
		cp "arch/arm/boot/uImage" "$GITDIR/kernel/out/$1/uImage-diags"
		echo "---- Output was saved in $GITDIR/kernel/out/$1/uImage-diags ----"
		exit 0
	else
		echo "---- There was an error during the build process, aborting... ----"
		exit 1
	fi
fi
