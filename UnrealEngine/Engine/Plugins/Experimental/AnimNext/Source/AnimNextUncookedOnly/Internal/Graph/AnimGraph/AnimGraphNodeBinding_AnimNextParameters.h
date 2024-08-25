// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimGraphNodeBinding.h"
#include "AnimGraphNodeBinding_AnimNextParameters.generated.h"

USTRUCT()
struct FAnimNextAnimGraphNodeParameterBinding
{
	GENERATED_BODY()

	UPROPERTY()
	FName BindingName;

	UPROPERTY()
	int32 ArrayIndex = INDEX_NONE;

	UPROPERTY()
	FName ParameterName;

	UPROPERTY()
	FText CachedParameterNameText;
};

UCLASS(EditInlineNew, DisplayName = "Anim Next Parameter Binding")
class UAnimGraphNodeBinding_AnimNextParameters : public UAnimGraphNodeBinding
{
	GENERATED_BODY()

private:
	friend class UAnimBlueprintExtension_AnimNextParameters;

	// UAnimGraphNodeBinding interface
	virtual UScriptStruct* GetAnimNodeHandlerStruct() const override;
	virtual void OnExpandNode(IAnimBlueprintCompilationContext& InCompilationContext, UAnimGraphNode_Base* InNode, UEdGraph* InSourceGraph) override;
	virtual bool HasBinding(FName InBindingName, bool bCheckArrayIndexName) const override;
	virtual void RemoveBindings(FName InBindingName) override;
	virtual void AddPinSearchMetaDataInfo(const UEdGraphPin* InPin, FName InBindingName, TArray<struct FSearchTagDataPair>& OutTaggedMetaData) const override;
	virtual void UpdateBindingNames(TFunctionRef<FString(const FString& InOldName)> InModifierFunction) override;
	virtual void GetRequiredExtensions(TArray<TSubclassOf<UAnimBlueprintExtension>>& OutExtensions) const override;
	virtual void ProcessDuringCompilation(IAnimBlueprintCompilationContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData) override;
#if WITH_EDITOR
	virtual TSharedRef<SWidget> MakePropertyBindingWidget(const UAnimGraphNode_Base::FAnimPropertyBindingWidgetArgs& InArgs) override;
#endif

	UPROPERTY()
	TMap<FName, FAnimNextAnimGraphNodeParameterBinding> PropertyBindings;
};