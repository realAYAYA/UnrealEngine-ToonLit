// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Inventory/LyraInventoryItemDefinition.h"
#include "Templates/SubclassOf.h"
#include "UObject/UObjectGlobals.h"

#include "InventoryFragment_ReticleConfig.generated.h"

class ULyraReticleWidgetBase;
class UObject;

UCLASS()
class UInventoryFragment_ReticleConfig : public ULyraInventoryItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Reticle)
	TArray<TSubclassOf<ULyraReticleWidgetBase>> ReticleWidgets;
};
