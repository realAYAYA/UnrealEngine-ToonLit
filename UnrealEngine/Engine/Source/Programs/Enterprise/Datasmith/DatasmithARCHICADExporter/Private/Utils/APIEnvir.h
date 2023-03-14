// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <stddef.h>

#include "WarningsDisabler.h"

DISABLE_SDK_WARNINGS_START

#include "IDatasmithSceneElements.h"
#include "DatasmithSceneFactory.h"

#undef PI

#include "Sight.hpp"
#if AC_VERSION < 26
#include "AttributeReader.hpp"
#endif
#include "Model.hpp"

DISABLE_SDK_WARNINGS_END

#if defined(_MSC_VER)
	#if !defined(WINDOWS)
		#define WINDOWS
	#endif
#endif

#if defined(WINDOWS)
	#include "Win32Interface.hpp"
#endif

#if PLATFORM_MAC
	#include <CoreServices/CoreServices.h>
#endif

#if !defined(ACExtension)
	#define ACExtension
#endif

#define BEGIN_NAMESPACE_UE_AC \
	namespace UE_AC           \
	{
#define END_NAMESPACE_UE_AC }

BEGIN_NAMESPACE_UE_AC

typedef char		utf8_t;
typedef std::string utf8_string;

END_NAMESPACE_UE_AC

#if PLATFORM_WINDOWS
	#define UE_AC_DirSep "\\"
	#define __printflike(fmtarg, firstvararg)
#else
	#define UE_AC_DirSep "/"
#endif

#define StrHelpLink "https://www.epicgames.com"

/* 0 - Right value UVEdit function
 * 1 - Make texture rotation and sizing compatible with Twinmotion */
#define PIVOT_0_5_0_5 0

#if AC_VERSION < 26
#define GET_HEADER_TYPEID(_header_) _header_.typeID
#else
#define GET_HEADER_TYPEID(_header_) _header_.type.typeID
#endif

