// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "BuildPatchSettings.h"

namespace BuildPatchServices
{
	class IPackageChunks
	{
	public:
		virtual ~IPackageChunks() {}
		virtual bool Run() = 0;
	};

	class FPackageChunksFactory
	{
	public:
		static IPackageChunks* Create(const FPackageChunksConfiguration& Configuration);
	};
}
