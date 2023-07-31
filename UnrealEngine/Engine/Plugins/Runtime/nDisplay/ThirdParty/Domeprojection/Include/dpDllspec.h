///
/// Copyright (C) 2018 domeprojection.com GmbH.
/// All rights reserved.
/// Contact: domeprojection.com GmbH (support@domeprojection.com)
///
/// This file is part of the dpLib.
///

#ifndef _DPDLLSPEC_H_
#define _DPDLLSPEC_H_

#ifdef DPLIB_EXPORTS
#define DPLIB_API __declspec(dllexport)
#else
#define DPLIB_API __declspec(dllimport)
#endif

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#define DPLIB_WINDOWS
#else
#define DPLIB_POSIX
#endif

#endif // _DPDLLSPEC_H_
