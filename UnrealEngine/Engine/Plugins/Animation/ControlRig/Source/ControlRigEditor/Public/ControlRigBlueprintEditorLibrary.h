// Copyright Epic Games, Inc. All Rights Reserved.

/**
 *
 * This thumbnail renderer displays a given Control Rig
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ControlRigBlueprint.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ControlRigBlueprintEditorLibrary.generated.h"

UENUM(BlueprintType)
enum class ECastToControlRigBlueprintCases : uint8
{
	CastSucceeded,
	CastFailed
};

UCLASS(meta=(ScriptName="ControlRigBlueprintLibrary"))
class CONTROLRIGEDITOR_API UControlRigBlueprintEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Control Rig Blueprint", Meta = (ExpandEnumAsExecs = "Branches"))
	static void CastToControlRigBlueprint(
		UObject* Object, 
		ECastToControlRigBlueprintCases& Branches,
		UControlRigBlueprint*& AsControlRigBlueprint);

	UFUNCTION(BlueprintCallable, Category = "Control Rig Blueprint")
	static void SetPreviewMesh(UControlRigBlueprint* InRigBlueprint, USkeletalMesh* PreviewMesh, bool bMarkAsDirty = true);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Control Rig Blueprint")
	static USkeletalMesh* GetPreviewMesh(UControlRigBlueprint* InRigBlueprint);

	UFUNCTION(BlueprintCallable, Category = "Control Rig Blueprint")
	static void RecompileVM(UControlRigBlueprint* InRigBlueprint);

	UFUNCTION(BlueprintCallable, Category = "Control Rig Blueprint")
	static void RecompileVMIfRequired(UControlRigBlueprint* InRigBlueprint);
	
	UFUNCTION(BlueprintCallable, Category = "Control Rig Blueprint")
	static void RequestAutoVMRecompilation(UControlRigBlueprint* InRigBlueprint);

	UFUNCTION(BlueprintCallable, Category = "Control Rig Blueprint")
	static void RequestControlRigInit(UControlRigBlueprint* InRigBlueprint);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Control Rig Blueprint")
	static URigVMGraph* GetModel(UControlRigBlueprint* InRigBlueprint);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Control Rig Blueprint")
	static URigVMController* GetController(UControlRigBlueprint* InRigBlueprint);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Control Rig Blueprint")
	static TArray<UControlRigBlueprint*> GetCurrentlyOpenRigBlueprints();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Control Rig Blueprint")
	static TArray<UStruct*> GetAvailableRigUnits();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Control Rig Blueprint")
	static URigHierarchy* GetHierarchy(UControlRigBlueprint* InRigBlueprint);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Control Rig Blueprint")
	static URigHierarchyController* GetHierarchyController(UControlRigBlueprint* InRigBlueprint);
};

