KSRC_DIR="$(pwd)"
obj_dir=""

OBJDIR_CFG="objdir.cfg"


#
# check output obj dir ...
#
function get_obj_dir() {
	if [ -f "${OBJDIR_CFG}" ];then
		obj_dir="$(cat ${OBJDIR_CFG})"
	else
		obj_dir="$1"
		#obj_dir="${HOME}/out/$(basename $KSRC_DIR)"
	fi


	if [ "${obj_dir}" ];then
		if [ -d "${obj_dir}" ];then
			echo -n ""
		else
			mkdir -p "${obj_dir}"
			[ $? != 0 ] && echo "create dir \"${obj_dir}\" fail !" && return 1 
		fi
	fi
	echo "${obj_dir}"
	return 0
}


function setup_platform() {
	_PLATFORM_NAME="$1"


	case $_PLATFORM_NAME in
		mx35 )
			platform_arch="arm"
			platform_cross="arm-none-eabi-"
			;;
		mx6 )
			platform_arch="arm"
			platform_cross="/opt/freescale/usr/local/gcc-4.6.2-glibc-2.13-linaro-multilib-2011.12/fsl-linaro-toolchain/bin/arm-fsl-linux-gnueabi-"
			;;
		mx50 )
			platform_arch="arm"
			platform_cross="/opt/freescale/usr/local/gcc-4.4.4-glibc-2.11.1-multilib-1.0/arm-fsl-linux-gnueabi/bin/arm-fsl-linux-gnueabi-"
			;;
		jz )
			platform_arch="mips"
			platform_cross="mipsel-linux-"
			;;
		m166e )
			platform_arch="arm"
			platform_cross="arm-marvell-linux-gnueabi-"
			;;
		mx35-linux )
			platform_arch="arm"
			platform_cross="arm-none-linux-gnueabi-"
			;;
		mx50-linux )
			platform_arch="arm"
			platform_cross="/opt/freescale/usr/local/gcc-4.4.4-glibc-2.11.1-multilib-1.0/arm-fsl-linux-gnueabi/bin/arm-fsl-linux-gnueabi-"
			;;
		mx6-linux )
			platform_arch="arm"
			platform_cross="/opt/freescale/usr/local/gcc-4.6.2-glibc-2.13-linaro-multilib-2011.12/fsl-linaro-toolchain/bin/arm-fsl-linux-gnueabi-"
			;;
		x86 )
			platform_arch=
			platform_cross=
			;;
		* )
			platform_arch=
			platform_cross=
			;;
	esac
	return 0
}

