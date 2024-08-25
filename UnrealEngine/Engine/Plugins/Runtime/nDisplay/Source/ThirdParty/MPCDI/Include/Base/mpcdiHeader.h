/* =========================================================================

  Program:   MPCDI Library
  Language:  C++
  Date:      $Date: 2012-02-08 11:39:41 -0500 (Wed, 08 Feb 2012) $
  Version:   $Revision: 18341 $

  Copyright (c) 2013 Scalable Display Technologies, Inc.
  All Rights Reserved.
  The MPCDI Library is distributed under the BSD license.
  Please see License.txt distributed with this package.

===================================================================auto== */

#pragma once

#ifndef __mcpcdiHeader_H_
#define __mcpcdiHeader_H_

//
// Windows and Exports
//

#if defined(_WIN32) || defined(WIN32)
// removes lots of stuff from Windows header file
#  define WIN32_LEAN_AND_MEAN
// remove min and max macros, unless we need them.

// define strict header for windows
#  ifndef STRICT
#    define STRICT
#  endif

// Not sure that we needs 
// #  include <windows.h>
#  define mcpcdiWIN_OS 1
#endif

#if (mcpcdiWIN_OS) && !defined(MPCDI_STATIC) 
# ifdef MPCDI_EXPORTS
#  define EXPORT_MPCDI __declspec( dllexport )
# else
#  define EXPORT_MPCDI __declspec( dllimport )
# endif  // mcpcdi_EXPORTS
#else
// unix needs nothing
#define EXPORT_MPCDI
#endif

//
// Turn off annoying warnings
//

#pragma warning(disable: 4201) // "nonstandard extension used: nameless struct/union
#pragma warning(disable: 4482) // "nostandard extension used: enum

// Windows the sprintf and related warnings.
#ifndef _CRT_SECURE_NO_WARNINGS
#  define _CRT_SECURE_NO_WARNINGS
#endif

// sw printfs
#ifndef _CRT_NON_CONFORMING_SWPRINTFS
#  define _CRT_NON_CONFORMING_SWPRINTFS
#endif

#endif // __mcpcdiHeader_H_
