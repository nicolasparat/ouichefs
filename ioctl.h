/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _OUICHEFS_IOCTL_H
#define _OUICHEFS_IOCTL_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

#define OUICHEFS_IOC_MAGIC 'o'
#define OUICHEFS_IOC_GET_EXTENTS _IO(OUICHEFS_IOC_MAGIC, 1)

#endif /* _OUICHEFS_IOCTL_H */