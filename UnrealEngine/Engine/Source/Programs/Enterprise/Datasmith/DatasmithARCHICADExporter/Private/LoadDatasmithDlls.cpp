// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoadDatasmithDlls.h"

#include <stddef.h>
#include "Utils/APIEnvir.h"

#include "IDatasmithSceneElements.h"

#include "DatasmithSceneFactory.h"
#include "DatasmithExporterManager.h"
#include "DatasmithDirectLink.h"

#include "Utils/DebugTools.h"

#if PLATFORM_WINDOWS
	#pragma warning(push)
	#pragma warning(disable : 4800)
	#pragma warning(disable : 4505)
#endif

#include <string>
#include <cstdarg>

#if PLATFORM_WINDOWS
	#pragma warning(pop)
#endif

#include "Utils/LocalizeTools.h"
#include "Utils/AddonTools.h"

BEGIN_NAMESPACE_UE_AC

static bool	   bLoadSucceed = false;
static int32_t LoadRequestCount = 0;

bool LoadDatasmithDlls()
{
	if (LoadRequestCount++ == 0)
	{
		UE_AC_TraceF("Addon %s\n", GetAddonVersionsStr().ToUtf8());

		// Get the addon location
		IO::Location ApxLoc;
		UE_AC_TestGSError(ACAPI_GetOwnLocation(&ApxLoc));

		GS::UniString PluginsPath;
		UE_AC_TestGSError(ApxLoc.ToPath(&PluginsPath));
		UE_AC_TraceF("Addon path=\"%s\"\n", PluginsPath.ToUtf8());

#if PLATFORM_WINDOWS
		// On Windows Datasmith dll are placed in the same directory than the addon file (.apx)
		UE_AC_TestGSError(ApxLoc.DeleteLastLocalName());
		UE_AC_TestGSError(ApxLoc.ToPath(&PluginsPath));

		// On Windows we delay loading of Datasmith dll, so we must set dll directory before 1st call to dll functions
		wchar_t PreviousDllDirectory[MAX_PATH];
		DWORD	l =
			GetDllDirectoryW(sizeof(PreviousDllDirectory) / sizeof(PreviousDllDirectory[0]), PreviousDllDirectory);
		UE_AC_Assert(l >= 0 && l < sizeof(PreviousDllDirectory) / sizeof(PreviousDllDirectory[0]));
		UE_AC_Assert(SetDllDirectoryW((LPCWSTR)PluginsPath.ToUStr().Get()) != 0);
#else
		// On macOS dylib can be specified, so no need to setup path before the 1st call
#endif
		UE_AC_TraceF("Try to load Datasmith dlls\n");
		{
			// 0
			FDatasmithExporterManager::FInitOptions Options;
			Options.bEnableMessaging = true; // DirectLink requires the Messaging service.
			Options.bSuppressLogs = UE_AC_VERBOSEF_ON == 0; // Log are useful, Unhapily there no Log level
#if PLATFORM_WINDOWS
			Options.bUseDatasmithExporterUI = true;
			Options.RemoteEngineDirPath = TEXT("C:\\ProgramData\\Epic\\Exporter\\ArchicadEngine\\");
#else
			Options.bUseDatasmithExporterUI = false; // Currently DatasmithExporter Slate UI is not supported on macOS
			Options.RemoteEngineDirPath = TEXT("/Library/Application Support/Epic/Exporter/ArchicadEngine/");
#endif
			if (!FDatasmithExporterManager::Initialize(Options))
			{
				UE_AC_DebugF("FDatasmithExporterManager Initialize returned false\n");
				return false;
			}
			if (int32 ErrorCode = FDatasmithDirectLink::ValidateCommunicationSetup())
			{
				UE_AC_DebugF("FDatasmithDirectLink::ValidateCommunicationSetup ErrorCode=%d\n", ErrorCode);
				return false;
			}
		}
		bLoadSucceed = true;
		UE_AC_TraceF("Succeed to load Datasmith dlls\n");
#if PLATFORM_WINDOWS
		UE_AC_Assert(SetDllDirectoryW(PreviousDllDirectory));
#endif
	}

	return bLoadSucceed;
}

void UnloadDatasmithDlls(bool bForce)
{
	--LoadRequestCount;
	if (bLoadSucceed)
	{
		if (LoadRequestCount == 0 || bForce)
		{
			if (LoadRequestCount != 0)
			{
				UE_AC_DebugF("Mismatch Datasmith Load Unload count %d\n", LoadRequestCount);
			}

			UE_AC_TraceF("UnloadDatasmithDlls - FDatasmithDirectLink::Shutdown\n");
			FDatasmithDirectLink::Shutdown();
			UE_AC_TraceF("UnloadDatasmithDlls - FDatasmithExporterManager::Shutdown\n");
			FDatasmithExporterManager::Shutdown();

			// the call to FDatasmithExporterManager::Shutdown() is not sufficient.
			// We have to make sure IsEngineExitRequested() is true before the dll is unloaded as some static destructors rely on that flag to behave properly.
			//
			// The specific case handled here:
			//     FSparseDelegateStorage::SparseDelegateObjectListener dtr uses a maybe-invalid critical-section,
			//     and rely on IsEngineExitRequested() to behave correctly. We enforce that when the dll unloads
			if (!IsEngineExitRequested())
			{
				RequestEngineExit(TEXT("DLL_PROCESS_DETACH received"));
			}
			bLoadSucceed = false;
		}
	}
	UE_AC_TraceF("UnloadDatasmithDlls - Done\n");
}

END_NAMESPACE_UE_AC
