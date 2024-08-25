// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace DisplayClusterViewportStrings
{
	namespace prefix
	{
		static constexpr const TCHAR* projection = TEXT("proj");
	}

	// ICVFXstrings
	namespace icvfx
	{
		static constexpr const TCHAR* prefix       = TEXT("icvfx");
		static constexpr const TCHAR* camera       = TEXT("incamera");
		static constexpr const TCHAR* chromakey    = TEXT("chromakey");
		static constexpr const TCHAR* lightcard    = TEXT("lightcard");
		static constexpr const TCHAR* uv_lightcard = TEXT("uv_lightcard");
		
	}

	namespace tile
	{
		static constexpr const TCHAR* prefix = TEXT("tile");
	}
};