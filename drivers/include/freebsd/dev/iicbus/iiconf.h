/* SPDX-License-Identifier: BSD-2-Clause */
/* dev/iicbus/iiconf.h shim — I2C error codes used by bcm2835_bsc.c */
#ifndef _DEV_IICBUS_IICONF_H_
#define _DEV_IICBUS_IICONF_H_

#define IIC_ENOADDR     0x01    /* no address supplied */
#define IIC_EBUSERR     0x02    /* bus error */
#define IIC_EBUSBSY     0x03    /* bus busy */
#define IIC_ETIMEOUT    0x04    /* transfer timed out */

#endif /* _DEV_IICBUS_IICONF_H_ */
