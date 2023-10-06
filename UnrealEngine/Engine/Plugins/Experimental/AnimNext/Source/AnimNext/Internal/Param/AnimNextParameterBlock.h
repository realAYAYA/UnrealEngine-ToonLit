// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PropertyBag.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "AnimNextParameterBlock.generated.h"

class UEdGraph;

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
	struct FUtilsPrivate;
}

namespace UE::AnimNext::Editor
{
	class FParametersEditor;
	struct FUtils;
}

// Library entry used to export to asset registry
USTRUCT()
struct FAnimNextParameterBlockAssetRegistryExportEntry
{
	GENERATED_BODY()

	FAnimNextParameterBlockAssetRegistryExportEntry() = default;

	FAnimNextParameterBlockAssetRegistryExportEntry(FName InName, const FSoftObjectPath& InLibrary)
		: Name(InName)
		, Library(InLibrary)
	{}
	
	UPROPERTY()
	FName Name;

	UPROPERTY()
	FSoftObjectPath Library;
};

// Library used to export to asset registry
USTRUCT()
struct FAnimNextParameterBlockAssetRegistryExports
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FAnimNextParameterBlockAssetRegistryExportEntry> Bindings;
};

/** An asset used to define AnimNext parameters and their bindings */
UCLASS(MinimalAPI, BlueprintType)
class UAnimNextParameterBlock : public UObject
{
	GENERATED_BODY()

	friend class UAnimNextParameterBlockFactory;
	friend class UAnimNextParameterBlock_EditorData;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend struct UE::AnimNext::UncookedOnly::FUtilsPrivate;
	friend class UE::AnimNext::Editor::FParametersEditor;
	friend struct UE::AnimNext::Editor::FUtils;

	// UObject interface
	virtual void PostRename(UObject* OldOuter, const FName OldName) override;
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

	// Support rig VM execution
	ANIMNEXT_API TArray<FRigVMExternalVariable> GetRigVMExternalVariables();

	UPROPERTY()
	TObjectPtr<URigVM> RigVM;

	UPROPERTY(transient)
	FRigVMExtendedExecuteContext ExtendedExecuteContext;

	UPROPERTY()
	FRigVMRuntimeSettings VMRuntimeSettings;

	UPROPERTY()
	FInstancedPropertyBag PropertyBag;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Parameters", meta = (ShowInnerProperties))
	TObjectPtr<UObject> EditorData;
#endif
};