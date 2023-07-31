#pragma once
/*
** Copyright (c) 2001-2016, Erik de Castro Lopo <erikd@mega-nerd.com>
** All rights reserved.
**
** This code is released under 2-clause BSD license. Please see the
** file at : https://github.com/erikd/libsamplerate/blob/master/COPYING
*/

/* Version 1.5 */

/*============================================================================
**	On Intel Pentium processors (especially PIII and probably P4), converting
**	from float to int is very slow. To meet the C specs, the code produced by
**	most C compilers targeting Pentium needs to change the FPU rounding mode
**	before the float to int conversion is performed.
**
**	Changing the FPU rounding mode causes the FPU pipeline to be flushed. It
**	is this flushing of the pipeline which is so slow.
**
**	Fortunately the ISO C99 specifications define the functions lrint, lrintf,
**	llrint and llrintf which fix this problem as a side effect.
**
**	On Unix-like systems, the configure process should have detected the
**	presence of these functions. If they weren't found we have to replace them
**	here with a standard C cast.
*/

/*
**	The C99 prototypes for lrint and lrintf are as follows:
**
**		long int lrintf (float x) ;
**		long int lrint  (double x) ;
*/

#include "config.h"

/*
**	The presence of the required functions are detected during the configure
**	process and the values HAVE_LRINT and HAVE_LRINTF are set accordingly in
**	the config.h file.
*/

#define		HAVE_LRINT_REPLACEMENT	0

#if !defined(HAVE_LRINT)
#define HAVE_LRINT 0
#endif

#if !defined(HAVE_LRINTF)
#define HAVE_LRINTF 0
#endif

#if (HAVE_LRINT && HAVE_LRINTF)

	/*
	**	These defines enable functionality introduced with the 1999 ISO C
	**	standard. They must be defined before the inclusion of math.h to
	**	engage them. If optimisation is enabled, these functions will be
	**	inlined. With optimisation switched off, you have to link in the
	**	maths library using -lm.
	*/

	#define	_ISOC9X_SOURCE	1
	#define _ISOC99_SOURCE	1

	#define	__USE_ISOC9X	1
	#define	__USE_ISOC99	1

	#include	<math.h>

#else
	#define	lrint(dbl)		((long) (dbl))
	#define	lrintf(flt)		((long) (flt))
#endif


