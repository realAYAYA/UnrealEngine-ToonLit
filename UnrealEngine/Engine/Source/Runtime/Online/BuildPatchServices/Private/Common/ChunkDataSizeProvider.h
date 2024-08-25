// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Common/DataSizeProvider.h"
#include "BuildPatchManifest.h"
#include "Containers/ContainersFwd.h"

namespace BuildPatchServices
{
	class IChunkDataSizeProvider
		: public IDataSizeProvider
	{
	public:
		virtual void AddManifestData(FBuildPatchAppManifestPtr Manifest) = 0;
		virtual void AddManifestData(TConstArrayView<FBuildPatchAppManifestPtr> Manifests) = 0;
	};

	class FChunkDataSizeProviderFactory
	{
	public:
		static IChunkDataSizeProvider* Create();
	};
}