# InkBox GNU/Linux kernel and bootloader

Useful resources and documentation to build a kernel for InkBox.

## Build
You can either use the provided toolchain in `toolchain/gcc-4.8` or your own. If you choose the first option, you'll need to install 32-bit libraries on your system for it to work (unless you're on a 32-bit machine or VM, of course).<br>
<b>Note: </b>First, you'll need set the initrd source files path in the kernel config. Replace `CONFIG_INITRAMFS_SOURCE` in `kernel/config/{n705,n905c}` to match the location of the initrd's rootfs. Also, you should run `scripts/make_devicenodes.sh` (prepending a GITDIR environment variable) before building any kernel or it will not boot.
<br><br>To compile a kernel for a device in particular, use:<br>
```
GITDIR=/home/build/kobo TOOLCHAINDIR=$GITDIR/toolchain/gcc-4.8 TARGET=arm-linux-gnueabihf THREADS=4 scripts/build_kernel.sh n705 std
```
and change the relevant environment variables as needed.<br><br>
To compile a kernel for each and all devices, use:<br>
```
GITDIR=/home/build/kobo TOOLCHAINDIR=$GITDIR/toolchain/gcc-4.8 TARGET=arm-linux-gnueabihf THREADS=4 scripts/build_all.sh
```
