// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextParameterBlockEntry.h"
#include "IAnimNextParameterBlockGraphInterface.h"
#include "IAnimNextParameterBlockBindingInterface.h"
#include "AnimNextParameterBlockBinding.generated.h"

class UAssetDefinition_AnimNextParameterBlockBinding;
class UAnimNextParameter;
class UAnimNextParameterLibrary;
class URigVMGraph;

namespace UE::AnimNext::Editor
{
	class SParameterBlockViewRow;
}

/** Parameter binding block entry */
UCLASS(MinimalAPI, BlueprintType)
class UAnimNextParameterBlockBinding : public UAnimNextParameterBlockEntry, public IAnimNextParameterBlockBindingInterface, public IAnimNextParameterBlockGraphInterface
{
	GENERATED_BODY()

	friend class UAnimNextParameterBlock_EditorData;
	friend class UAssetDefinition_AnimNextParameterBlockBinding;
	friend class UE::AnimNext::Editor::SParameterBlockViewRow;

	// IAnimNextParameterBlockBindingInterface interface
	virtual FAnimNextParamType GetParamType() const override;
	virtual void SetParameterName(FName InName, bool bSetupUndoRedo = true) override;
	virtual FName GetParameterName() const override;
	virtual const UAnimNextParameter* GetParameter() const override;
	virtual const UAnimNextParameterLibrary* GetLibrary() const override;

	// IAnimNextParameterBlockGraphInterface interface
	virtual URigVMGraph* GetGraph() const override { return BindingGraph; }

	// UAnimNextParameterBlockEntry interface
	virtual FText GetDisplayName() const override;
	virtual FText GetDisplayNameTooltip() const override;
	virtual void GetEditedObjects(TArray<UObject*>& OutObjects) const override;

	/** Parameter name we reference */
	UPROPERTY(VisibleAnywhere, Category = Parameter)
	FName ParameterName;

	/** Parameter library we reference */
	UPROPERTY(VisibleAnywhere, Category = Parameter)
	TObjectPtr<UAnimNextParameterLibrary> Library;

	/** Binding graph */
	UPROPERTY()
	TObjectPtr<URigVMGraph> BindingGraph;
};