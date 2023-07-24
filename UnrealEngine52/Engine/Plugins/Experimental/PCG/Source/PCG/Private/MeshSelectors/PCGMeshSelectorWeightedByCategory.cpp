// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshSelectors/PCGMeshSelectorWeightedByCategory.h"

#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Elements/PCGStaticMeshSpawnerContext.h"
#include "Helpers/PCGBlueprintHelpers.h"

#include "Math/RandomStream.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGMeshSelectorWeightedByCategory)

#define LOCTEXT_NAMESPACE "PCGMeshSelectorWeightedByCategory"

#if WITH_EDITOR
void FPCGWeightedByCategoryEntryList::ApplyDeprecation()
{
	for (FPCGMeshSelectorWeightedEntry& Entry : WeightedMeshEntries)
	{
		Entry.ApplyDeprecation();
	}
}
#endif

void UPCGMeshSelectorWeightedByCategory::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	for (FPCGWeightedByCategoryEntryList& Entry : Entries)
	{
		Entry.ApplyDeprecation();
	}
#endif
}

bool UPCGMeshSelectorWeightedByCategory::SelectInstances(
	FPCGStaticMeshSpawnerContext& Context,
	const UPCGStaticMeshSpawnerSettings* Settings,
	const UPCGPointData* InPointData,
	TArray<FPCGMeshInstanceList>& OutMeshInstances,
	UPCGPointData* OutPointData) const
{
	if (!InPointData)
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, LOCTEXT("InputMissingData", "Missing input data"));
		return true;
	}

	if (!InPointData->Metadata)
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, LOCTEXT("InputMissingMetadata", "Unable to get metadata from input"));
		return true;
	}

	if (!InPointData->Metadata->HasAttribute(CategoryAttribute))
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, FText::Format(LOCTEXT("InputMissingAttribute", "Attribute '{0}' is not in the metadata"), FText::FromName(CategoryAttribute)));
		return true;
	}

	const FPCGMetadataAttributeBase* AttributeBase = InPointData->Metadata->GetConstAttribute(CategoryAttribute);
	check(AttributeBase);

	// TODO: support enum type as well
	if (AttributeBase->GetTypeId() != PCG::Private::MetadataTypes<FString>::Id)
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, FText::Format(LOCTEXT("AttributeInvalidType", "Attribute '{0}' is not of valid type FString"), FText::FromName(CategoryAttribute)));
		return true;
	}

	const FPCGMetadataAttribute<FString>* Attribute = static_cast<const FPCGMetadataAttribute<FString>*>(AttributeBase);

	// maps a CategoryEntry ValueKey to the meshes and precomputed weight data 
	TMap<PCGMetadataValueKey, FPCGInstancesAndWeights>& CategoryEntryToInstancesAndWeights = Context.CategoryEntryToInstancesAndWeights;

	// unmarked points will fallback to the MeshEntries associated with the DefaultValueKey
	PCGMetadataValueKey DefaultValueKey = PCGDefaultValueKey;

	// Setup
	if (Context.CurrentPointIndex == 0)
	{
		for (const FPCGWeightedByCategoryEntryList& Entry : Entries)
		{
			if (Entry.WeightedMeshEntries.Num() == 0)
			{
				PCGE_LOG_C(Verbose, LogOnly, &Context, FText::Format(LOCTEXT("EmptyEntryInCategory", "Empty entry found in category '{0}'"), FText::FromString(Entry.CategoryEntry)));
				continue;
			}

			PCGMetadataValueKey ValueKey = Attribute->FindValue<FString>(Entry.CategoryEntry);

			if (ValueKey == PCGDefaultValueKey)
			{
				PCGE_LOG_C(Verbose, LogOnly, &Context, FText::Format(LOCTEXT("InvalidCategory", "Invalid category '{0}'"), FText::FromString(Entry.CategoryEntry)));
				continue;
			}

			FPCGInstancesAndWeights* InstancesAndWeights = CategoryEntryToInstancesAndWeights.Find(ValueKey);

			if (InstancesAndWeights)
			{
				PCGE_LOG_C(Warning, GraphAndLog, &Context, FText::Format(LOCTEXT("DuplicateEntry", "Duplicate entry found in category '{0}'. Subsequent entries are ignored."), FText::FromString(Entry.CategoryEntry)));
				continue;
			}

			if (Entry.IsDefault)
			{
				if (DefaultValueKey == PCGDefaultValueKey)
				{
					DefaultValueKey = ValueKey;
				}
				else
				{
					PCGE_LOG_C(Warning, GraphAndLog, &Context, LOCTEXT("DuplicateDefaultEntry", "Duplicate default entry found. Subsequent default entries are ignored."));
				}
			}

			InstancesAndWeights = &CategoryEntryToInstancesAndWeights.Add(ValueKey, FPCGInstancesAndWeights());

			int TotalWeight = 0;
			for (const FPCGMeshSelectorWeightedEntry& WeightedEntry : Entry.WeightedMeshEntries)
			{
				if (WeightedEntry.Weight <= 0)
				{
					PCGE_LOG_C(Verbose, LogOnly, &Context, FText::Format(LOCTEXT("ZeroOrNegativeWeight", "Entry found with weight <= 0 in category '{0}'"), FText::FromString(Entry.CategoryEntry)));
					continue;
				}

				TArray<FPCGMeshInstanceList>& PickEntry = InstancesAndWeights->MeshInstances.Emplace_GetRef();
				PickEntry.Emplace_GetRef(WeightedEntry.Descriptor);

				// precompute the weights
				TotalWeight += WeightedEntry.Weight;
				InstancesAndWeights->CumulativeWeights.Add(TotalWeight);
			}

			// if no weighted entries were collected, discard this InstancesAndWeights
			if (InstancesAndWeights->CumulativeWeights.Num() == 0)
			{
				CategoryEntryToInstancesAndWeights.Remove(ValueKey);
			}
		}
	}

	FPCGMeshMaterialOverrideHelper& MaterialOverrideHelper = Context.MaterialOverrideHelper;
	if (!MaterialOverrideHelper.IsInitialized())
	{
		MaterialOverrideHelper.Initialize(Context, bUseAttributeMaterialOverrides, MaterialOverrideAttributes, InPointData->Metadata);
	}

	if (!MaterialOverrideHelper.IsValid())
	{
		return true;
	}

	TArray<FPCGPoint>* OutPoints = nullptr;
	FPCGMetadataAttribute<FString>* OutAttribute = nullptr;

	if (OutPointData)
	{
		check(OutPointData->Metadata);

		if (!OutPointData->Metadata->HasAttribute(Settings->OutAttributeName)) 
		{
			PCGE_LOG_C(Error, GraphAndLog, &Context, FText::Format(LOCTEXT("AttributeNotInMetadata", "Out attribute '{0}' is not in the metadata"), FText::FromName(Settings->OutAttributeName)));
		}

		FPCGMetadataAttributeBase* OutAttributeBase = OutPointData->Metadata->GetMutableAttribute(Settings->OutAttributeName);

		if (OutAttributeBase)
		{
			if (OutAttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FString>::Id)
			{
				OutAttribute = static_cast<FPCGMetadataAttribute<FString>*>(OutAttributeBase);
				OutPoints = &OutPointData->GetMutablePoints();
			}
			else
			{
				PCGE_LOG_C(Error, GraphAndLog, &Context, LOCTEXT("TypeNotFString", "Out attribute is not of valid type FString"));
			}
		}
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGStaticMeshSpawnerElement::Execute::SelectEntries);

	int32 CurrentPointIndex = Context.CurrentPointIndex;
	int32 LastCheckpointIndex = CurrentPointIndex;
	constexpr int32 TimeSlicingCheckFrequency = 1024;
	TMap<TSoftObjectPtr<UStaticMesh>, PCGMetadataValueKey>& MeshToValueKey = Context.MeshToValueKey;

	// Assign points to entries
	const TArray<FPCGPoint>& Points = InPointData->GetPoints();
	while(CurrentPointIndex < Points.Num())
	{
		const FPCGPoint& Point = Points[CurrentPointIndex++];

		if (Point.Density <= 0.0f)
		{
			continue;
		}

		PCGMetadataValueKey ValueKey = Attribute->GetValueKey(Point.MetadataEntry);

		// if no mesh list was processed for this attribute value, fallback to the default mesh list
		FPCGInstancesAndWeights* InstancesAndWeights = CategoryEntryToInstancesAndWeights.Find(ValueKey);
		if (!InstancesAndWeights)
		{
			if (DefaultValueKey != PCGDefaultValueKey)
			{
				InstancesAndWeights = CategoryEntryToInstancesAndWeights.Find(DefaultValueKey);
				check(InstancesAndWeights);
			}
			else
			{
				continue;
			}
		}

		const int TotalWeight = InstancesAndWeights->CumulativeWeights.Last();

		FRandomStream RandomSource = UPCGBlueprintHelpers::GetRandomStream(Point, Settings, Context.SourceComponent.Get());
		int RandomWeightedPick = RandomSource.RandRange(0, TotalWeight - 1);

		int RandomPick = 0;
		while(RandomPick < InstancesAndWeights->MeshInstances.Num() && InstancesAndWeights->CumulativeWeights[RandomPick] <= RandomWeightedPick)
		{
			++RandomPick;
		}

		if(RandomPick < InstancesAndWeights->MeshInstances.Num())
		{
			const bool bNeedsReverseCulling = (Point.Transform.GetDeterminant() < 0);
			FPCGMeshInstanceList& InstanceList = PCGMeshSelectorWeighted::GetInstanceList(InstancesAndWeights->MeshInstances[RandomPick], bUseAttributeMaterialOverrides, MaterialOverrideHelper.GetMaterialOverrides(Point.MetadataEntry), bNeedsReverseCulling);
			InstanceList.Instances.Emplace(Point.Transform);
			InstanceList.InstancesMetadataEntry.Emplace(Point.MetadataEntry);

			const TSoftObjectPtr<UStaticMesh>& Mesh = InstanceList.Descriptor.StaticMesh;

			if (OutPointData && OutAttribute)
			{
				PCGMetadataValueKey* OutValueKey = MeshToValueKey.Find(Mesh);
				if(!OutValueKey)
				{
					PCGMetadataValueKey TempValueKey = OutAttribute->AddValue(Mesh.ToSoftObjectPath().ToString());
					OutValueKey = &MeshToValueKey.Add(Mesh, TempValueKey);
				}
				
				check(OutValueKey);
				
				FPCGPoint& OutPoint = OutPoints->Add_GetRef(Point);
				OutPointData->Metadata->InitializeOnSet(OutPoint.MetadataEntry);
				OutAttribute->SetValueFromValueKey(OutPoint.MetadataEntry, *OutValueKey);
			}
		}

		// Check if we should stop here and continue in a subsequent call
		if (CurrentPointIndex - LastCheckpointIndex >= TimeSlicingCheckFrequency)
		{
			if (Context.ShouldStop())
			{
				break;
			}
			else
			{
				LastCheckpointIndex = CurrentPointIndex;
			}
		}
	}

	Context.CurrentPointIndex = CurrentPointIndex;

	if (CurrentPointIndex == Points.Num())
	{
		// Collapse to OutMeshInstances
		for (TPair<PCGMetadataValueKey, FPCGInstancesAndWeights>& Entry : CategoryEntryToInstancesAndWeights)
		{
			for (TArray<FPCGMeshInstanceList>& PickedMeshInstances : Entry.Value.MeshInstances)
			{
				for (FPCGMeshInstanceList& PickedMeshInstanceEntry : PickedMeshInstances)
				{
					OutMeshInstances.Emplace(MoveTemp(PickedMeshInstanceEntry));
				}
			}
		}

		return true;
	}
	else
	{
		return false;
	}
}

#undef LOCTEXT_NAMESPACE
