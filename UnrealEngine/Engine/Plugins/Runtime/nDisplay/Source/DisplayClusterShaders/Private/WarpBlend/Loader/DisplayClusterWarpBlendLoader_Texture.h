// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RenderResource.h"
#include "WarpBlend/DisplayClusterWarpEnums.h"

class IDisplayClusterRenderTexture;

namespace mpcdi
{
	struct DataMap;
	struct GeometryWarpFile;
	struct PFM;
};

class FDisplayClusterWarpBlendLoader_Texture
{
public:
	static IDisplayClusterRenderTexture* CreateBlendMap(mpcdi::DataMap* SourceDataMap);
	static IDisplayClusterRenderTexture* CreateBlendMap(const FString& InFileName);
	static IDisplayClusterRenderTexture* CreateDummyBlendMap();

	static IDisplayClusterRenderTexture* CreateWarpMap(EDisplayClusterWarpProfileType ProfileType, const FString& InPFMFile, float PFMScale, bool bIsUnrealGameSpace);
	static IDisplayClusterRenderTexture* CreateWarpMap(EDisplayClusterWarpProfileType InProfileType, mpcdi::GeometryWarpFile* SourceWarpMap);
	static IDisplayClusterRenderTexture* CreateWarpMap(EDisplayClusterWarpProfileType InProfileType, mpcdi::PFM* SourcePFM, float PFMScale, bool bIsUnrealGameSpace);
	static IDisplayClusterRenderTexture* CreateWarpMap(EDisplayClusterWarpProfileType InProfileType, const TArray<FVector>& InPoints, uint32 WarpX, uint32 WarpY, float WorldScale, bool bIsUnrealGameSpace);
	static IDisplayClusterRenderTexture* CreateWarpMap(EDisplayClusterWarpProfileType InProfileType, const struct FMPCDIGeometryImportData& Src);
};
