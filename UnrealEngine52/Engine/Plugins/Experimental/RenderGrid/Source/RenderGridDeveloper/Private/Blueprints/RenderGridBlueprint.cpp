// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprints/RenderGridBlueprint.h"

#include "EdGraphSchema_K2_Actions.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "RenderGrid/RenderGrid.h"
#include "RenderGrid/RenderGridBlueprintGeneratedClass.h"


URenderGridBlueprint::URenderGridBlueprint()
{
	RenderGrid = CreateDefaultSubobject<URenderGrid>("RenderGrid");
	RenderGrid->SetFlags(RF_DefaultSubObject | RF_Transactional);
}

UClass* URenderGridBlueprint::GetBlueprintClass() const
{
	return URenderGridBlueprintGeneratedClass::StaticClass();
}

void URenderGridBlueprint::PostLoad()
{
	Super::PostLoad();

	PropagateAllPropertiesToInstances();

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

	OnChanged().RemoveAll(this);
	OnChanged().AddUObject(this, &URenderGridBlueprint::OnPostVariablesChange);

	OnPostVariablesChange(this);
}

void URenderGridBlueprint::RunOnInstances(const UE::RenderGrid::FRenderGridBlueprintRunOnInstancesCallback& Callback)
{
	if (UClass* MyRenderGridClass = GeneratedClass; IsValid(MyRenderGridClass))
	{
		if (URenderGrid* DefaultObject = Cast<URenderGrid>(MyRenderGridClass->GetDefaultObject(true)); IsValid(DefaultObject))
		{
			Callback.ExecuteIfBound(DefaultObject);

			TArray<UObject*> ArchetypeInstances;
			DefaultObject->GetArchetypeInstances(ArchetypeInstances);
			for (UObject* ArchetypeInstance : ArchetypeInstances)
			{
				if (URenderGrid* InstanceGrid = Cast<URenderGrid>(ArchetypeInstance); IsValid(InstanceGrid))
				{
					Callback.ExecuteIfBound(InstanceGrid);
				}
			}
		}
	}
}

void URenderGridBlueprint::Load()
{
	for (URenderGridJob* Job : RenderGrid->GetRenderGridJobsRef())
	{
		Job->Rename(nullptr, RenderGrid);
	}
}

void URenderGridBlueprint::Save()
{
	if (UClass* MyRenderGridClass = GeneratedClass; IsValid(MyRenderGridClass))
	{
		if (URenderGrid* DefaultObject = Cast<URenderGrid>(MyRenderGridClass->GetDefaultObject(true)); IsValid(DefaultObject))
		{
			for (URenderGridJob* Job : DefaultObject->GetRenderGridJobsRef())
			{
				Job->Rename(nullptr, DefaultObject);
			}
		}
	}
}

void URenderGridBlueprint::PropagateJobsToInstances()
{
	RunOnInstances(UE::RenderGrid::FRenderGridBlueprintRunOnInstancesCallback::CreateLambda([this](URenderGrid* Instance)
	{
		Instance->CopyJobs(RenderGrid);
	}));
}

void URenderGridBlueprint::PropagateAllPropertiesExceptJobsToInstances()
{
	RunOnInstances(UE::RenderGrid::FRenderGridBlueprintRunOnInstancesCallback::CreateLambda([this](URenderGrid* Instance)
	{
		Instance->CopyAllPropertiesExceptJobs(RenderGrid);
	}));
}

void URenderGridBlueprint::PropagateAllPropertiesToInstances()
{
	RunOnInstances(UE::RenderGrid::FRenderGridBlueprintRunOnInstancesCallback::CreateLambda([this](URenderGrid* Instance)
	{
		Instance->CopyAllProperties(RenderGrid);
	}));
}

void URenderGridBlueprint::PropagateJobsToAsset(URenderGrid* Instance)
{
	check(IsValid(Instance));
	RenderGrid->CopyJobs(Instance);
}

void URenderGridBlueprint::PropagateAllPropertiesExceptJobsToAsset(URenderGrid* Instance)
{
	check(IsValid(Instance));
	RenderGrid->CopyAllPropertiesExceptJobs(Instance);
}

void URenderGridBlueprint::PropagateAllPropertiesToAsset(URenderGrid* Instance)
{
	check(IsValid(Instance));
	RenderGrid->CopyAllProperties(Instance);
}

URenderGrid* URenderGridBlueprint::GetRenderGridWithBlueprintGraph() const
{
	URenderGrid* Result = GetRenderGrid();
	const_cast<URenderGridBlueprint*>(this)->RunOnInstances(UE::RenderGrid::FRenderGridBlueprintRunOnInstancesCallback::CreateLambda([&Result](URenderGrid* Instance)
	{
		if (!IsValid(Result) || Result->HasAnyFlags(RF_ClassDefaultObject | RF_DefaultSubObject))
		{
			Result = Instance;
		}
	}));
	return Result;
}

URenderGrid* URenderGridBlueprint::GetRenderGridClassDefaultObject() const
{
	if (UClass* MyRenderGridClass = GeneratedClass; IsValid(MyRenderGridClass))
	{
		if (URenderGrid* DefaultObject = Cast<URenderGrid>(MyRenderGridClass->GetDefaultObject(true)); IsValid(DefaultObject))
		{
			return DefaultObject;
		}
	}
	return nullptr;
}

void URenderGridBlueprint::OnPostVariablesChange(UBlueprint* InBlueprint)
{
	if (InBlueprint != this)
	{
		return;
	}

	bool bFoundChange = false;
	for (FBPVariableDescription& NewVariable : NewVariables)
	{
		uint64 PreviousPropertyFlags = NewVariable.PropertyFlags;
		bFoundChange = bFoundChange || NewVariable.HasMetaData(FBlueprintMetadata::MD_ExposeOnSpawn);

		// fix that's required for the way of saving/loading the render grid,
		//  the render grid instance (in the blueprint class) isn't of the generated class (but instead it's of the URenderGrid class itself), and so variables can't be saved or load,
		//  meaning that every variable has to be transient no matter what, otherwise saving or loading the render grid asset will cause a crash
		NewVariable.PropertyFlags |= CPF_DisableEditOnInstance;
		NewVariable.PropertyFlags &= ~CPF_ExposeOnSpawn;
		NewVariable.PropertyFlags |= CPF_Transient;
		NewVariable.RemoveMetaData(FBlueprintMetadata::MD_ExposeOnSpawn);

		bFoundChange = bFoundChange || (NewVariable.PropertyFlags != PreviousPropertyFlags);
	}

	if (bFoundChange)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(this);
	}
}
