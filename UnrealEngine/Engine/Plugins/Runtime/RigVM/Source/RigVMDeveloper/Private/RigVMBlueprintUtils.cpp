// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMBlueprintUtils.h"

#include "BlueprintActionDatabase.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "RigVMCore/RigVMStruct.h"
#include "UObject/UObjectIterator.h"
#include "EdGraph/RigVMEdGraphNode.h"
#include "RigVMBlueprint.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Stats/StatsHierarchical.h"

#define LOCTEXT_NAMESPACE "RigVMBlueprintUtils"

FName FRigVMBlueprintUtils::ValidateName(UBlueprint* InBlueprint, const FString& InName)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FString Name = InName;
	if (Name.StartsWith(TEXT("RigUnit_")))
	{
		Name.RightChopInline(8, EAllowShrinking::No);
	}
	else if (Name.StartsWith(TEXT("RigVMStruct_")))
	{
		Name.RightChopInline(12, EAllowShrinking::No);
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

void FRigVMBlueprintUtils::ForAllRigVMStructs(TFunction<void(UScriptStruct*)> InFunction)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	// Run over all unit types
	for(TObjectIterator<UStruct> StructIt; StructIt; ++StructIt)
	{
		if (*StructIt)
		{
			if(StructIt->IsChildOf(FRigVMStruct::StaticStruct()) && !StructIt->HasMetaData(FRigVMStruct::AbstractMetaName))
			{
				if (UScriptStruct* ScriptStruct = Cast<UScriptStruct>(*StructIt))
				{
					InFunction(ScriptStruct);
				}
			}
		}
	}
}

void FRigVMBlueprintUtils::HandleReconstructAllNodes(UBlueprint* InBlueprint)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	return HandleRefreshAllNodes(InBlueprint);
}

void FRigVMBlueprintUtils::HandleRefreshAllNodes(UBlueprint* InBlueprint)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
#ifdef WITH_EDITORONLY_DATA
	// Avoid refreshing EdGraph nodes during cook
	if (GIsCookerLoadingPackage)
	{
		return;
	}
	
	if(const URigVMBlueprint* RigVMBlueprint = Cast<URigVMBlueprint>(InBlueprint))
	{
		if (RigVMBlueprint->GetDefaultModel() == nullptr)
		{
			return;
		}

		TArray<URigVMEdGraphNode*> AllNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass(RigVMBlueprint, AllNodes);

		for (URigVMEdGraphNode* Node : AllNodes)
		{
			Node->SetFlags(RF_Transient);
		}

		for(URigVMEdGraphNode* Node : AllNodes)
		{
			Node->ReconstructNode();
		}

		for (URigVMEdGraphNode* Node : AllNodes)
		{
			Node->ClearFlags(RF_Transient);
		}
	}
#endif
}

void FRigVMBlueprintUtils::HandleAssetDeleted(const FAssetData& InAssetData)
{
	if (InAssetData.GetClass() && InAssetData.GetClass()->IsChildOf(URigVMBlueprint::StaticClass()))
	{
		// Make sure any RigVMBlueprint removes any TypeActions related to this asset (e.g. public functions)
		FBlueprintActionDatabase& ActionDatabase = FBlueprintActionDatabase::Get();
		ActionDatabase.ClearAssetActions(InAssetData.GetClass());
		ActionDatabase.RefreshClassActions(InAssetData.GetClass());
	}
}

void FRigVMBlueprintUtils::RemoveMemberVariableIfNotUsed(UBlueprint* Blueprint, const FName VarName)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if (Blueprint->IsA<URigVMBlueprint>())
	{
		FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, VarName);
	}
}
#undef LOCTEXT_NAMESPACE