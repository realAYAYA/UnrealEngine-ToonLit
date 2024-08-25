// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMMaterialStageSource.h"
#include "DMMaterialStageInput.generated.h"

class UDMMaterialSlot;
class UMaterial;
struct FDMMaterialStageConnectorChannel;

/**
 * A node which produces an output (e.g. Texture coordinate.)
 */
UCLASS(Abstract, BlueprintType, ClassGroup = "Material Designer", meta = (DisplayName = "Material Designer Stage Input"))
class DYNAMICMATERIALEDITOR_API UDMMaterialStageInput : public UDMMaterialStageSource
{
	GENERATED_BODY()

public:
	static const FString StageInputPrefixStr;

	virtual FText GetChannelDescription(const FDMMaterialStageConnectorChannel& Channel)
		PURE_VIRTUAL(UDMMaterialStageInput::GetChannelDescription, return FText::GetEmpty();)
		
	virtual void Update(EDMUpdateType InUpdateType) override;

	//~ Begin UDMMaterialComponent
	virtual FString GetComponentPathComponent() const override;
	//~ End UDMMaterialComponent

protected:
	UDMMaterialStageInput()
	{
	}

	//~ Begin UDMMaterialStageSource
	virtual void UpdateOutputConnectors() { }
	virtual void UpdatePreviewMaterial(UMaterial* InPreviewMaterial = nullptr) override;
	//~ End UDMMaterialStageSource

	//~ Begin UDMMaterialComponent
	virtual void GetComponentPathInternal(TArray<FString>& OutChildComponentPathComponents) const override;
	//~ End UDMMaterialComponent
};
