// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Interfaces/ToolMode.h"

#if !UE_BUILD_SHIPPING

namespace BuildPatchTool
{
	class FAutomationToolModeFactory
	{
	public:
		static IToolModeRef Create(IBuildPatchServicesModule& BpsInterface);
	};
}

#endif // !UE_BUILD_SHIPPING
