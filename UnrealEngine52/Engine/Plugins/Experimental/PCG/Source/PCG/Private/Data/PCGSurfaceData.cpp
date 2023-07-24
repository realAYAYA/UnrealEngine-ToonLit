// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGSurfaceData.h"
#include "Data/PCGSpatialData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGSurfaceData)

void UPCGSurfaceData::CopyBaseSurfaceData(UPCGSurfaceData* NewSurfaceData) const
{
	NewSurfaceData->Transform = Transform;
}
