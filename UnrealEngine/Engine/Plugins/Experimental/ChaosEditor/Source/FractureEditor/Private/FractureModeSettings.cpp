// Copyright Epic Games, Inc. All Rights Reserved.

#include "FractureModeSettings.h"

#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionConvexPropertiesInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FractureModeSettings)

void UFractureModeSettings::ApplyDefaultConvexSettings(FGeometryCollection& GeometryCollection) const
{
	FGeometryCollectionConvexPropertiesInterface::FConvexCreationProperties Properties = GeometryCollection.GetConvexProperties();
	Properties.FractionRemove = ConvexFractionAllowRemove;
	Properties.SimplificationThreshold = ConvexSimplificationDistanceThreshold;
	Properties.CanExceedFraction = ConvexCanExceedFraction;
	Properties.RemoveOverlaps = ConvexRemoveOverlaps;
	Properties.OverlapRemovalShrinkPercent = ConvexOverlapRemovalShrinkPercent;
	GeometryCollection.SetConvexProperties(Properties);
}
