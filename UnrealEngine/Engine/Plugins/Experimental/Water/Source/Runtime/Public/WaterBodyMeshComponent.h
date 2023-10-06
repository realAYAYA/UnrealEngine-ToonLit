// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/StaticMeshComponent.h"

#include "WaterBodyMeshComponent.generated.h"

class UWaterBodyComponent;
struct FMeshDescription;
class UMaterialInterface;

/**
 * Base class for meshes used to render water bodies without relying on the water zone/water mesh.
 */
UCLASS(MinimalAPI)
class UWaterBodyMeshComponent : public UStaticMeshComponent
{
	GENERATED_UCLASS_BODY()
public:

protected:
	virtual bool CanCreateSceneProxy() const;

	// Exclude water mesh components from HLOD since we have a custom HLOD builder for water bodies.
	virtual bool IsHLODRelevant() const override { return false; }
};

