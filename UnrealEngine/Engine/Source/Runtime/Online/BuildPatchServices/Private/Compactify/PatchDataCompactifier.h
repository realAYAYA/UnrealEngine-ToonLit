// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "BuildPatchSettings.h"

namespace BuildPatchServices
{
	class IPatchDataCompactifier
	{
	public:
		virtual ~IPatchDataCompactifier() {}
		virtual bool Run() = 0;
	};

	class FPatchDataCompactifierFactory
	{
	public:
		static IPatchDataCompactifier* Create(const FCompactifyConfiguration& Configuration);
	};
}
