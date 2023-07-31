// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "BuildPatchSettings.h"

namespace BuildPatchServices
{
	class IDiffManifests
	{
	public:
		virtual ~IDiffManifests(){}
		virtual bool Run() = 0;
	};

	class FDiffManifestsFactory
	{
	public:
		static IDiffManifests* Create(const FDiffManifestsConfiguration& Configuration);
	};
}
