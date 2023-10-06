// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/Material.h"

#include "PreviewMaterial.generated.h"

UCLASS(MinimalAPI)
class UPreviewMaterial : public UMaterial
{
	GENERATED_UCLASS_BODY()


	//~ Begin UMaterial Interface.
	UNREALED_API virtual FMaterialResource* AllocateResource() override;
	virtual bool IsAsset()  const override  { return false; }
	//~ End UMaterial Interface.
};

