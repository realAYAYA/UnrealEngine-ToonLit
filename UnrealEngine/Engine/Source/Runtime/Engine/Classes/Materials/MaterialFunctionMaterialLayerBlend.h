// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInstance.h"
#include "MaterialFunctionMaterialLayerBlend.generated.h"

/**
 * Specialized Material Function that acts as a blend
 */
UCLASS(hidecategories=object, MinimalAPI)
class UMaterialFunctionMaterialLayerBlend : public UMaterialFunction
{
	GENERATED_UCLASS_BODY()
};

/**
* Specialized Material Function Instance that instances a blend
*/
UCLASS(hidecategories = object, MinimalAPI)
class UMaterialFunctionMaterialLayerBlendInstance : public UMaterialFunctionInstance
{
	GENERATED_UCLASS_BODY()
};
