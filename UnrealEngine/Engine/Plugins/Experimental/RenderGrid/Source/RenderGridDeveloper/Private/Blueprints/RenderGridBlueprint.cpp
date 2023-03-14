// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprints/RenderGridBlueprint.h"

#include "EdGraphSchema_K2_Actions.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "RenderGrid/RenderGrid.h"
#include "RenderGrid/RenderGridBlueprintGeneratedClass.h"


UClass* URenderGridBlueprint::GetBlueprintClass() const
{
	return URenderGridBlueprintGeneratedClass::StaticClass();
}

void URenderGridBlueprint::PostLoad()
{
	Super::PostLoad();

	if (UbergraphPages.IsEmpty() || ((UbergraphPages.Num() == 1) && UbergraphPages[0]->Nodes.IsEmpty()))
	{
		if (!UbergraphPages.IsEmpty())
		{
			for (const TObjectPtr<UEdGraph>& Graph : UbergraphPages)
			{
				Graph->MarkAsGarbage();
				Graph->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders);
			}
			UbergraphPages.Empty();
		}

		UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(this, UEdGraphSchema_K2::GN_EventGraph, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		NewGraph->bAllowDeletion = false;

		{// create every RenderGrid blueprint event >>
			int32 i = 0;
			for (const FString& Event : URenderGrid::GetBlueprintImplementableEvents())
			{
				int32 InOutNodePosY = (i * 256) - 48;
				FKismetEditorUtilities::AddDefaultEventNode(this, NewGraph, FName(Event), URenderGrid::StaticClass(), InOutNodePosY);
				i++;
			}
		}// create every RenderGrid blueprint event <<

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(this);
		FBlueprintEditorUtils::AddUbergraphPage(this, NewGraph);
		LastEditedDocuments.AddUnique(NewGraph);
	}

	FCoreUObjectDelegates::OnObjectModified.RemoveAll(this);
	OnChanged().RemoveAll(this);
	FCoreUObjectDelegates::OnObjectModified.AddUObject(this, &URenderGridBlueprint::OnPreVariablesChange);
	OnChanged().AddUObject(this, &URenderGridBlueprint::OnPostVariablesChange);

	OnPreVariablesChange(this);
	OnPostVariablesChange(this);
}

void URenderGridBlueprint::OnPreVariablesChange(UObject* InObject)
{
	if (InObject != this)
	{
		return;
	}
	LastNewVariables = NewVariables;
}

void URenderGridBlueprint::OnPostVariablesChange(UBlueprint* InBlueprint)
{
	if (InBlueprint != this)
	{
		return;
	}

	bool bFoundChange = false;

	TMap<FGuid, int32> NewVariablesByGuid;
	for (int32 VarIndex = 0; VarIndex < NewVariables.Num(); VarIndex++)
	{
		NewVariablesByGuid.Add(NewVariables[VarIndex].VarGuid, VarIndex);
	}

	TMap<FGuid, int32> OldVariablesByGuid;
	for (int32 VarIndex = 0; VarIndex < LastNewVariables.Num(); VarIndex++)
	{
		OldVariablesByGuid.Add(LastNewVariables[VarIndex].VarGuid, VarIndex);
	}

	for (FBPVariableDescription& OldVariable : LastNewVariables)
	{
		if (!NewVariablesByGuid.Contains(OldVariable.VarGuid))
		{
			bFoundChange = true;
			OnVariableRemoved(OldVariable);
		}
	}

	for (FBPVariableDescription& NewVariable : NewVariables)
	{
		if (!OldVariablesByGuid.Contains(NewVariable.VarGuid))
		{
			bFoundChange = true;
			OnVariableAdded(NewVariable);
			continue;
		}

		int32 OldVarIndex = OldVariablesByGuid.FindChecked(NewVariable.VarGuid);
		const FBPVariableDescription& OldVariable = LastNewVariables[OldVarIndex];
		if (OldVariable.VarName != NewVariable.VarName)
		{
			bFoundChange = true;
			OnVariableRenamed(NewVariable, OldVariable.VarName, NewVariable.VarName);
		}

		if (OldVariable.VarType != NewVariable.VarType)
		{
			bFoundChange = true;
			OnVariableTypeChanged(NewVariable, OldVariable.VarType, NewVariable.VarType);
		}

		if (OldVariable.PropertyFlags != NewVariable.PropertyFlags)
		{
			bFoundChange = true;
			OnVariablePropertyFlagsChanged(NewVariable, OldVariable.PropertyFlags, NewVariable.PropertyFlags);
		}
	}

	LastNewVariables = NewVariables;

	if (bFoundChange)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(this);
	}
}

void URenderGridBlueprint::OnVariableAdded(FBPVariableDescription& InVar)
{
	MakeVariableTransientUnlessInstanceEditable(InVar);
}

void URenderGridBlueprint::OnVariablePropertyFlagsChanged(FBPVariableDescription& InVar, const uint64 InOldVarPropertyFlags, const uint64 InNewVarPropertyFlags)
{
	if ((InOldVarPropertyFlags & CPF_DisableEditOnInstance) != (InNewVarPropertyFlags & CPF_DisableEditOnInstance))// if the value of [Instance Editable] changed
	{
		MakeVariableTransientUnlessInstanceEditable(InVar);
	}
}

void URenderGridBlueprint::MakeVariableTransientUnlessInstanceEditable(FBPVariableDescription& InVar)
{
	if ((InVar.PropertyFlags & CPF_DisableEditOnInstance) == 0)// if [Instance Editable]
	{
		InVar.PropertyFlags &= ~CPF_Transient;// set [Transient] to false
	}
	else
	{
		InVar.PropertyFlags |= CPF_Transient;// set [Transient] to true
	}
}
