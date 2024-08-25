// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "GeometryCollectionBlueprintLibrary.generated.h"

/** Blueprint library for Geometry Collections. */
UCLASS(meta = (ScriptName = "GeometryCollectionLibrary"))
class GEOMETRYCOLLECTIONENGINE_API UGeometryCollectionBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 
	 * Set a custom instance data value for all instances associated with a geometry collection. 
	 * This assumes that the geometry collection is using a custom instanced renderer.
	 * @param GeometryCollectionComponent	The Geometry Collection Component that we want to set custom instance data on.
	 * @param CustomDataIndex	The custom instance data slot that we want to set.
	 * @param CustomDataValue	The value to set to the custom instance data slot.
	 */
	UFUNCTION(BlueprintCallable, Category = AtomMaterial)
	static void SetISMPoolCustomInstanceData(UGeometryCollectionComponent* GeometryCollectionComponent, int32 CustomDataIndex, float CustomDataValue);
};
