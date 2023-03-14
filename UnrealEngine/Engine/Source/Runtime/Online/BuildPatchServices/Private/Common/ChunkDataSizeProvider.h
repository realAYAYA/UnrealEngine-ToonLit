// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Common/DataSizeProvider.h"
#include "BuildPatchManifest.h"

namespace BuildPatchServices
{
	class IChunkDataSizeProvider
		: public IDataSizeProvider
	{
	public:
		virtual void AddManifestData(FBuildPatchAppManifestPtr Manifest) = 0;
	};

	class FChunkDataSizeProviderFactory
	{
	public:
		static IChunkDataSizeProvider* Create();
	};
}