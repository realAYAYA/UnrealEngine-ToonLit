// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextParameterBlock_EdGraph.h"
#include "AnimNextRigVMAssetEditorData.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Param/AnimNextParameterExecuteContext.h"
#include "Param/ParamType.h"
#include "AnimNextParameterBlock_EditorData.generated.h"

class UAnimNextParameterBlock;
class UAnimNextParameterBlockParameter;
class UAnimNextParameterBlock_Controller;
enum class ERigVMGraphNotifType : uint8;


namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
	struct FUtilsPrivate;
}

namespace UE::AnimNext::Editor
{
	struct FUtils;
	class FParameterBlockEditor;
	class SRigVMAssetView;
	class SParameterPicker;
	class FParameterBlockTabSummoner;
	class SRigVMAssetViewRow;
	class FParameterBlockParameterCustomization;
	class FModule;
}

namespace UE::AnimNext::Tests
{
	class FEditor_Parameters_ParameterBlock;
}

UCLASS()
class UAnimNextParameterBlockLibrary_Schema : public URigVMSchema
{
	GENERATED_BODY()
};

// Script-callable editor API hoisted onto UAnimNextParameterBlock
UCLASS()
class ANIMNEXTUNCOOKEDONLY_API UAnimNextParameterBlockLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, Category = "AnimNext|Parameter Block", meta=(ScriptMethod))
	static UAnimNextParameterBlockParameter* AddParameter(UAnimNextParameterBlock* InBlock, FName InName, EPropertyBagPropertyType InValueType, EPropertyBagContainerType InContainerType = EPropertyBagContainerType::None, const UObject* InValueTypeObject = nullptr, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	UFUNCTION(BlueprintCallable, Category = "AnimNext|Parameter Block", meta=(ScriptMethod))
	static UAnimNextParameterBlockGraph* AddGraph(UAnimNextParameterBlock* InBlock, FName InName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);
};

UCLASS(MinimalAPI)
class UAnimNextParameterBlock_EditorData : public UAnimNextRigVMAssetEditorData
{
	GENERATED_BODY()
	
	ANIMNEXTUNCOOKEDONLY_API static const FName ExportsAssetRegistryTag; 

	friend class UAnimNextParameterBlockLibrary;
	friend class UAnimNextParameterBlockFactory;
	friend class UAnimNextRigVMAssetEntry;
	friend class UAnimNextParameterBlock_EdGraph;
	friend class UAnimNextParameterBlockParameter;
	friend class UAnimNextParameterBlockBindingReference;
	friend struct UE::AnimNext::Editor::FUtils;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend struct UE::AnimNext::UncookedOnly::FUtilsPrivate;
	friend class UE::AnimNext::Editor::FModule;
	friend class UE::AnimNext::Editor::FParameterBlockEditor;
	friend class UE::AnimNext::Editor::SRigVMAssetView;
	friend class UE::AnimNext::Editor::SParameterPicker;
	friend class UE::AnimNext::Editor::FParameterBlockTabSummoner;
	friend class UE::AnimNext::Tests::FEditor_Parameters_ParameterBlock;
	friend class UE::AnimNext::Editor::SRigVMAssetViewRow;
	friend class UE::AnimNext::Editor::FParameterBlockParameterCustomization;

	ANIMNEXTUNCOOKEDONLY_API UAnimNextParameterBlockParameter* AddParameter(FName InName, FAnimNextParamType InType, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	ANIMNEXTUNCOOKEDONLY_API UAnimNextParameterBlockGraph* AddGraph(FName InName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	// UObject interface
	virtual void PostLoad() override;

	// IRigVMClientHost interface
	virtual void RecompileVM() override;

	// UAnimNextRigVMAssetEditorData interface
	virtual TSubclassOf<URigVMSchema> GetRigVMSchemaClass() const override { return UAnimNextParameterBlockLibrary_Schema::StaticClass(); }
	virtual UScriptStruct* GetExecuteContextStruct() const override { return FAnimNextParameterExecuteContext::StaticStruct(); }
	virtual UEdGraph* CreateEdGraph(URigVMGraph* InRigVMGraph, bool bForce) override;
	virtual bool RemoveEdGraph(URigVMGraph* InModel) override;
	virtual void CreateEdGraphForCollapseNode(URigVMCollapseNode* InNode) override;
	virtual TConstArrayView<TSubclassOf<UAnimNextRigVMAssetEntry>> GetEntryClasses() const override;

	UPROPERTY()
	TArray<TObjectPtr<UAnimNextParameterBlock_EdGraph>> Graphs_DEPRECATED;
};
