// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextRigVMAssetEntry.h"
#include "IAnimNextRigVMParameterInterface.h"
#include "Param/ParamType.h"
#include "AnimNextParameterBlockParameter.generated.h"

class UAnimNextParameterLibrary;
class UAnimNextParameterBlock_EditorData;

namespace UE::AnimNext::Editor
{
	class FParameterBlockParameterCustomization;
}

namespace UE::AnimNext::Tests
{
	class FEditor_Parameters_ParameterBlock;
}

UCLASS(MinimalAPI, Category = "Parameters")
class UAnimNextParameterBlockParameter : public UAnimNextRigVMAssetEntry, public IAnimNextRigVMParameterInterface
{
	GENERATED_BODY()

	friend class UAnimNextParameterBlock_EditorData;
	friend class UE::AnimNext::Tests::FEditor_Parameters_ParameterBlock;
	friend class UE::AnimNext::Editor::FParameterBlockParameterCustomization;

	// UAnimNextRigVMAssetEntry interface
	virtual FName GetEntryName() const override;
	virtual void SetEntryName(FName InName, bool bSetupUndoRedo = true) override;
	virtual FText GetDisplayName() const override;
	virtual FText GetDisplayNameTooltip() const override;

	// IAnimNextRigVMParameterInterface interface
	virtual FAnimNextParamType GetParamType() const override;
	virtual bool SetParamType(const FAnimNextParamType& InType, bool bSetupUndoRedo = true) override;
	virtual FInstancedPropertyBag& GetPropertyBag() const override;
	
	/** Parameter name we reference */
	UPROPERTY(VisibleAnywhere, Category = Parameter)
	FName ParameterName;

	/** The parameter's type */
	UPROPERTY(EditAnywhere, Category = "Parameter", AssetRegistrySearchable)
	FAnimNextParamType Type = FAnimNextParamType::GetType<bool>();
	
	/** Comment to display in editor */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta=(MultiLine))
	FString Comment;
};