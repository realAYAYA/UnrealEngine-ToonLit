// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "K2Node_AddPinInterface.h"

#include "K2Node_MakeRequestHeader.generated.h"

USTRUCT()
struct FOptionalPin
{
	GENERATED_BODY()

	UPROPERTY()
	FName PinName;

	UPROPERTY()
	FString PinDefaultValue;
};

UCLASS(MinimalAPI)
class UK2Node_MakeRequestHeader
	: public UK2Node
	, public IK2Node_AddPinInterface
{
	GENERATED_BODY()

public:
	UK2Node_MakeRequestHeader(const FObjectInitializer& ObjectInitializer);

	/** Begin UEdGraphNode Interface */
	virtual void AllocateDefaultPins() override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	/** End UEdGraphNode Interface */

	/** Begin UK2Node Interface */
	virtual void GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual bool NodeCausesStructuralBlueprintChange() const override;
	virtual bool IsNodePure() const override;
	/** Begin UK2Node Interface */

	/** Begin IK2Node_AddPinInterface */
	virtual bool CanAddPin() const override;
	virtual bool CanRemovePin(const UEdGraphPin* Pin) const override;
	virtual void AddInputPin() override;
	virtual void RemoveInputPin(UEdGraphPin* Pin) override;
	/** End IK2Node_AddPinInterface */

	static FName GetOutputPinName();

	UEdGraphPin* GetOutputPin() const;

protected:
	UE_NODISCARD bool IsValidInputPin(const UEdGraphPin* InputPin) const;

	void ConstructDefaultPinsForPreset();

	void SyncPinNames();
	static FName GetPinName(const int32& PinIndex);

private:
	TObjectPtr<UEnum> PresetEnum;

	UPROPERTY()
	int32 PresetEnumIndex;

	UPROPERTY()
	int32 NumInputs;

	UPROPERTY()
	TArray<FOptionalPin> OptionalPins;
};
