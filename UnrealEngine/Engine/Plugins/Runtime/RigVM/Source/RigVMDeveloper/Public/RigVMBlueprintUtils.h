// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "EdGraph/EdGraphPin.h"

class UStruct;
class UBlueprint;
class URigVMEdGraphNode;
class UEdGraph;
class UEdGraphPin;

struct RIGVMDEVELOPER_API FRigVMBlueprintUtils
{
/** Call a function for each valid rig unit struct */
static void ForAllRigVMStructs(TFunction<void(UScriptStruct*)> InFunction);

/** Handle blueprint node reconstruction */
static void HandleReconstructAllNodes(UBlueprint* InBlueprint);

/** Handle blueprint node refresh */
static void HandleRefreshAllNodes(UBlueprint* InBlueprint);

/** Handle blueprint deleted */
static void HandleAssetDeleted(const FAssetData& InAssetData);

/** remove the variable if not used by anybody else but ToBeDeleted*/
static void RemoveMemberVariableIfNotUsed(UBlueprint* Blueprint, const FName VarName);

static FName ValidateName(UBlueprint* InBlueprint, const FString& InName);
};