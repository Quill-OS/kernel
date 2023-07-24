# InkBox GNU/Linux kernel and bootloader

Useful resources and documentation to build a kernel for InkBox.

## Build
You can either use the provided toolchain in `toolchain/gcc-4.8` or your own. If you choose the first option, you'll need to install 32-bit libraries on your system for it to work (unless you're on a 32-bit machine or VM, of course).<br>
<b>Note: </b>First, you'll need set the initrd source files path in the kernel config. Replace `CONFIG_INITRAMFS_SOURCE` in `kernel/config/config-<device model number>` to match the location of the initrd's rootfs.
<br><br>To compile a kernel for a device in particular, use:<br>
```
GITDIR=/home/build/kobo TOOLCHAINDIR=$GITDIR/toolchain/gcc-4.8 TARGET=arm-linux-gnueabihf THREADS=4 scripts/build_kernel.sh n705 std
```
and change the relevant environment variables as needed.<br><br>
To compile a kernel for each and all devices, use:<br>
```
GITDIR=/home/build/kobo TOOLCHAINDIR=$GITDIR/toolchain/gcc-4.8 TARGET=arm-linux-gnueabihf THREADS=4 scripts/build_all.sh
```
## Additional notes:
- Don't forget to symlink the kernel repository to `/home/build/inkbox/kernel` or create the repository in that location. The path is fixed.

- If anything goes wrong, consider using a Debian container. You will need to install the following packages:
```
sudo apt install u-boot-tools make gcc xz-utils
```

Notes:
- If a device has some other models, remember to `git reset --hard` before building
- to make modules sqashfs: `mksquashfs dir new_modules -b 1048576 -comp gzip -always-use-fragments`
