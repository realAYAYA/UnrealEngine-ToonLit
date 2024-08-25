// Copyright Epic Games, Inc. All Rights Reserved.

#include "BuildSettings.h"

namespace BuildSettings
{
	bool IsLicenseeVersion()
	{
		return ENGINE_IS_LICENSEE_VERSION;
	}

	int GetEngineVersionMajor()
	{
		return ENGINE_VERSION_MAJOR;
	}

	int GetEngineVersionMinor()
	{
		return ENGINE_VERSION_MINOR;
	}

	int GetEngineVersionHotfix()
	{
		return ENGINE_VERSION_HOTFIX;
	}

	const TCHAR* GetEngineVersionString()
	{
		return TEXT(ENGINE_VERSION_STRING);
	}

	int GetCurrentChangelist()
	{
		return CURRENT_CHANGELIST;
	}

	int GetCompatibleChangelist()
	{
		return COMPATIBLE_CHANGELIST;
	}

	const TCHAR* GetBranchName()
	{
		return TEXT(BRANCH_NAME);
	}
	
	const TCHAR* GetBuildDate()
	{
		return TEXT(__DATE__);
	}

	const TCHAR* GetBuildTime()
	{
		return TEXT(__TIME__);
	}

	const TCHAR* GetBuildVersion()
	{
		return TEXT(BUILD_VERSION);
	}

	bool IsPromotedBuild()
	{
		return ENGINE_IS_PROMOTED_BUILD;
	}
	
	bool IsWithDebugInfo()
	{
		return UE_WITH_DEBUG_INFO;
	}
	
	const TCHAR* GetBuildURL()
	{
		return TEXT(BUILD_SOURCE_URL);
	}
}


