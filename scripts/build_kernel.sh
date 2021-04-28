#!/bin/bash

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
cd - > /dev/null

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
else
	echo "You must specify a valid kernel type."
	echo "Available types are: std, root"
	exit 1
fi

if [ "$1" == "n705" ]; then
	cd $GITDIR/kernel/linux-2.6.35.3
	make ARCH=arm CROSS_COMPILE=$TARGET- mrproper
	cp $GITDIR/kernel/config/config-n705 $GITDIR/kernel/linux-2.6.35.3/.config
elif [ "$1" == "n905c" ]; then
	cd $GITDIR/kernel/linux-2.6.35.3
	make ARCH=arm CROSS_COMPILE=$TARGET- mrproper
	cp $GITDIR/kernel/config/config-n905c $GITDIR/kernel/linux-2.6.35.3/.config
fi

# Build kernel
if [ "$2" == "std" ]; then
	if [ "$1" == "n705" ]; then
		sudo cp $GITDIR/initrd/common/rcS-std $GITDIR/initrd/n705/etc/init.d/rcS
		sudo cp $GITDIR/initrd/common/startx $GITDIR/initrd/n705/etc/init.d/startx
		sudo cp $GITDIR/initrd/common/inkbox-splash $GITDIR/initrd/n705/etc/init.d/inkbox-splash
		mkdir -p $GITDIR/kernel/out/n705
	elif [ "$1" == "n905c" ]; then
		sudo cp $GITDIR/initrd/common/rcS-std $GITDIR/initrd/n905c/etc/init.d/rcS
		sudo cp $GITDIR/initrd/common/startx $GITDIR/initrd/n905c/etc/init.d/startx
		sudo cp $GITDIR/initrd/common/inkbox-splash $GITDIR/initrd/n905c/etc/init.d/inkbox-splash
		mkdir -p $GITDIR/kernel/out/n905c
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
		sudo cp $GITDIR/initrd/common/rcS-root $GITDIR/initrd/n705/etc/init.d/rcS
		sudo cp $GITDIR/initrd/common/startx $GITDIR/initrd/n705/etc/init.d/startx
		sduo cp $GITDIR/initrd/common/inkbox-splash $GITDIR/initrd/n705/etc/init.d/inkbox-splash
		mkdir -p $GITDIR/kernel/out/n705
	elif [ "$1" == "n905c" ]; then
		sudo cp $GITDIR/initrd/common/rcS-root $GITDIR/initrd/n905c/etc/init.d/rcS
		sudo cp $GITDIR/initrd/common/startx $GITDIR/initrd/n905c/etc/init.d/startx
		sudo cp $GITDIR/initrd/common/inkbox-splash $GITDIR/initrd/n905c/etc/init.d/inkbox-splash
		mkdir -p $GITDIR/kernel/out/n905c
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
fi
