// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VCamK2Node_SwitchConnectionSystemBase.h"
#include "Util/WidgetReference.h"
#include "VCamK2Node_SwitchOnWidgetConnections.generated.h"

class UVCamWidget;

/**
 * 
 */
UCLASS()
class VCAMBLUEPRINTNODES_API UVCamK2Node_SwitchOnWidgetConnections : public UVCamK2Node_SwitchConnectionSystemBase
{
	GENERATED_BODY()
public:

	/** A child VCamWidget from which to generate the pins. If left None, this Blueprint's widget will be used. */
	UPROPERTY(EditAnywhere, Category = PinOptions)
	FVCamChildWidgetReference TargetWidget;
	
protected:

	// UEdGraphNode interface
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	// End of UEdGraphNode interface

	//~ Begin UVCamK2Node_SwitchConnectionSystemBase Interface
	virtual bool SupportsBlueprintClass(UClass*) const override;
	virtual TArray<FName> GetPinsToCreate() const override;
	//~ End UVCamK2Node_SwitchConnectionSystemBase Interface

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface

private:

	void AccessBlueprintCDO(TFunctionRef<void(UVCamWidget*)> Func) const;
};
