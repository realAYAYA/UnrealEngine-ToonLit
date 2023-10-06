// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <stddef.h>

#include "IDatasmithSceneElements.h"
#include "DatasmithSceneFactory.h"

#undef PI

// clang-format off

THIRD_PARTY_INCLUDES_START

#ifdef __clang__

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcomma"
#pragma clang diagnostic ignored "-Wdefaulted-function-deleted"

#if (__clang_major__ > 12) || (__clang_major__ == 12 && __clang_minor__ == 0 && __clang_patchlevel__ > 4)
#pragma clang diagnostic ignored "-Wnon-c-typedef-for-linkage"
#endif

#elif PLATFORM_WINDOWS

__pragma(warning(push))
__pragma(warning(disable: 4005)) // 'TEXT': macro redefinition
__pragma(warning(disable: 5040)) // Support\Modules\GSRoot\vaarray.hpp: dynamic exception specifications are valid only in C++14 and earlier

#else

_Pragma("clang diagnostic ignored \"-Wcomma\"")
_Pragma("clang diagnostic ignored \"-Wdefaulted-function-deleted\"")

#endif

// clang-format on

#include "ACAPinc.h"
#include "Md5.hpp"

#include "Sight.hpp"
#if AC_VERSION < 26
#include "AttributeReader.hpp"
#else
#include "IAttributeReader.hpp"
#endif
#include "Model.hpp"

#include "ModelElement.hpp"
#include "ModelMeshBody.hpp"
#include "ConvexPolygon.hpp"

#include "Transformation.hpp"
#include "Parameter.hpp"
#include "Light.hpp"

#include "exp.h"
#include "FileSystem.hpp"

#include "Lock.hpp"

#include "DGModule.hpp"

#include "DGDialog.hpp"

#include "Transformation.hpp"
#include "Line3D.hpp"

#include "DGFileDlg.hpp"

// clang-format off
#ifdef __clang__
#pragma clang diagnostic pop
#elif PLATFORM_WINDOWS
__pragma(warning(pop))
#else
_Pragma("clang diagnostic pop")
#endif
// clang-format on

THIRD_PARTY_INCLUDES_END

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

