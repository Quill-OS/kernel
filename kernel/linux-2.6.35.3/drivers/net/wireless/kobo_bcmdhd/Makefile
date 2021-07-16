
ifeq ($(CONFIG_KOBO_BCM_DHD_DEBUG),y)
EXTRA_CFLAGS += -DDEBUG -DDHD_DEBUG -DSDTEST -DBCMDBG -DSRCBASE=\"$(obj)\"
endif

ifeq ($(CONFIG_KOBO_BCM_DHD_STATIC_BUF),y) 
EXTRA_CFLAGS += -DDHD_USE_STATIC_BUF
endif

include $(objtree)/.config

#####_____________________________________________________
#####
# diese zwei sind definitiv falsch für den Kobo Mini ntx508
# sie sind gesetzt in den Quellen, die von Kobolab-github stammen
# DFLAGS += -DCUSTOM_OOB_GPIO_NUM=173                                                                                                                                                                                        
# DFLAGS += -DOOB_INTR_ONLY                                                                                                                                                                                                  
#
# die folgenden Definitionen müssen noch hinzugefügt werden
# gefunden durch Vergleich der .cmd-Dateien nach dem Build 
DFLAGS += -DSDIO_ISR_THREAD
DFLAGS += -DDHDTHREAD
#
# gefunden durch den Vergleich der mit "nm" (rück-)generierten Symbol-Tabellen 
# von originalem und selbskompiliertem Modul
DFLAGS += -DOEM_CHROMIUMOS
#
# das musste ich noch selber reincodieren, das Original benutzt definitiv mutex_lock()
DFLAGS += -DCONFIG_DHD_MUTEXES=1 -DWITH_OBSOLETE_CODE=0                                       
#####__________________________________________________
#####
                                                                                                                                                                       
# basic options (defines in DFLAGS, includes in IFLAGS)                                                                                                                                                                    
# aus der original-Makefile
#
DFLAGS += -DLINUX -DBCMDRIVER -DBCMDONGLEHOST                                                                                                                                                     
DFLAGS += -DUNRELEASEDCHIP -DBCMDMA32                                                                                                                                                                                      
DFLAGS += -DBCMFILEIMAGE                                                                                                                                                                                                   
DFLAGS += -DCONFIG_WIRELESS_EXT

DFLAGS += -DWIFI_ACT_FRAME

# for dhd_linux_sched.c
DFLAGS += -DDHD_GPL -DDHD_SCHED

# for dhd_cdc.c
DFLAGS += -DBDC -DTOE 
DFLAGS += -DDHD_BCMEVENTS -DSHOW_EVENTS

# for dhd_sdio.c
DFLAGS += -DBCMSDIO -DBCMLXSDMMC -DBCMPLATFORM_BUS # ist doppelt -DDHD_GPL 

ifeq ($(CONFIG_MMC_MSM7X00A),y)
DFLAGS += -Dlinux
DFLAGS += -DDHD_SDALIGN=64 -DMAX_HDR_READ=64 -DDHD_FIRSTREAD=64
endif


#_______cfg80211________________________________________
#DFLAGS += -Dlinux
#DFLAGS += -DOEM_ANDROID
#DFLAGS += -DDHD_USE_STATIC_BUF
#DFLAGS += -DCONFIG_MACH_MAHIMAHI
#DFLAGS += -DCUSTOM_OOB_GPIO_NUM=299
#DFLAGS += -DOOB_INTR_ONLY
#DFLAGS += -DMMC_SDIO_ABORT
#ifeq ("$(LINUX_MINOR)", "35")
#$(warning Define HW_OOB for 2.6.35 kernel**)
#DFLAGS += -DHW_OOB
#endif
#CFILES += dhd_sdio.c 
#
#ifneq ($(findstring -cfg80211-,-$(TARGET)-),)
#ifndef COMPAT_WIRELESS
#COMPAT_WIRELESS=/projects/hnd/tools/linuxwireless/compat-wireless
#export COMPAT_WIRELESS
#endif

obj-$(CONFIG_KOBO_BCM_DHD) += dhd.o

dhd-objs := dhd_common.o \
	    dhd_cdc.o \
	    dhd_custom_gpio.o \
	    dhd_linux.o \
	    dhd_linux_sched.o \
	    dhd_sdio.o \
	    aiutils.o \
	    bcmevent.o \
	    bcmsdh.o \
	    bcmsdh_linux.o \
	    bcmsdh_sdmmc.o \
	    bcmsdh_sdmmc_linux.o \
	    bcmutils.o \
	    bcmwifi.o \
	    hndpmu.o \
	    linux_osl.o \
	    sbutils.o \
	    siutils.o \
	    wl_iw.o



ifeq ($(subst m,y,$(CONFIG_CFG80211)),y) 

ifeq ($(CPTCFG_BACKPORT_KERNEL_3_99),y)
ifeq ($(CPTCFG_KOBO_BCM_DHD_DEVELOPING),y)
# detect if compilation was started from backport drivers tree
DFLAGS += -DBACKPORT_KERNEL_DRIVER

DFLAGS += -DWL_CFG80211 
dhd-objs += wl_cfg80211.o

DFLAGS += -DWLP2P 
dhd-objs += wl_cfgp2p.o 
endif
else
# we are on standard 2.6.35.3 kernel for Kobo Mini ntx508

## wl_cfg80211.c will not compile on this kernel
## due to missing interface/older headers to the nl80211 stuff
## you will need the backport-kernel-driver environment
DFLAGS += -DWL_CFG80211 
dhd-objs += wl_cfg80211.o

ifeq ($(CONFIG_KOBO_BCM_DHD_WIFI_DIRECT),y)
# try what we can do with this, it says Wifi-direct support,
# seems to be useable for repeaters
DFLAGS += -DWLP2P 
dhd-objs += wl_cfgp2p.o 
endif

endif
endif


ccflags-y = -I$(obj)/include -I$(obj)/. $(DFLAGS)
