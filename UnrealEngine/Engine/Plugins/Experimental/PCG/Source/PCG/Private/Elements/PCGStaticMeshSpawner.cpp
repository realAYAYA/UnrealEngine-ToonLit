// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGStaticMeshSpawner.h"

#include "PCGCommon.h"
#include "PCGHelpers.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGActorHelpers.h"

#include "InstancePackers/PCGInstancePackerBase.h"
#include "MeshSelectors/PCGMeshSelectorBase.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

UPCGStaticMeshSpawnerSettings::UPCGStaticMeshSpawnerSettings(const FObjectInitializer &ObjectInitializer)
{
	bUseSeed = true;

	MeshSelectorType = UPCGMeshSelectorWeighted::StaticClass();
	MeshSelectorInstance = ObjectInitializer.CreateDefaultSubobject<UPCGMeshSelectorWeighted>(this, TEXT("DefaultSelectorInstance"));
}

FPCGElementPtr UPCGStaticMeshSpawnerSettings::CreateElement() const
{
	return MakeShared<FPCGStaticMeshSpawnerElement>();
}

bool FPCGStaticMeshSpawnerElement::PrepareDataInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGStaticMeshSpawnerElement::PrepareDataInternal);
	// TODO : time-sliced implementation
	FPCGStaticMeshSpawnerContext* Context = static_cast<FPCGStaticMeshSpawnerContext*>(InContext);
	const UPCGStaticMeshSpawnerSettings* Settings = Context->GetInputSettings<UPCGStaticMeshSpawnerSettings>();
	check(Settings);

	if (!Settings->MeshSelectorInstance)
	{
		PCGE_LOG(Error, "Invalid MeshSelectorInstance");
		return true;
	}

	// perform mesh selection
	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	// Forward any non-input data
	Outputs.Append(Context->InputData.GetAllSettings());

	const bool bOutputPinConnected = Context->Node && Context->Node->IsOutputPinConnected(PCGPinConstants::DefaultOutputLabel);

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);

		if (!SpatialData)
		{
			PCGE_LOG(Error, "Invalid input data");
			continue;
		}

		AActor* TargetActor = SpatialData->TargetActor.Get();

		if (!TargetActor)
		{
			PCGE_LOG(Error, "Invalid target actor");
			continue;
		}

		UPCGPointData* OutputPointData = nullptr;

		if (bOutputPinConnected || Settings->bForceConnectOutput)
		{
			FPCGTaggedData& Output = Outputs.Add_GetRef(Input);

			OutputPointData = NewObject<UPCGPointData>();
			OutputPointData->InitializeFromData(SpatialData);

			if (OutputPointData->Metadata->HasAttribute(Settings->OutAttributeName))
			{
				OutputPointData->Metadata->DeleteAttribute(Settings->OutAttributeName);
				PCGE_LOG(Verbose, "Metadata attribute %s is being overwritten in the output data", *Settings->OutAttributeName.ToString());
			}

			OutputPointData->Metadata->CreateStringAttribute(Settings->OutAttributeName, FName(NAME_None).ToString(), /*bAllowsInterpolation=*/false);

			Output.Data = OutputPointData;
		}

		TArray<FPCGMeshInstanceList> MeshInstances;
		Settings->MeshSelectorInstance->SelectInstances(*Context, Settings, SpatialData, MeshInstances, OutputPointData);

		TArray<FPCGPackedCustomData> PackedCustomData;
		PackedCustomData.SetNum(MeshInstances.Num());
		if (Settings->InstancePackerInstance)
		{
			for(int32 InstanceListIndex = 0; InstanceListIndex < MeshInstances.Num(); ++InstanceListIndex)
			{
				Settings->InstancePackerInstance->PackInstances(*Context, SpatialData, MeshInstances[InstanceListIndex], PackedCustomData[InstanceListIndex]);
			}
		}

		FPCGStaticMeshSpawnerContext::FPackedInstanceListData& InstanceListData = Context->MeshInstancesData.Emplace_GetRef();
		InstanceListData.SpatialData = SpatialData;
		InstanceListData.MeshInstances = MoveTemp(MeshInstances);
		InstanceListData.PackedCustomData = MoveTemp(PackedCustomData);
	}

	return true;
}

bool FPCGStaticMeshSpawnerElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGStaticMeshSpawnerElement::Execute);
	FPCGStaticMeshSpawnerContext* Context = static_cast<FPCGStaticMeshSpawnerContext*>(InContext);
	const UPCGStaticMeshSpawnerSettings* Settings = Context->GetInputSettings<UPCGStaticMeshSpawnerSettings>();
	check(Settings);

	while(!Context->MeshInstancesData.IsEmpty())
	{
		const FPCGStaticMeshSpawnerContext::FPackedInstanceListData& InstanceList = Context->MeshInstancesData.Last();
		check(InstanceList.MeshInstances.Num() == InstanceList.PackedCustomData.Num());

		if (InstanceList.SpatialData->TargetActor.IsValid())
		{
			for (int32 DataIndex = 0; DataIndex < InstanceList.MeshInstances.Num(); ++DataIndex)
			{
				SpawnStaticMeshInstances(Context, InstanceList.MeshInstances[DataIndex], InstanceList.SpatialData->TargetActor.Get(), InstanceList.PackedCustomData[DataIndex]);
			}
		}

		Context->MeshInstancesData.RemoveAtSwap(Context->MeshInstancesData.Num() - 1);

		if (Context->ShouldStop())
		{
			break;
		}
	}

	return Context->MeshInstancesData.IsEmpty();
}

FPCGContext* FPCGStaticMeshSpawnerElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGStaticMeshSpawnerContext* Context = new FPCGStaticMeshSpawnerContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}

bool FPCGStaticMeshSpawnerElement::CanExecuteOnlyOnMainThread(FPCGContext* Context) const
{
	return Context->CurrentPhase == EPCGExecutionPhase::Execute;
}

void FPCGStaticMeshSpawnerElement::SpawnStaticMeshInstances(FPCGContext* Context, const FPCGMeshInstanceList& InstanceList, AActor* TargetActor, const FPCGPackedCustomData& PackedCustomData) const
{
	// Populate the (H)ISM from the previously prepared entries
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGStaticMeshSpawnerElement::Execute::PopulateISMs);

	if (InstanceList.Instances.Num() == 0)
	{
		return;
	}

	// Todo: we could likely pre-load these meshes asynchronously in the settings
	UStaticMesh* LoadedMesh = InstanceList.Mesh.LoadSynchronous();

	if (!LoadedMesh)
	{
		return;
	}

	FPCGISMCBuilderParameters Params;
	Params.Mesh = LoadedMesh;

	if (InstanceList.bOverrideCollisionProfile)
	{
		Params.CollisionProfile = InstanceList.CollisionProfile.Name;
	}

	if (InstanceList.bOverrideMaterials)
	{
		Params.MaterialOverrides = InstanceList.MaterialOverrides;
	}

	Params.CullStartDistance = InstanceList.CullStartDistance;
	Params.CullEndDistance = InstanceList.CullEndDistance;

	Params.NumCustomDataFloats = PackedCustomData.NumCustomDataFloats;

	UInstancedStaticMeshComponent* ISMC = UPCGActorHelpers::GetOrCreateISMC(TargetActor, Context->SourceComponent.Get(), Params);

	const int32 PreExistingInstanceCount = ISMC->GetInstanceCount();
	const int32 NewInstanceCount = InstanceList.Instances.Num();
	const int32 NumCustomDataFloats = PackedCustomData.NumCustomDataFloats;

	check((ISMC->NumCustomDataFloats == 0 && PreExistingInstanceCount == 0) || ISMC->NumCustomDataFloats == NumCustomDataFloats);
	ISMC->NumCustomDataFloats = NumCustomDataFloats;

	// The index in ISMC PerInstanceSMCustomData where we should pick up to begin inserting new floats
	const int32 PreviousCustomDataOffset = PreExistingInstanceCount * NumCustomDataFloats;

	// Populate the ISM instances
	TArray<FTransform> Instances;
	Instances.Reserve(NewInstanceCount);
	for (int32 InstanceIndex = 0; InstanceIndex < NewInstanceCount; ++InstanceIndex)
	{
		Instances.Emplace(InstanceList.Instances[InstanceIndex].Transform);
	}

	ISMC->AddInstances(Instances, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true);

	// Copy new CustomData into the ISMC PerInstanceSMCustomData
	if (NumCustomDataFloats > 0)
	{
		check(PreviousCustomDataOffset + PackedCustomData.CustomData.Num() == ISMC->PerInstanceSMCustomData.Num());
		FMemory::Memcpy(&ISMC->PerInstanceSMCustomData[PreviousCustomDataOffset], &PackedCustomData.CustomData[0], PackedCustomData.CustomData.Num() * sizeof(float));

		// Force recreation of the render data when proxy is created
		ISMC->InstanceUpdateCmdBuffer.NumEdits++;
	}

	ISMC->UpdateBounds();

	PCGE_LOG(Verbose, "Added %d instances of %s on actor %s", InstanceList.Instances.Num(), *InstanceList.Mesh->GetFName().ToString(), *TargetActor->GetFName().ToString());
}

void UPCGStaticMeshSpawnerSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (Meshes_DEPRECATED.Num() != 0)
	{
		SetMeshSelectorType(UPCGMeshSelectorWeighted::StaticClass());

		UPCGMeshSelectorWeighted* MeshSelector = CastChecked<UPCGMeshSelectorWeighted>(MeshSelectorInstance);

		for (const FPCGStaticMeshSpawnerEntry& Entry : Meshes_DEPRECATED)
		{
			FPCGMeshSelectorWeightedEntry& NewEntry = MeshSelector->MeshEntries.Emplace_GetRef(Entry.Mesh, Entry.Weight);
			NewEntry.CollisionProfile = Entry.CollisionProfile;
			NewEntry.bOverrideCollisionProfile = Entry.bOverrideCollisionProfile;
		}

		Meshes_DEPRECATED.Reset();
	}
#endif

	if (!MeshSelectorInstance)
	{
		RefreshMeshSelector();
	}

	if (!InstancePackerInstance)
	{
		RefreshInstancePacker();
	}
}

#if WITH_EDITOR
void UPCGStaticMeshSpawnerSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) 
{
	if (PropertyChangedEvent.Property)
	{
		const FName& PropertyName = PropertyChangedEvent.Property->GetFName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UPCGStaticMeshSpawnerSettings, MeshSelectorType))
		{
			RefreshMeshSelector();
		} 
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UPCGStaticMeshSpawnerSettings, InstancePackerType))
		{
			RefreshInstancePacker();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UPCGStaticMeshSpawnerSettings::SetMeshSelectorType(TSubclassOf<UPCGMeshSelectorBase> InMeshSelectorType) 
{
	if (!MeshSelectorInstance || InMeshSelectorType != MeshSelectorType)
	{
		if (InMeshSelectorType != MeshSelectorType)
		{
			MeshSelectorType = InMeshSelectorType;
		}
		
		RefreshMeshSelector();
	}
}

void UPCGStaticMeshSpawnerSettings::SetInstancePackerType(TSubclassOf<UPCGInstancePackerBase> InInstancePackerType) 
{
	if (!InstancePackerInstance || InInstancePackerType != InstancePackerType)
	{
		if (InInstancePackerType != InstancePackerType)
		{
			InstancePackerType = InInstancePackerType;
		}
		
		RefreshInstancePacker();
	}
}

void UPCGStaticMeshSpawnerSettings::RefreshMeshSelector()
{
	if (MeshSelectorType)
	{
		MeshSelectorInstance = NewObject<UPCGMeshSelectorBase>(this, MeshSelectorType);
	}
	else
	{
		MeshSelectorInstance = nullptr;
	}
}

void UPCGStaticMeshSpawnerSettings::RefreshInstancePacker()
{
	if (InstancePackerType)
	{
		InstancePackerInstance = NewObject<UPCGInstancePackerBase>(this, InstancePackerType);
	}
	else
	{
		InstancePackerInstance = nullptr;
	}
}
