/*
 * Copyright (C) 2003 Apple Computer, Inc. All rights reserved.
 *
 * This file is part of the GNU LIBICONV Library.
 * Modified by Elmar Hanlhofer to be used with hfsprescue.
 *
 * The GNU LIBICONV Library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * The GNU LIBICONV Library is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the GNU LIBICONV Library; see the file COPYING.LIB.
 * If not, write to the Free Software Foundation, Inc., 59 Temple Place -
 * Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * UTF-8-MAC
 */

 /*
 	Includes Unicode 3.2 decomposition code derived from Core Foundation
 */


#ifndef _UTF8MAC_
#define _UTF8MAC_

#include "config.h"

#include <sys/types.h>
#include <errno.h>

#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif




#define	UTF_REVERSE_ENDIAN	0x01	/* reverse UCS-2 byte order */
#define	UTF_NO_NULL_TERM	0x02	/* do not add null termination */
#define	UTF_DECOMPOSED		0x04	/* generate fully decomposed UCS-2 */
#define	UTF_PRECOMPOSED		0x08	/* generate precomposed UCS-2 */

int	utf8_encodestr (const u_int16_t *, size_t, u_int8_t *, size_t *,
		size_t, u_int16_t, int);

int	utf8_decodestr (const u_int8_t *, size_t, u_int16_t *,size_t *,
		size_t, u_int16_t, int, size_t *);


#endif