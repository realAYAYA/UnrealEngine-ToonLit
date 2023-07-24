// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimGraphNode_CustomProperty.h"
#include "AnimGraph/AnimNode_AnimNextInterfaceGraph.h"
#include "SSearchableComboBox.h"
#include "AnimGraphNode_AnimNextInterfaceGraph.generated.h"

struct FVariableMappingInfo;

UCLASS(MinimalAPI)
class UAnimGraphNode_AnimNextInterfaceGraph : public UAnimGraphNode_CustomProperty
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_AnimNextInterfaceGraph Node;

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostReconstructNode() override;

private:
	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const override;
	virtual FText GetTooltipText() const override;
	virtual void PreloadRequiredAssets() override;
	//virtual bool IsSinkNode() const { return true; }

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

	bool IsInputProperty(const FName& PropertyName) const;

	virtual UObject* GetJumpTargetForDoubleClick() const override;
};

