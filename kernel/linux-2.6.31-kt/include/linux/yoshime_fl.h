/*
 * 
 */
#ifndef __LINUX_YOSHIME_FL_H
#define __LINUX_YOSHIME_FL_H 

#define FL_DEV_FILE "/dev/yoshime_fl"

#define FL_MAGIC_NUMBER         'L'
#define FL_IOCTL_SET_INTENSITY  _IOW(FL_MAGIC_NUMBER, 0x01, int)
#define FL_IOCTL_GET_INTENSITY  _IOR(FL_MAGIC_NUMBER, 0x02, int)
#define FL_IOCTL_GET_RANGE_MAX  _IOR(FL_MAGIC_NUMBER, 0x03, int)
    
#endif

