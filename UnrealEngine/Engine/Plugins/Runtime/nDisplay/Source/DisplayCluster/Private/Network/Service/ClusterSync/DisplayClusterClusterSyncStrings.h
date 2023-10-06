// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


/**
 * Cluster synchronization messages
 */
namespace DisplayClusterClusterSyncStrings
{
	constexpr static const TCHAR* ProtocolName = TEXT("ClusterSync");
	
	constexpr static const TCHAR* TypeRequest  = TEXT("Request");
	constexpr static const TCHAR* TypeResponse = TEXT("Response");

	constexpr static const TCHAR* ArgumentsDefaultCategory = TEXT("CS");
	constexpr static const TCHAR* ArgumentsJsonEvents      = TEXT("CS_JE");
	constexpr static const TCHAR* ArgumentsBinaryEvents    = TEXT("CS_BE");

	namespace WaitForGameStart
	{
		constexpr static const TCHAR* Name = TEXT("WaitForGameStart");
	}

	namespace WaitForFrameStart
	{
		constexpr static const TCHAR* Name = TEXT("WaitForFrameStart");
	}

	namespace WaitForFrameEnd
	{
		constexpr static const TCHAR* Name = TEXT("WaitForFrameEnd");
	}

	namespace GetTimeData
	{
		constexpr static const TCHAR* Name = TEXT("GetTimeData");
		constexpr static const TCHAR* ArgDeltaTime        = TEXT("DeltaTime");
		constexpr static const TCHAR* ArgGameTime         = TEXT("GameTime");
		constexpr static const TCHAR* ArgIsFrameTimeValid = TEXT("IsFrameTimeValid");
		constexpr static const TCHAR* ArgFrameTime        = TEXT("FrameTime");
	}

	namespace GetObjectsData
	{
		constexpr static const TCHAR* Name = TEXT("GetObjectsData");
		constexpr static const TCHAR* ArgSyncGroup = TEXT("SyncGroup");
	}

	namespace GetEventsData
	{
		constexpr static const TCHAR* Name = TEXT("GetEventsData");
	}

	namespace GetNativeInputData
	{
		constexpr static const TCHAR* Name = TEXT("GetNativeInputData");
	}
};
