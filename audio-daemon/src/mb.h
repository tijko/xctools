/*
 * Copyright (c) 2012 Citrix Systems, Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _MB_H_
#define _MB_H_

#if defined(__i386__)
#define mb()  asm volatile ( "lock; addl $0,0(%%esp)" : : : "memory" )
#define rmb() asm volatile ( "lock; addl $0,0(%%esp)" : : : "memory" )
#define wmb() asm volatile ( "" : : : "memory")
#elif defined(__x86_64__)
#define mb()  asm volatile ( "mfence" : : : "memory")
#define rmb() asm volatile ( "lfence" : : : "memory")
#define wmb() asm volatile ( "" : : : "memory")
#else
#error "Unknow architecture"
#endif

#endif
