// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "Slate/SDMSlot.h"
#include "UObject/WeakObjectPtr.h"

class UDMMaterialLayerObject;
class UDMMaterialSlot;
class UDMMaterialStage;
class UDMMaterialStageInput;
struct FExpressionInput;
struct FExpressionOutput;

namespace UE::DynamicMaterialEditor::Private
{
	struct FDMInputInputs
	{
		int32 InputIndex;
		TArray<UDMMaterialStageInput*> ChannelInputs;
	};

	void SetMask(FExpressionInput& InInputConnector, const FExpressionOutput& InOutputConnector, int32 InChannelOverride);

	/** Converts 0,1,2,3,4 to 0,1,2,4,8 */
	int32 ChannelIndexToChannelBit(int32 InChannelIndex);

	bool IsCustomMaterialProperty(EDMMaterialPropertyType InMaterialProperty);

	void LogError(const FString& InMessage, bool bInToast = false);
}

struct FDMMaterialLayerReference
{
	TWeakObjectPtr<UDMMaterialLayerObject> LayerWeak;

	FDMMaterialLayerReference();
	FDMMaterialLayerReference(UDMMaterialLayerObject* InLayer);

	UDMMaterialLayerObject* GetLayer() const;

	bool IsBaseEnabled() const;
	bool IsBaseBeingEdited() const;
	bool IsMaskEnabled() const;
	bool IsMaskBeingEdited() const;
};
