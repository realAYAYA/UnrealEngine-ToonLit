// Copyright Epic Games, Inc. All Rights Reserved.

/**
 *
 * This thumbnail renderer displays a given Control Rig
 */

#pragma once

#include "CoreMinimal.h"
#include "RigVMBlueprint.h"
#include "AssetRegistry/AssetData.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RigVMEditorBlueprintLibrary.generated.h"

UENUM(BlueprintType)
enum ERigVMBlueprintLoadLogSeverity
{
	Display,
	Warning,
	Error
};

USTRUCT(BlueprintType)
struct RIGVMEDITOR_API FRigVMBlueprintLoadLogEntry
{
	GENERATED_BODY()

	FRigVMBlueprintLoadLogEntry()
		: Severity(ERigVMBlueprintLoadLogSeverity::Display)
		, Subject(nullptr)
	{}

	FRigVMBlueprintLoadLogEntry(ERigVMBlueprintLoadLogSeverity InSeverity, UObject* InSubject, const FString& InMessage)
		: Severity(InSeverity)
		, Subject(InSubject)
		, Message(InMessage)
	{}
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=Log)
	TEnumAsByte<ERigVMBlueprintLoadLogSeverity> Severity;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=Log)
	TObjectPtr<UObject> Subject;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=Log)
	FString Message;
};

DECLARE_DELEGATE_RetVal_OneParam(bool, FRigVMAssetDataFilter, const FAssetData&);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FRigVMBlueprintFilter, const URigVMBlueprint*, const TArray<FRigVMBlueprintLoadLogEntry>&);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FRigVMNodeFilter, const URigVMBlueprint*, const URigVMNode*);
DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(bool, FRigVMAssetDataFilterDynamic, FAssetData, AssetData);
DECLARE_DYNAMIC_DELEGATE_RetVal_TwoParams(bool, FRigVMBlueprintFilterDynamic, const URigVMBlueprint*, Blueprint, TArray<FRigVMBlueprintLoadLogEntry>, LogDuringLoad);
DECLARE_DYNAMIC_DELEGATE_RetVal_TwoParams(bool, FRigVMNodeFilterDynamic, const URigVMBlueprint*, Blueprint, const URigVMNode*, Node);

UCLASS(BlueprintType, meta=(ScriptName="RigVMBlueprintLibrary"))
class RIGVMEDITOR_API URigVMEditorBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint")
	static void RecompileVM(URigVMBlueprint* InBlueprint);

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint")
	static void RecompileVMIfRequired(URigVMBlueprint* InBlueprint);
	
	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint")
	static void RequestAutoVMRecompilation(URigVMBlueprint* InBlueprint);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RigVM Blueprint")
	static URigVMGraph* GetModel(URigVMBlueprint* InBlueprint);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RigVM Blueprint")
	static URigVMController* GetController(URigVMBlueprint* InBlueprint);

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint")
	static TArray<URigVMBlueprint*> LoadAssets();

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint")
	static TArray<URigVMBlueprint*> LoadAssetsByClass(TSubclassOf<URigVMBlueprint> InClass);

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint", meta = (DisplayName = "LoadAssetsWithBlueprintFilter", ScriptName = "LoadAssetsWithBlueprintFilter"))
		static TArray<URigVMBlueprint*> LoadAssetsWithBlueprintFilter_ForBlueprint(
		TSubclassOf<URigVMBlueprint> InClass,
		FRigVMBlueprintFilterDynamic InBlueprintFilter);

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint", meta = (DisplayName = "LoadAssetsWithAssetDataFilter", ScriptName = "LoadAssetsWithAssetDataFilter"))
		static TArray<URigVMBlueprint*> LoadAssetsWithAssetDataFilter_ForBlueprint(
		TSubclassOf<URigVMBlueprint> InClass,
		FRigVMAssetDataFilterDynamic InAssetDataFilter);

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint", meta = (DisplayName = "LoadAssetsWithNodeFilter", ScriptName = "LoadAssetsWithNodeFilter"))
	static TArray<URigVMBlueprint*> LoadAssetsWithNodeFilter_ForBlueprint(
		TSubclassOf<URigVMBlueprint> InClass,
		FRigVMNodeFilterDynamic InNodeFilter);

	static TArray<URigVMBlueprint*> LoadAssetsWithNodeFilter(
		TSubclassOf<URigVMBlueprint> InClass,
		FRigVMNodeFilter InNodeFilter);

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint", meta = (DisplayName = "LoadAssetsWithAssetDataAndBlueprintFilters", ScriptName = "LoadAssetsWithAssetDataAndBlueprintFilters"))
	static TArray<URigVMBlueprint*> LoadAssetsWithAssetDataAndBlueprintFilters_ForBlueprint(
		TSubclassOf<URigVMBlueprint> InClass,
		FRigVMAssetDataFilterDynamic InAssetDataFilter,
		FRigVMBlueprintFilterDynamic InBlueprintFilter);

	static TArray<URigVMBlueprint*> LoadAssetsWithAssetDataAndBlueprintFilters(
		TSubclassOf<URigVMBlueprint> InClass,
		FRigVMAssetDataFilter InAssetDataFilter,
		FRigVMBlueprintFilter InBlueprintFilter);

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint", meta = (DisplayName = "LoadAssetsWithAssetDataAndNodeFilters", ScriptName = "LoadAssetsWithAssetDataAndNodeFilters"))
	static TArray<URigVMBlueprint*> LoadAssetsWithAssetDataAndNodeFilters_ForBlueprint(
		TSubclassOf<URigVMBlueprint> InClass,
		FRigVMAssetDataFilterDynamic InAssetDataFilter,
		FRigVMNodeFilterDynamic InNodeFilter);

	static TArray<URigVMBlueprint*> LoadAssetsWithAssetDataAndNodeFilters(
		TSubclassOf<URigVMBlueprint> InClass,
		FRigVMAssetDataFilter InAssetDataFilter,
		FRigVMNodeFilter InNodeFilter);

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint", meta = (DisplayName = "GetAssetsWithFilter", ScriptName = "GetAssetsWithFilter"))
	static TArray<FAssetData> GetAssetsWithFilter_ForBlueprint(
		TSubclassOf<URigVMBlueprint> InClass,
		FRigVMAssetDataFilterDynamic InAssetDataFilter);

	static TArray<FAssetData> GetAssetsWithFilter(
		TSubclassOf<URigVMBlueprint> InClass,
		FRigVMAssetDataFilter InAssetDataFilter);
};
