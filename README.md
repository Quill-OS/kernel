# InkBox GNU/Linux kernel and bootloader

Useful resources and documentation to build a kernel for InkBox.

## Build
There are different toolchains in this repository. Depending on the device, a specific toolchain is needed.
- `n705`, `n905b`, `n905c`, `n613`: `TOOLCHAINDIR="${GITDIR}/toolchain/gcc-4.8"` | You'll need to install 32-bit libraries on your system for it to work (unless you're on a 32-bit machine or VM, of course).<br>
- `n236`, `n437`, `n306`, `n306c`, `n873`: `TOOLCHAINDIR="${GITDIR}/toolchain/arm-nickel-linux-gnueabihf"`
- `kt`: `TOOLCHAINDIR="${GITDIR}/toolchain/gcc-4.4.1"`
- `n249`: `TOOLCHAINDIR="${GITDIR}/toolchain/armv7l-linux-musleabihf-cross/"`

<br><br>To compile a kernel for a device in particular (here, the Kobo Clara HD), use, for example:<br>
```
env PATH=/usr/bin GITDIR=/home/build/inkbox/kernel TOOLCHAINDIR=/home/build/inkbox/compiled-binaries/armv7l-linux-musleabihf/ INIT_GCC=/home/build/inkbox/compiled-binaries/armv7l-linux-musleabihf/bin/armv7l-linux-musleabihf-gcc INIT_STRIP=/home/build/inkbox/compiled-binaries/armv7l-linux-musleabihf/bin/armv7l-linux-musleabihf-strip THREADS=32 TARGET=armv7l-linux-musleabihf scripts/build_kernel.sh n249 root
```
and change the relevant environment variables as needed.<br><br>
## Additional notes:
- The `INIT_GCC` and `INIT_STRIP` variables should always point to an `armv7l` musl toolchain.
- Don't forget to symlink the kernel repository to `/home/build/inkbox/kernel` or create the repository in that location. The path is fixed.
- If anything goes wrong, consider using a Debian container. You will need to install the following packages:
```
sudo apt install u-boot-tools make gcc xz-utils
```

Notes:
- If a device has variants, remember to `git reset --hard` before building.
- To create the `modules.sqsh` squashfs archive: `mksquashfs dir new_modules.sqsh -b 1048576 -comp gzip -always-use-fragments`
