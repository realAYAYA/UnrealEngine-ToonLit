// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInstance.h"
#include "MaterialFunctionMaterialLayer.generated.h"

/**
 * Specialized Material Function that acts as a layer
 */
UCLASS(hidecategories=object, MinimalAPI)
class UMaterialFunctionMaterialLayer : public UMaterialFunction
{
	GENERATED_UCLASS_BODY()
};

/**
* Specialized Material Function Instance that instances a layer
*/
UCLASS(hidecategories = object, MinimalAPI)
class UMaterialFunctionMaterialLayerInstance : public UMaterialFunctionInstance
{
	GENERATED_UCLASS_BODY()
};
