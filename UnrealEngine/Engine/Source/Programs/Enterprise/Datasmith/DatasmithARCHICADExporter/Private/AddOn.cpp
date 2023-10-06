// Copyright Epic Games, Inc. All Rights Reserved.

// ---------------------------------- Includes ---------------------------------

#include <stddef.h>

#include "Utils/APIEnvir.h"
#include "Utils/TaskMgr.h"
#include "Utils/TaskCalledFromEventLoop.h"

#include "IDatasmithSceneElements.h"
#include "DatasmithSceneFactory.h"

#undef PI

#include "Utils/DebugTools.h"
#include "LoadDatasmithDlls.h"
#include "Export.h"
#include "Menus.h"
#include "Palette.h"
#include "ProjectEvent.h"
#include "Utils/ViewEvent.h"
#include "ElementEvent.h"
#include "ResourcesIDs.h"
#include "Synchronizer.h"
#include "ReportWindow.h"
#if PLATFORM_MAC
	#include "ConnectionWindow.h"
#endif

using namespace UE_AC;

// =============================================================================
// Required functions
// =============================================================================

// -----------------------------------------------------------------------------
// Dependency definitions
// -----------------------------------------------------------------------------

API_AddonType __ACENV_CALL CheckEnvironment(API_EnvirParams* envir)
{
	UE_AC_TraceF("-> UE_AC CheckEnvironment\n");

	short		  IdDescription = LocalizeResId(kStrListSyncPlugInDescription);
	GS::UniString versStr("\n\t");
	versStr += GetAddonVersionsStr();
	RSGetIndString(&envir->addOnInfo.name, IdDescription, 1, ACAPI_GetOwnResModule());
#ifdef DEBUG
	envir->addOnInfo.name += " d";
#endif
	RSGetIndString(&envir->addOnInfo.description, IdDescription, 2, ACAPI_GetOwnResModule());
	envir->addOnInfo.description += versStr;

	UE_AC_TraceF("<- UE_AC CheckEnvironment\n");

	return APIAddon_Preload;
}

// -----------------------------------------------------------------------------
// Interface definitions
// -----------------------------------------------------------------------------

GSErrCode __ACENV_CALL RegisterInterface(void)
{
	UE_AC_TraceF("-> UE_AC RegisterInterface\n");

	GSErrCode GSErr = FExport::Register();
	if (GSErr == NoError)
	{
		GSErr = FMenus::Register();
	}
	if (GSErr == NoError)
	{
		GSErr = FSynchronizer::Register();
	}
	if (GSErr == NoError)
	{
		GSErr = FTaskCalledFromEventLoop::Register();
	}

	ACAPI_KeepInMemory(true);

	UE_AC_TraceF("<- UE_AC RegisterInterface\n");

	return GSErr;
}

// -----------------------------------------------------------------------------
// Called when the Add-On has been loaded into memory
// to perform an operation
// -----------------------------------------------------------------------------

GSErrCode __ACENV_CALL Initialize(void)
{
	UE_AC_TraceF("-> UE_AC Initialize\n");

	LoadDatasmithDlls();
	FTraceListener::Get();

	GSErrCode GSErr = FExport::Initialize();
	if (GSErr == NoError)
	{
		GSErr = FMenus::Initialize();
	}
	if (GSErr == NoError)
	{
		GSErr = FSynchronizer::Initialize();
	}
	if (GSErr == NoError)
	{
		GSErr = FProjectEvent::Initialize();
	}
	if (GSErr == NoError)
	{
		GSErr = FViewEvent::Initialize();
	}
	if (GSErr == NoError)
	{
		GSErr = FElementEvent::Initialize();
	}
	if (GSErr == NoError)
	{
		GSErr = FTaskCalledFromEventLoop::Initialize();
	}

	FPalette::Register();

	ACAPI_KeepInMemory(true);

	UE_AC_TraceF("<- UE_AC Initialize\n");

	return GSErr;
}

// -----------------------------------------------------------------------------
// FreeData
//		called when the Add-On is going to be unloaded
// -----------------------------------------------------------------------------

GSErrCode __ACENV_CALL FreeData(void)
{
	UE_AC_TraceF("-> UE_AC FreeData\n");

	FTaskCalledFromEventLoop::Uninitialize();
#if PLATFORM_MAC
	FConnectionWindow::DeleteWindow();
#endif
	FPalette::Delete();
	FPalette::Unregister();
	FSynchronizer::DeleteSingleton();
	UnloadDatasmithDlls(true);
	FReportWindow::Delete();
	FTraceListener::Delete();
	FTaskMgr::DeleteMgr();

	UE_AC_TraceF("<- UE_AC FreeData\n");

	return NoError;
}
