/* This is a generated file, don't edit */

#define NUM_APPLETS 93
#define KNOWN_APPNAME_OFFSETS 4

const uint16_t applet_nameofs[] ALIGN2 = {
123,
279,
407,
};

const char applet_names[] ALIGN1 = ""
"[" "\0"
"[[" "\0"
"ash" "\0"
"awk" "\0"
"base64" "\0"
"cat" "\0"
"chattr" "\0"
"chgrp" "\0"
"chmod" "\0"
"chown" "\0"
"chroot" "\0"
"cp" "\0"
"cttyhack" "\0"
"cut" "\0"
"date" "\0"
"dd" "\0"
"devmem" "\0"
"dmesg" "\0"
"echo" "\0"
"env" "\0"
"false" "\0"
"fbset" "\0"
"fdformat" "\0"
"fold" "\0"
"free" "\0"
"fsck" "\0"
"fuser" "\0"
"getty" "\0"
"grep" "\0"
"halt" "\0"
"i2cdetect" "\0"
"i2cdump" "\0"
"i2cget" "\0"
"i2cset" "\0"
"i2ctransfer" "\0"
"ifconfig" "\0"
"ifdown" "\0"
"ifenslave" "\0"
"ifplugd" "\0"
"ifup" "\0"
"init" "\0"
"insmod" "\0"
"kill" "\0"
"killall" "\0"
"linuxrc" "\0"
"ln" "\0"
"login" "\0"
"losetup" "\0"
"ls" "\0"
"lsattr" "\0"
"lsmod" "\0"
"lsof" "\0"
"mkdir" "\0"
"mkfifo" "\0"
"mknod" "\0"
"mount" "\0"
"mv" "\0"
"nc" "\0"
"nohup" "\0"
"nproc" "\0"
"pidof" "\0"
"ping" "\0"
"poweroff" "\0"
"printf" "\0"
"ps" "\0"
"pwd" "\0"
"reboot" "\0"
"reset" "\0"
"rm" "\0"
"rmmod" "\0"
"route" "\0"
"run-init" "\0"
"rx" "\0"
"sed" "\0"
"sh" "\0"
"sha256sum" "\0"
"sleep" "\0"
"sort" "\0"
"split" "\0"
"stat" "\0"
"sync" "\0"
"test" "\0"
"timeout" "\0"
"touch" "\0"
"tr" "\0"
"true" "\0"
"umount" "\0"
"uname" "\0"
"unlink" "\0"
"vi" "\0"
"watchdog" "\0"
"xxd" "\0"
"yes" "\0"
;

#define APPLET_NO_ash 2
#define APPLET_NO_awk 3
#define APPLET_NO_base64 4
#define APPLET_NO_cat 5
#define APPLET_NO_chattr 6
#define APPLET_NO_chgrp 7
#define APPLET_NO_chmod 8
#define APPLET_NO_chown 9
#define APPLET_NO_chroot 10
#define APPLET_NO_cp 11
#define APPLET_NO_cttyhack 12
#define APPLET_NO_cut 13
#define APPLET_NO_date 14
#define APPLET_NO_dd 15
#define APPLET_NO_devmem 16
#define APPLET_NO_dmesg 17
#define APPLET_NO_echo 18
#define APPLET_NO_env 19
#define APPLET_NO_false 20
#define APPLET_NO_fbset 21
#define APPLET_NO_fdformat 22
#define APPLET_NO_fold 23
#define APPLET_NO_free 24
#define APPLET_NO_fsck 25
#define APPLET_NO_fuser 26
#define APPLET_NO_getty 27
#define APPLET_NO_grep 28
#define APPLET_NO_halt 29
#define APPLET_NO_i2cdetect 30
#define APPLET_NO_i2cdump 31
#define APPLET_NO_i2cget 32
#define APPLET_NO_i2cset 33
#define APPLET_NO_i2ctransfer 34
#define APPLET_NO_ifconfig 35
#define APPLET_NO_ifdown 36
#define APPLET_NO_ifenslave 37
#define APPLET_NO_ifplugd 38
#define APPLET_NO_ifup 39
#define APPLET_NO_init 40
#define APPLET_NO_insmod 41
#define APPLET_NO_kill 42
#define APPLET_NO_killall 43
#define APPLET_NO_linuxrc 44
#define APPLET_NO_ln 45
#define APPLET_NO_login 46
#define APPLET_NO_losetup 47
#define APPLET_NO_ls 48
#define APPLET_NO_lsattr 49
#define APPLET_NO_lsmod 50
#define APPLET_NO_lsof 51
#define APPLET_NO_mkdir 52
#define APPLET_NO_mkfifo 53
#define APPLET_NO_mknod 54
#define APPLET_NO_mount 55
#define APPLET_NO_mv 56
#define APPLET_NO_nc 57
#define APPLET_NO_nohup 58
#define APPLET_NO_nproc 59
#define APPLET_NO_pidof 60
#define APPLET_NO_ping 61
#define APPLET_NO_poweroff 62
#define APPLET_NO_printf 63
#define APPLET_NO_ps 64
#define APPLET_NO_pwd 65
#define APPLET_NO_reboot 66
#define APPLET_NO_reset 67
#define APPLET_NO_rm 68
#define APPLET_NO_rmmod 69
#define APPLET_NO_route 70
#define APPLET_NO_rx 72
#define APPLET_NO_sed 73
#define APPLET_NO_sh 74
#define APPLET_NO_sha256sum 75
#define APPLET_NO_sleep 76
#define APPLET_NO_sort 77
#define APPLET_NO_split 78
#define APPLET_NO_stat 79
#define APPLET_NO_sync 80
#define APPLET_NO_test 81
#define APPLET_NO_timeout 82
#define APPLET_NO_touch 83
#define APPLET_NO_tr 84
#define APPLET_NO_true 85
#define APPLET_NO_umount 86
#define APPLET_NO_uname 87
#define APPLET_NO_unlink 88
#define APPLET_NO_vi 89
#define APPLET_NO_watchdog 90
#define APPLET_NO_xxd 91
#define APPLET_NO_yes 92

#ifndef SKIP_applet_main
int (*const applet_main[])(int argc, char **argv) = {
test_main,
test_main,
ash_main,
awk_main,
baseNUM_main,
cat_main,
chattr_main,
chgrp_main,
chmod_main,
chown_main,
chroot_main,
cp_main,
cttyhack_main,
cut_main,
date_main,
dd_main,
devmem_main,
dmesg_main,
echo_main,
env_main,
false_main,
fbset_main,
fdformat_main,
fold_main,
free_main,
fsck_main,
fuser_main,
getty_main,
grep_main,
halt_main,
i2cdetect_main,
i2cdump_main,
i2cget_main,
i2cset_main,
i2ctransfer_main,
ifconfig_main,
ifupdown_main,
ifenslave_main,
ifplugd_main,
ifupdown_main,
init_main,
modprobe_main,
kill_main,
kill_main,
init_main,
ln_main,
login_main,
losetup_main,
ls_main,
lsattr_main,
lsmod_main,
lsof_main,
mkdir_main,
mkfifo_main,
mknod_main,
mount_main,
mv_main,
nc_main,
nohup_main,
nproc_main,
pidof_main,
ping_main,
halt_main,
printf_main,
ps_main,
pwd_main,
halt_main,
reset_main,
rm_main,
modprobe_main,
route_main,
switch_root_main,
rx_main,
sed_main,
ash_main,
md5_sha1_sum_main,
sleep_main,
sort_main,
split_main,
stat_main,
sync_main,
test_main,
timeout_main,
touch_main,
tr_main,
true_main,
umount_main,
uname_main,
unlink_main,
vi_main,
watchdog_main,
xxd_main,
yes_main,
};
#endif

const uint8_t applet_suid[] ALIGN1 = {
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x20,
0x00,
0x00,
0x00,
0x04,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
0x00,
};

const uint8_t applet_install_loc[] ALIGN1 = {
0x33,
0x31,
0x11,
0x11,
0x11,
0x14,
0x31,
0x11,
0x12,
0x31,
0x41,
0x34,
0x23,
0x23,
0x21,
0x44,
0x44,
0x24,
0x22,
0x24,
0x22,
0x31,
0x10,
0x21,
0x11,
0x32,
0x31,
0x11,
0x31,
0x33,
0x11,
0x32,
0x11,
0x32,
0x21,
0x22,
0x13,
0x31,
0x31,
0x13,
0x31,
0x13,
0x13,
0x11,
0x13,
0x32,
0x03,
};
