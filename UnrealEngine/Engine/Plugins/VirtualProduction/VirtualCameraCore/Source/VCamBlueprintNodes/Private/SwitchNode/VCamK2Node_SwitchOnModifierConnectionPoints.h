// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VCamK2Node_SwitchConnectionSystemBase.h"
#include "VCamK2Node_SwitchOnModifierConnectionPoints.generated.h"

class UVCamModifier;

/**
 * 
 */
UCLASS()
class VCAMBLUEPRINTNODES_API UVCamK2Node_SwitchOnModifierConnectionPoints : public UVCamK2Node_SwitchConnectionSystemBase
{
	GENERATED_BODY()
protected:

	// UEdGraphNode interface
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	// End of UEdGraphNode interface

	//~ Begin UVCamK2Node_SwitchConnectionSystemBase Interface
	virtual bool SupportsBlueprintClass(UClass*) const override;
	virtual TArray<FName> GetPinsToCreate() const override;
	//~ End UVCamK2Node_SwitchConnectionSystemBase Interface

private:

	void AccessBlueprintCDO(TFunctionRef<void(UVCamModifier*)> Func) const;
};
