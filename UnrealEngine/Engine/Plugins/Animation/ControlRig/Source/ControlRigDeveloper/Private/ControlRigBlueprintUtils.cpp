// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigBlueprintUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Units/RigUnit.h"
#include "UObject/UObjectIterator.h"
#include "ControlRig.h"
#include "Graph/ControlRigGraphNode.h"
#include "ControlRigBlueprint.h"
#include "Kismet2/Kismet2NameValidators.h"

#define LOCTEXT_NAMESPACE "ControlRigBlueprintUtils"

FName FControlRigBlueprintUtils::ValidateName(UBlueprint* InBlueprint, const FString& InName)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FString Name = InName;
	if (Name.StartsWith(TEXT("RigUnit_")))
	{
		Name.RightChopInline(8, false);
	}

	TSharedPtr<FKismetNameValidator> NameValidator;
	NameValidator = MakeShareable(new FKismetNameValidator(InBlueprint));

	// Clean up BaseName to not contain any invalid characters, which will mean we can never find a legal name no matter how many numbers we add
	if (NameValidator->IsValid(Name) == EValidatorResult::ContainsInvalidCharacters)
	{
		for (TCHAR& TestChar : Name)
		{
			for (TCHAR BadChar : UE_BLUEPRINT_INVALID_NAME_CHARACTERS)
			{
				if (TestChar == BadChar)
				{
					TestChar = TEXT('_');
					break;
				}
			}
		}
	}

	if (UClass* ParentClass = InBlueprint->ParentClass)
	{
		FFieldVariant ExistingField = FindUFieldOrFProperty(ParentClass, *Name);
		if (ExistingField)
		{
			Name = FString::Printf(TEXT("%s_%d"), *Name, 0);
		}
	}

	int32 Count = 0;
	FString BaseName = Name;
	while (NameValidator->IsValid(Name) != EValidatorResult::Ok)
	{
		// Calculate the number of digits in the number, adding 2 (1 extra to correctly count digits, another to account for the '_' that will be added to the name
		int32 CountLength = Count > 0 ? (int32)log((double)Count) + 2 : 2;

		// If the length of the final string will be too long, cut off the end so we can fit the number
		if (CountLength + BaseName.Len() > NameValidator->GetMaximumNameLength())
		{
			BaseName.LeftInline(NameValidator->GetMaximumNameLength() - CountLength);
		}
		Name = FString::Printf(TEXT("%s_%d"), *BaseName, Count);
		Count++;
	}

	return *Name;
}

void FControlRigBlueprintUtils::ForAllRigUnits(TFunction<void(UScriptStruct*)> InFunction)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	// Run over all unit types
	for(TObjectIterator<UStruct> StructIt; StructIt; ++StructIt)
	{
		if (*StructIt)
		{
			if(StructIt->IsChildOf(FRigUnit::StaticStruct()) && !StructIt->HasMetaData(FRigVMStruct::AbstractMetaName))
			{
				if (UScriptStruct* ScriptStruct = Cast<UScriptStruct>(*StructIt))
				{
					InFunction(ScriptStruct);
				}
			}
		}
	}
}

void FControlRigBlueprintUtils::HandleReconstructAllNodes(UBlueprint* InBlueprint)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	return HandleRefreshAllNodes(InBlueprint);
}

void FControlRigBlueprintUtils::HandleRefreshAllNodes(UBlueprint* InBlueprint)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if(UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(InBlueprint))
	{
		if (RigBlueprint->GetDefaultModel() == nullptr)
		{
			return;
		}

		TArray<UControlRigGraphNode*> AllNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass(RigBlueprint, AllNodes);

		for (UControlRigGraphNode* Node : AllNodes)
		{
			Node->SetFlags(RF_Transient);
		}

		for(UControlRigGraphNode* Node : AllNodes)
		{
			Node->ReconstructNode();
		}

		for (UControlRigGraphNode* Node : AllNodes)
		{
			Node->ClearFlags(RF_Transient);
		}
	}
}

void FControlRigBlueprintUtils::RemoveMemberVariableIfNotUsed(UBlueprint* Blueprint, const FName VarName, UControlRigGraphNode* ToBeDeleted)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if (Blueprint->IsA<UControlRigBlueprint>())
	{
		FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, VarName);
	}
}
#undef LOCTEXT_NAMESPACE