/* basename.c
 *
 * $Id$
 *
 * Provides an implementation of the "basename" function, conforming
 * to SUSv3, with extensions to accommodate Win32 drive designators,
 * and suitable for use on native Microsoft(R) Win32 platforms.
 *
 * Written by Keith Marshall <keithmarshall@users.sourceforge.net>
 *
 * This is free software.  You may redistribute and/or modify it as you
 * see fit, without restriction of copyright.
 *
 * This software is provided "as is", in the hope that it may be useful,
 * but WITHOUT WARRANTY OF ANY KIND, not even any implied warranty of
 * MERCHANTABILITY, nor of FITNESS FOR ANY PARTICULAR PURPOSE.  At no
 * time will the author accept any form of liability for any damages,
 * however caused, resulting from the use of this software.
 *
 */

#ifndef __BASENAME_H_
#define __BASENAME_H_

char * __cdecl basename(char *path);

#endif
