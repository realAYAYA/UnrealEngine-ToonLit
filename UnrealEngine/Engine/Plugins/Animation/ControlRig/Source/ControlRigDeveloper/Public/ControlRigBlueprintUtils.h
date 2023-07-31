// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "EdGraph/EdGraphPin.h"

class UStruct;
class UBlueprint;
class UControlRigGraphNode;
class UEdGraph;
class UEdGraphPin;

struct CONTROLRIGDEVELOPER_API FControlRigBlueprintUtils
{
/** Call a function for each valid rig unit struct */
static void ForAllRigUnits(TFunction<void(UScriptStruct*)> InFunction);

/** Handle blueprint node reconstruction */
static void HandleReconstructAllNodes(UBlueprint* InBlueprint);

/** Handle blueprint node refresh */
static void HandleRefreshAllNodes(UBlueprint* InBlueprint);

/** remove the variable if not used by anybody else but ToBeDeleted*/
static void RemoveMemberVariableIfNotUsed(UBlueprint* Blueprint, const FName VarName, UControlRigGraphNode* ToBeDeleted);

static FName ValidateName(UBlueprint* InBlueprint, const FString& InName);
};