/*
 *	Copyright (C) 2014 Ovidiu Mara
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; version 2 is the only version of this
 *  license under which this program may be distributed.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef EMBEDFILE_H
#define EMBEDFILE_H

// USAGE: call BINDATA(name, file.txt) and access the char array with &name.

__asm__(
".altmacro\n" \
".macro binfile p q\n" \
"   .global \\p\n" \
"\\p:\n" \
"   .incbin \\q\n" \
"\\p&_end:\n" \
"   .byte 0\n" \
"   .global \\p&_length\n" \
"\\p&_length:\n" \
"   .int(\\p&_end - \\p)\n" \
".endm\n\t"
);

#ifdef __cplusplus
extern "C" {
#endif

#define BINDATA(n, s) \
__asm__("\n\n.data\n\tbinfile " #n " \"" #s "\"\n"); \
extern char n; \
extern int n##_length;

#ifdef __cplusplus
}
#endif

#endif // EMBEDFILE_H

