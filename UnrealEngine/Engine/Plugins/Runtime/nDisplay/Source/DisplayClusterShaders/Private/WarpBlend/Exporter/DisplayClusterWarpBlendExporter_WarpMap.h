// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FDisplayClusterWarpBlendExporter_WarpMap
{
public:
	static bool ExportWarpMap(class IDisplayClusterRenderTexture* InWarpMap, struct FMPCDIGeometryExportData& Dst, uint32 InMaxDimension = 0);
};
