/* SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later */
#ifndef _COMPILER_H
#define _COMPILER_H

#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#define __attribute__(x)
#define __attribute(x)
#define MCTP_PACKED
#define __unused
#define alignof __alignof
#else
#define MCTP_PACKED __attribute__((packed))
#ifndef __unused
#define __unused __attribute__((unused))
#endif
#endif

#endif
