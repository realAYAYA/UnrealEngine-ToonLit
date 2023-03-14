// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshSelectors/PCGMeshSelectorByAttribute.h"

#include "Elements/PCGPointFilter.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTraits.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

void UPCGMeshSelectorByAttribute::SelectInstances_Implementation(
	FPCGContext& Context, 
	const UPCGStaticMeshSpawnerSettings* Settings, 
	const UPCGSpatialData* InSpatialData, 
	TArray<FPCGMeshInstanceList>& OutMeshInstances,
	UPCGPointData* OutPointData) const
{
	const UPCGPointData* PointData = InSpatialData->ToPointData(&Context);

	if (!PointData)
	{
		PCGE_LOG_C(Error, &Context, "Unable to get point data from input");
		return;
	}

	if (!PointData->Metadata)
	{
		PCGE_LOG_C(Error, &Context, "Unable to get metadata from input");
		return;
	}

	if (!PointData->Metadata->HasAttribute(AttributeName)) 
	{
		PCGE_LOG_C(Error, &Context, "Attribute %s is not in the metadata", *AttributeName.ToString());
		return;
	}

	const FPCGMetadataAttributeBase* AttributeBase = PointData->Metadata->GetConstAttribute(AttributeName);
	check(AttributeBase);

	if (AttributeBase->GetTypeId() != PCG::Private::MetadataTypes<FString>::Id)
	{
		PCGE_LOG_C(Error, &Context, "Attribute is not of valid type FString");
		return;
	}

	const FPCGMetadataAttribute<FString>* Attribute = static_cast<const FPCGMetadataAttribute<FString>*>(AttributeBase);

	TMap<PCGMetadataValueKey, TSoftObjectPtr<UStaticMesh>> ValueKeyToMesh;

	// ByAttribute takes in SoftObjectPaths per point in the metadata, so we can pass those directly into the outgoing pin if it exists
	if (OutPointData)
	{
		OutPointData->SetPoints(PointData->GetPoints()); 
		OutPointData->Metadata->DeleteAttribute(Settings->OutAttributeName);
		OutPointData->Metadata->CopyAttribute(PointData->Metadata, AttributeName, Settings->OutAttributeName);
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGStaticMeshSpawnerElement::Execute::SelectEntries);

	// Assign points to entries
	for (const FPCGPoint& Point : PointData->GetPoints()) 
	{
		if (Point.Density <= 0.0f)
		{
			continue;
		}

		PCGMetadataValueKey ValueKey = Attribute->GetValueKey(Point.MetadataEntry);
		TSoftObjectPtr<UStaticMesh>* NewMesh = ValueKeyToMesh.Find(ValueKey);
		TSoftObjectPtr<UStaticMesh> Mesh = nullptr;

		// if this ValueKey has not been seen before, let's cache it for the future
		if (!NewMesh)
		{
			FSoftObjectPath MeshPath(Attribute->GetValue(ValueKey));
			Mesh = TSoftObjectPtr<UStaticMesh>(MeshPath);

			ValueKeyToMesh.Add(ValueKey, Mesh);
		}
		else
		{
			Mesh = *NewMesh;
		}

		if (Mesh.IsNull())
		{
			PCGE_LOG_C(Error, &Context, "Invalid object path.");
			continue;
		}

		int32 Index = INDEX_NONE;
		FindOrAddInstanceList(OutMeshInstances, Mesh, bOverrideCollisionProfile, CollisionProfile, bOverrideMaterials, MaterialOverrides, CullStartDistance, CullEndDistance, Index);
		OutMeshInstances[Index].Instances.Emplace(Point);
	}
}
