// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGDebug.h"
#include "Materials/MaterialInterface.h"

namespace PCGDebugVisConstants
{
	const FSoftObjectPath DefaultPointMesh = FSoftObjectPath(TEXT("/PCG/DebugObjects/PCG_Cube.PCG_Cube"));
	const FSoftObjectPath MaterialForDefaultPointMesh = FSoftObjectPath(TEXT("Material'/PCG/DebugObjects/PCG_DebugMaterial.PCG_DebugMaterial'"));
}

FPCGDebugVisualizationSettings::FPCGDebugVisualizationSettings()
{
	PointMesh = PCGDebugVisConstants::DefaultPointMesh;
}

TSoftObjectPtr<UMaterialInterface> FPCGDebugVisualizationSettings::GetMaterial() const
{
	if (MaterialOverride.IsNull() && PointMesh.ToSoftObjectPath() == PCGDebugVisConstants::DefaultPointMesh)
	{
		return TSoftObjectPtr<UMaterialInterface>(PCGDebugVisConstants::MaterialForDefaultPointMesh);
	}
	else
	{
		return MaterialOverride;
	}
}