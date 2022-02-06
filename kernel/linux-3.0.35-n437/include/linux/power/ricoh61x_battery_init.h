/*
 * include/linux/power/ricoh61x_battery_init.h
 *
 * Battery initial parameter for RICOH RN5T618/619 power management chip.
 *
 * Copyright (C) 2013 RICOH COMPANY,LTD
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */
#ifndef __LINUX_POWER_RICOH61X_BATTERY_INIT_H
#define __LINUX_POWER_RICOH61X_BATTERY_INIT_H


uint8_t battery_init_para[][32] = {
        {
        0x0B, 0x74, 0x0B, 0xCC, 0x0B, 0xFA, 0x0C, 0x0E, 0x0C, 0x1F, 0x0C, 0x37, 0x0C, 0x69, 0x0C, 0x9C,
        0x0C, 0xD0, 0x0D, 0x10, 0x0D, 0x62, 0x05, 0xE6, 0x00, 0x59, 0x0F, 0xC8, 0x05, 0x2C, 0x22, 0x56
        }
};
#endif


