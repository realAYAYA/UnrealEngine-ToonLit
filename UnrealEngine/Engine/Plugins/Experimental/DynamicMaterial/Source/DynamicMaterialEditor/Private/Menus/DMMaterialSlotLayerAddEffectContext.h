// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Components/DMMaterialLayer.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "DMMaterialSlotLayerAddEffectContext.generated.h"

class UDMMaterialLayerObject;
class UToolMenu;

UCLASS(MinimalAPI)
class UDMMaterialSlotLayerAddEffectContext : public UObject
{
	GENERATED_BODY()

public:
	UDMMaterialSlotLayerAddEffectContext() = default;

	void SetLayer(UDMMaterialLayerObject* InLayer) { LayerWeak = InLayer; }

	UDMMaterialLayerObject* GetLayer() const { return LayerWeak.Get(); }

private:
	TWeakObjectPtr<UDMMaterialLayerObject> LayerWeak;
};
