// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IOptimusNodeAdderPinProvider.h"
#include "IOptimusNodePairProvider.h"
#include "IOptimusParameterBindingProvider.h"
#include "IOptimusPinMutabilityDefiner.h"
#include "IOptimusUnnamedNodePinProvider.h"
#include "OptimusBindingTypes.h"
#include "OptimusNode.h"
#include "OptimusNode_GraphTerminal.h"

#include "OptimusNode_LoopTerminal.generated.h"

class UOptimusNode_LoopTerminal;


USTRUCT()
struct FOptimusPinPairInfo
{
	GENERATED_BODY()
	// Using PinNamePath here such that it plays well with default UObject undo/redo
	
	UPROPERTY()
	TArray<FName> InputPinPath;
	
	UPROPERTY()
	TArray<FName> OutputPinPath;

	bool operator==(const FOptimusPinPairInfo& InOther) const
	{
		return InOther.InputPinPath == InputPinPath && InOther.OutputPinPath == OutputPinPath;
	}

};

USTRUCT()
struct FOptimusLoopTerminalInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Loop Terminal")
	int32 Count = 1;

	UPROPERTY(EditAnywhere, Category="Loop Terminal", meta=(FullyExpand="true", NoResetToDefault))
	FOptimusParameterBindingArray Bindings;	
};

UCLASS(Hidden)
class OPTIMUSCORE_API UOptimusNode_LoopTerminal :
	public UOptimusNode,
	public IOptimusNodeAdderPinProvider,
	public IOptimusUnnamedNodePinProvider,
	public IOptimusNodePairProvider,
	public IOptimusPinMutabilityDefiner,
	public IOptimusParameterBindingProvider
{
	GENERATED_BODY()
	
public:
	UOptimusNode_LoopTerminal();

#if WITH_EDITOR
	// UObject overrides
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
	// UOptimusNode overrides
	FName GetNodeCategory() const override { return NAME_None; }
	FText GetDisplayName() const override;
	void ConstructNode() override;
	bool ValidateConnection(const UOptimusNodePin& InThisNodesPin, const UOptimusNodePin& InOtherNodesPin, FString* OutReason) const override;

	// IOptimusNodeAdderPinProvider
	TArray<FAdderPinAction> GetAvailableAdderPinActions(const UOptimusNodePin* InSourcePin, EOptimusNodePinDirection InNewPinDirection, FString* OutReason) const override;
	TArray<UOptimusNodePin*> TryAddPinFromPin(const FAdderPinAction& InSelectedAction, UOptimusNodePin* InSourcePin, FName InNameToUse) override;
	bool RemoveAddedPins(TConstArrayView<UOptimusNodePin*> InAddedPinsToRemove) override;

	// IOptimusUnnamedNodePinProvider
	bool IsPinNameHidden(UOptimusNodePin* InPin) const override;
	FName GetNameForAdderPin(UOptimusNodePin* InPin) const override;

	// IOptimusPinMutabilityDefiner
	EOptimusPinMutability GetOutputPinMutability(const UOptimusNodePin* InPin) const override;

	// IOptimusNodePairProvider
	void PairToCounterpartNode(const IOptimusNodePairProvider* NodePairProvider) override;

	// IOptimusParameterBindingProvider
	FString GetBindingDeclaration(FName BindingName) const;
	bool GetBindingSupportAtomicCheckBoxVisibility(FName BindingName) const;
	bool GetBindingSupportReadCheckBoxVisibility(FName BindingName) const;
	EOptimusDataTypeUsageFlags GetTypeUsageFlags(const FOptimusDataDomain& InDataDomain) const;
	
	UOptimusNodePin* GetPinCounterpart(const UOptimusNodePin* InNodePin, EOptimusTerminalType InTerminalType, TOptional<EOptimusNodePinDirection> InDirection = {}) const;
	UOptimusNode_LoopTerminal* GetOtherTerminal() const;
	int32 GetLoopCount() const;
	EOptimusTerminalType GetTerminalType() const;

	static int32 GetDataFunctionIndexFromPin(const UOptimusNodePin* InPin);

protected:
	friend class UOptimusNodeGraph;
	
	/** Indicates whether this is an entry or a return terminal node */
	UPROPERTY(VisibleAnywhere, Category="Loop Terminal")
	EOptimusTerminalType TerminalType;

	
	UPROPERTY(EditAnywhere, Category="Loop Terminal", DisplayName="Settings", meta=(FullyExpand="true", EditCondition="TerminalType==EOptimusTerminalType::Entry", EditConditionHides))
	FOptimusLoopTerminalInfo LoopInfo;

	UPROPERTY()
	UOptimusNodePin* IndexPin;
	
	UPROPERTY()
	UOptimusNodePin* CountPin;

	UPROPERTY()
	TArray<FOptimusPinPairInfo> PinPairInfos;
	
private:
#if WITH_EDITOR
	void PropertyArrayPasted(const FPropertyChangedEvent& InPropertyChangedEvent);
	void PropertyValueChanged(const FPropertyChangedEvent& InPropertyChangedEvent);
	void PropertyArrayItemAdded(const FPropertyChangedEvent& InPropertyChangedEvent);
	void PropertyArrayItemRemoved(const FPropertyChangedEvent& InPropertyChangedEvent);
	void PropertyArrayCleared(const FPropertyChangedEvent& InPropertyChangedEvent);
	void PropertyArrayItemMoved(const FPropertyChangedEvent& InPropertyChangedEvent);
#endif

	TArray<UOptimusNodePin*> AddPinPairs(const FOptimusParameterBinding& InBinding);
	TArray<UOptimusNodePin*> AddPinPairsDirect(const FOptimusParameterBinding& InBinding);
	TArray<UOptimusNodePin*> GetPairedPins(const FOptimusPinPairInfo& InPair) const;
	static int32 GetPairIndex(const UOptimusNodePin* Pin);
	void RemovePinPair(int32 InPairIndex);
	void RemovePinPairDirect(int32 InPairIndex);
	void ClearPinPairs();
	void MovePinPair();
	void UpdatePinPairs();
	FOptimusLoopTerminalInfo* GetLoopInfo();
	const FOptimusLoopTerminalInfo* GetLoopInfo() const;
	
	void SanitizeBinding(FOptimusParameterBinding& InOutBinding, FName InOldName);
	UOptimusNode_LoopTerminal* GetTerminalByType(EOptimusTerminalType InType);
	const UOptimusNode_LoopTerminal* GetTerminalByType(EOptimusTerminalType InType) const;
	
	FName GetSanitizedBindingName(
		FName InNewName,
		FName InOldName
	);
};
