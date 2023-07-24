// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimGraphNode_CustomProperty.h"
#include "AnimNode_ControlRig.h"
#include "SSearchableComboBox.h"
#include "AnimGraphNode_ControlRig.generated.h"

struct FVariableMappingInfo;

UCLASS(MinimalAPI)
class UAnimGraphNode_ControlRig : public UAnimGraphNode_CustomProperty
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_ControlRig Node;

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostReconstructNode() override;

private:
	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const override;
	virtual FText GetTooltipText() const override;

	virtual FAnimNode_CustomProperty* GetCustomPropertyNode() override { return &Node; }
	virtual const FAnimNode_CustomProperty* GetCustomPropertyNode() const override { return &Node; }

	// property related things
	void GetVariables(bool bInput, TMap<FName, FRigVMExternalVariable>& OutParameters) const;

	TMap<FName, FRigVMExternalVariable> InputVariables;
	TMap<FName, FRigVMExternalVariable> OutputVariables;

	void RebuildExposedProperties();
	virtual void CreateCustomPins(TArray<UEdGraphPin*>* OldPins) override;
	virtual void ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog) override;
	
	// pin option related
	bool IsPropertyExposeEnabled(FName PropertyName) const;
	ECheckBoxState IsPropertyExposed(FName PropertyName) const;
	void OnPropertyExposeCheckboxChanged(ECheckBoxState NewState, FName PropertyName);

	// SVariableMappingWidget related
	void OnVariableMappingChanged(const FName& PathName, const FName& Curve, bool bInput);
	FName GetVariableMapping(const FName& PathName, bool bInput);
	void GetAvailableMapping(const FName& PathName, TArray<FName>& OutArray, bool bInput);
	void CreateVariableMapping(const FString& FilteredText, TArray< TSharedPtr<FVariableMappingInfo> >& OutArray, bool bInput);

	bool IsAvailableToMapToCurve(const FName& PropertyName, bool bInput) const;
	bool IsInputProperty(const FName& PropertyName) const;
	FRigControlElement* FindControlElement(const FName& InControlName) const;
};

