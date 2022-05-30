/*
	wwan_ctrl.h
	
	Copyright (C) 2011 Sony Corporation
	
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License, version 2, as
	published by the Free Software Foundation.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA 
*/

#ifndef _LINUX_WWAN_CTRL_H
#define _LINUX_WWAN_CTRL_H

/* ioctrl */
#define WWAN_IOCTL_SET_W_DISABLE_H	  _IOW( 'W',1,int )
#define WWAN_IOCTL_SET_W_DISABLE_L	  _IOW( 'W',2,int )
#define WWAN_IOCTL_SET_POWER_ON		  _IOW( 'W',3,int )
#define WWAN_IOCTL_SET_POWER_OFF	  _IOW( 'W',4,int )

#endif /* _LINUX_WWAN_CTRL_H */
