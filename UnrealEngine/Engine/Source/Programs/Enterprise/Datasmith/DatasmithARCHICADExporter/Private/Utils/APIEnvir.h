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
#include "TranMatToTransformationConverter.hpp"
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

#if AC_VERSION > 26
// Declarations of methods used by the AddOn and defined in ACAPI_MigrationHeader.h
GSErrCode ACAPI_3D_CreateSight (void** newSightPtr);
GSErrCode ACAPI_3D_GetComponent (API_Component3D* component);
GSErrCode ACAPI_3D_GetCurrentWindowSight (void** sightPtr);
GSErrCode ACAPI_3D_SelectSight (void* sight, void** oldSightPtr);
GSErrCode ACAPI_Command_CallFromEventLoop (const API_ModulID* mdid, GSType cmdID, Int32 cmdVersion, GSHandle paramsHandle, bool silentMode, APICommandCallBackProc* callbackProc);
GSErrCode ACAPI_Database (API_DatabaseID code, void* par1 = nullptr, void* par2 = nullptr, void* par3 = nullptr);
GSErrCode ACAPI_Database_GetLast3DDefLevels (const GS::Array<API_ElemType>& elemTypes, GS::Array<double>& levels);
GSErrCode ACAPI_ElemComponent_GetPropertyDefinitions (const API_ElemComponentID& elemComponent, API_PropertyDefinitionFilter filter, GS::Array<API_PropertyDefinition>& propertyDefinitions);
GSErrCode ACAPI_ElemComponent_GetPropertyValues (const API_ElemComponentID& elemComponent, const GS::Array<API_PropertyDefinition>& propertyDefinitions, GS::Array<API_Property>& properties);
GSErrCode ACAPI_ElementGroup_GetGroup (const API_Guid& elemGuid, API_Guid* groupGuid);
GSErrCode ACAPI_Element_Get3DInfo (const API_Elem_Head& elemHead, API_ElemInfo3D* info3D);
GSErrCode ACAPI_Element_GetCategoryValue (const API_Guid& elemGuid, const API_ElemCategory& elemCategory, API_ElemCategoryValue* elemCategoryValue);
GSErrCode ACAPI_Element_GetConnectedElements (const API_Guid& guid, const API_ElemType& connectedElemType, GS::Array<API_Guid>* connectedElements, API_ElemFilterFlags filterBits = APIFilt_None, const API_Guid& renovationFilterGuid = APINULLGuid);
GSErrCode ACAPI_Environment (API_EnvironmentID code, void* par1 = nullptr, void* par2 = nullptr, void* par3 = nullptr);
GSErrCode ACAPI_Goodies (API_GoodiesID code, void* par1 = nullptr, void* par2 = nullptr, void* par3 = nullptr, void* par4 = nullptr);
GSErrCode ACAPI_Install_FileTypeHandler3D (GSType cmdID, APIIO3DCommandProc* handlerProc);
GSErrCode ACAPI_Install_MenuHandler (short menuStrResID, APIMenuCommandProc* handlerProc);
GSErrCode ACAPI_Install_ModulCommandHandler (GSType cmdID, Int32 cmdVersion, APIModulCommandProc* handlerProc);
GSErrCode ACAPI_Interface (API_InterfaceID code, void* par1 = nullptr, void* par2 = nullptr, void* par3 = nullptr, void* par4 = nullptr, void* par5 = nullptr);
GSErrCode ACAPI_LibPart_Get (API_LibPart* libPart);
GSErrCode ACAPI_LibPart_GetNum (Int32* count);
GSErrCode ACAPI_LibPart_GetSection (Int32 libInd, API_LibPartSection* section, GSHandle* sectionHdl, GS::UniString* sectionStr, GS::UniString* password = nullptr);
GSErrCode ACAPI_LibPart_GetSectionList (Int32 libInd, Int32* nSection, API_LibPartSection*** sections);
GSErrCode ACAPI_LibPart_Search (API_LibPart* ancestor, bool createIfMissing, bool onlyPlaceable = false);
GSErrCode ACAPI_ListData_Get (API_ListData* listdata);
GSErrCode ACAPI_ListData_GetLocal (Int32 libIndex, const API_Elem_Head* elemHead, API_ListData* listdata);
GSErrCode ACAPI_Notify_CatchNewElement (const API_ToolBoxItem* elemType, APIElementEventHandlerProc* handlerProc);
GSErrCode ACAPI_Notify_CatchProjectEvent (GSFlags eventTypes, APIProjectEventHandlerProc* handlerProc);
GSErrCode ACAPI_Notify_CatchViewEvent (GSFlags eventTypes, API_NavigatorMapID mapId, APIViewEventHandlerProc* handlerProc);
GSErrCode ACAPI_Notify_InstallElementObserver (APIElementEventHandlerProc* handlerProc);
GSErrCode ACAPI_Register_FileType (Int32 refCon, GSType ftype, GSType fcreator, const char* extname, short iconResID, short descStrResID, short descStrResItemID, API_IOMethod methodFlags);
GSErrCode ACAPI_Register_Menu (short menuStrResID, short promptStrResID, APIMenuCodeID menuPosCode, GSFlags menuFlags);
GSErrCode ACAPI_Register_SupportedService (GSType cmdID, Int32 cmdVersion);
#endif

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

