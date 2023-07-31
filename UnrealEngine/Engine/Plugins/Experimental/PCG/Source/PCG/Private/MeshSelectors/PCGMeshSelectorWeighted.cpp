// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshSelectors/PCGMeshSelectorWeighted.h"

#include "PCGPoint.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Helpers/PCGBlueprintHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTraits.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

#include "Engine/StaticMesh.h"
#include "Math/RandomStream.h"

void UPCGMeshSelectorWeighted::SelectInstances_Implementation(
	FPCGContext& Context, 
	const UPCGStaticMeshSpawnerSettings* Settings, 
	const UPCGSpatialData* InSpatialData,
	TArray<FPCGMeshInstanceList>& OutMeshInstances,
	UPCGPointData* OutPointData) const
{
	TArray<int> CumulativeWeights;

	int TotalWeight = 0;

	for (const FPCGMeshSelectorWeightedEntry& Entry : MeshEntries)
	{
		if (Entry.Weight <= 0)
		{
			PCGE_LOG_C(Verbose, &Context, "Entry found with weight <= 0");
			continue;
		}

		int32 Index = INDEX_NONE;
		FindOrAddInstanceList(OutMeshInstances, Entry.Mesh, Entry.bOverrideCollisionProfile, Entry.CollisionProfile, Entry.bOverrideMaterials, Entry.MaterialOverrides, Entry.CullStartDistance, Entry.CullEndDistance, Index);
		TotalWeight += Entry.Weight;
		CumulativeWeights.Add(TotalWeight);
	}

	if (TotalWeight <= 0)
	{
		return;
	}

	const UPCGPointData* PointData = InSpatialData->ToPointData(&Context);

	if (!PointData)
	{
		PCGE_LOG_C(Error, &Context, "Unable to get point data from input");
		return;
	}

	TArray<FPCGPoint>* OutPoints = nullptr;
	FPCGMetadataAttribute<FString>* OutAttribute = nullptr;
	TMap<TSoftObjectPtr<UStaticMesh>, PCGMetadataValueKey> MeshToValueKey;

	if (OutPointData)
	{
		check(OutPointData->Metadata);

		if (!OutPointData->Metadata->HasAttribute(Settings->OutAttributeName)) 
		{
			PCGE_LOG_C(Error, &Context, "Out attribute %s is not in the metadata", *Settings->OutAttributeName.ToString());
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
				PCGE_LOG_C(Error, &Context, "Out attribute is not of valid type FString");
			}
		}
	}

	// Assign points to entries
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGStaticMeshSpawnerElement::Execute::SelectEntries);

		for (const FPCGPoint& Point : PointData->GetPoints()) 
		{
			if (Point.Density <= 0.0f)
			{
				continue;
			}

			FRandomStream RandomSource = UPCGBlueprintHelpers::GetRandomStream(Point, Settings, Context.SourceComponent.Get());
			int RandomWeightedPick = RandomSource.RandRange(0, TotalWeight - 1);

			int RandomPick = 0;
			while (RandomPick < OutMeshInstances.Num() && CumulativeWeights[RandomPick] <= RandomWeightedPick)
			{
				++RandomPick;
			}

			if (RandomPick < OutMeshInstances.Num())
			{
				FPCGMeshInstanceList& InstanceList = OutMeshInstances[RandomPick];
				InstanceList.Instances.Emplace(Point);

				const TSoftObjectPtr<UStaticMesh>& Mesh = InstanceList.Mesh;

				if (OutPointData && OutAttribute)
				{
					PCGMetadataValueKey* OutValueKey = MeshToValueKey.Find(Mesh);
					if(!OutValueKey)
					{
						PCGMetadataValueKey ValueKey = OutAttribute->AddValue(Mesh.ToSoftObjectPath().ToString());
						OutValueKey = &MeshToValueKey.Add(Mesh, ValueKey);
					}
					
					check(OutValueKey);
					
					FPCGPoint& OutPoint = OutPoints->Add_GetRef(Point);
					OutPointData->Metadata->InitializeOnSet(OutPoint.MetadataEntry);
					OutAttribute->SetValueFromValueKey(OutPoint.MetadataEntry, *OutValueKey);
				}
			}
		}
	}
}
