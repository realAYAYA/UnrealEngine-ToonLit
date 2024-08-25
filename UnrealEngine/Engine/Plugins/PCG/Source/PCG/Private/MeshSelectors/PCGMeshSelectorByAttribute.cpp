// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshSelectors/PCGMeshSelectorByAttribute.h"

#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Elements/PCGStaticMeshSpawnerContext.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "MeshSelectors/PCGMeshSelectorBase.h"
#include "Metadata/PCGObjectPropertyOverride.h"
#include "Metadata/PCGMetadataPartitionCommon.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "Engine/StaticMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGMeshSelectorByAttribute)

#define LOCTEXT_NAMESPACE "PCGMeshSelectorByAttribute"

namespace PCGMeshSelectorAttribute
{
	// Returns variation based on mesh, material overrides and reverse culling
	FPCGMeshInstanceList& GetInstanceList(
		TArray<FPCGMeshInstanceList>& InstanceLists,
		const FSoftISMComponentDescriptor& TemplateDescriptor,
		TSoftObjectPtr<UStaticMesh> Mesh,
		const TArray<TSoftObjectPtr<UMaterialInterface>>& MaterialOverrides,
		bool bReverseCulling,
		const int AttributePartitionIndex = INDEX_NONE)
	{
		for (FPCGMeshInstanceList& InstanceList : InstanceLists)
		{
			if (InstanceList.Descriptor.StaticMesh == Mesh &&
				InstanceList.Descriptor.bReverseCulling == bReverseCulling &&
				InstanceList.Descriptor.OverrideMaterials == MaterialOverrides &&
				InstanceList.AttributePartitionIndex == AttributePartitionIndex)
			{
				return InstanceList;
			}
		}

		FPCGMeshInstanceList& NewInstanceList = InstanceLists.Emplace_GetRef(TemplateDescriptor);
		NewInstanceList.Descriptor.StaticMesh = Mesh;
		NewInstanceList.Descriptor.OverrideMaterials = MaterialOverrides;
		NewInstanceList.Descriptor.bReverseCulling = bReverseCulling;
		NewInstanceList.AttributePartitionIndex = AttributePartitionIndex;

		return NewInstanceList;
	}
}

void UPCGMeshSelectorByAttribute::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (bOverrideMaterials_DEPRECATED)
	{
		MaterialOverrideMode_DEPRECATED = EPCGMeshSelectorMaterialOverrideMode::StaticOverride;
		bOverrideMaterials_DEPRECATED = false;
	}

	if (bOverrideCollisionProfile_DEPRECATED ||
		CollisionProfile_DEPRECATED.Name != UCollisionProfile::NoCollision_ProfileName ||
		MaterialOverrides_DEPRECATED.Num() > 0 ||
		MaterialOverrideMode_DEPRECATED != EPCGMeshSelectorMaterialOverrideMode::NoOverride ||
		CullStartDistance_DEPRECATED != 0 ||
		CullEndDistance_DEPRECATED != 0 ||
		WorldPositionOffsetDisableDistance_DEPRECATED != 0)
	{
		if (bOverrideCollisionProfile_DEPRECATED)
		{
			TemplateDescriptor.bUseDefaultCollision = false;
			TemplateDescriptor.BodyInstance.SetCollisionProfileName(CollisionProfile_DEPRECATED.Name);
		}
		else
		{
			TemplateDescriptor.bUseDefaultCollision = true;
		}

		if (MaterialOverrideMode_DEPRECATED != EPCGMeshSelectorMaterialOverrideMode::NoOverride)
		{
			TemplateDescriptor.OverrideMaterials = MaterialOverrides_DEPRECATED;
		}
		
		TemplateDescriptor.InstanceStartCullDistance = CullStartDistance_DEPRECATED;
		TemplateDescriptor.InstanceEndCullDistance = CullEndDistance_DEPRECATED;
		TemplateDescriptor.WorldPositionOffsetDisableDistance = WorldPositionOffsetDisableDistance_DEPRECATED;

		bUseAttributeMaterialOverrides = (MaterialOverrideMode_DEPRECATED == EPCGMeshSelectorMaterialOverrideMode::ByAttributeOverride);

		bOverrideCollisionProfile_DEPRECATED = false;
		CollisionProfile_DEPRECATED = UCollisionProfile::NoCollision_ProfileName;
		MaterialOverrideMode_DEPRECATED = EPCGMeshSelectorMaterialOverrideMode::NoOverride;
		MaterialOverrides_DEPRECATED.Reset();
		CullStartDistance_DEPRECATED = 0;
		CullEndDistance_DEPRECATED = 0;
		WorldPositionOffsetDisableDistance_DEPRECATED = 0;
	}
#endif

	// TODO: Remove if/when FBodyInstance is updated or replaced
	// Necessary to update the collision Response Container from the Response Array
	TemplateDescriptor.PostLoadFixup(this);
}

bool UPCGMeshSelectorByAttribute::SelectInstances(
	FPCGStaticMeshSpawnerContext& Context,
	const UPCGStaticMeshSpawnerSettings* Settings,
	const UPCGPointData* InPointData,
	TArray<FPCGMeshInstanceList>& OutMeshInstances,
	UPCGPointData* OutPointData) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGMeshSelectorByAttribute::SelectInstances);

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

	if (!InPointData->Metadata->HasAttribute(AttributeName))
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, FText::Format(LOCTEXT("AttributeNotInMetadata", "Attribute '{0}' is not in the metadata"), FText::FromName(AttributeName)));
		return true;
	}

	const FPCGMetadataAttributeBase* AttributeBase = InPointData->Metadata->GetConstAttribute(AttributeName);
	check(AttributeBase);

	// Set up a getter lambda to retrieve the mesh asset in the selection loop below. Returns true if path was valid.
	TFunction<void(PCGMetadataValueKey, FSoftObjectPath&)> MeshPathGetter;
	if (PCG::Private::IsOfTypes<FSoftObjectPath>(AttributeBase->GetTypeId()))
	{
		MeshPathGetter = [AttributeBase](PCGMetadataValueKey InValueKey, FSoftObjectPath& OutMeshPath)
		{
			const FPCGMetadataAttribute<FSoftObjectPath>* Attribute = static_cast<const FPCGMetadataAttribute<FSoftObjectPath>*>(AttributeBase);
			OutMeshPath = Attribute->GetValue(InValueKey);
		};
	}
	else if (PCG::Private::IsOfTypes<FString>(AttributeBase->GetTypeId()))
	{
		MeshPathGetter = [AttributeBase](PCGMetadataValueKey InValueKey, FSoftObjectPath& OutMeshPath)
		{
			const FPCGMetadataAttribute<FString>* Attribute = static_cast<const FPCGMetadataAttribute<FString>*>(AttributeBase);

			const FString Path = Attribute->GetValue(InValueKey);
			if (!Path.IsEmpty() && Path != TEXT("None"))
			{
				OutMeshPath = FSoftObjectPath(Path);
			}
		};
	}
	else
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, FText::Format(LOCTEXT("AttributeInvalidType", "Attribute '{0}' is not of valid type (must be FString or FSoftObjectPath)"), FText::FromName(AttributeName)));
		return true;
	}

	FPCGMeshMaterialOverrideHelper& MaterialOverrideHelper = Context.MaterialOverrideHelper;
	if (!MaterialOverrideHelper.IsInitialized())
	{
		MaterialOverrideHelper.Initialize(Context, bUseAttributeMaterialOverrides, TemplateDescriptor.OverrideMaterials, MaterialOverrideAttributes, InPointData->Metadata);
	}

	if (!MaterialOverrideHelper.IsValid())
	{
		return true;
	}

	// ByAttribute takes in SoftObjectPaths per point in the metadata, so we can pass those directly into the outgoing pin if it exists
	if (Context.CurrentPointIndex == 0 && OutPointData)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGMeshSelectorByAttribute::SetupOutPointData);
		OutPointData->SetPoints(InPointData->GetPoints());
		OutPointData->Metadata->DeleteAttribute(Settings->OutAttributeName);
		OutPointData->Metadata->CopyAttribute(InPointData->Metadata, AttributeName, Settings->OutAttributeName);
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGMeshSelectorByAttribute::SelectEntries);

	// Assign points to entries
	int32 CurrentPointIndex = Context.CurrentPointIndex;
	int32 LastCheckpointIndex = CurrentPointIndex;
	constexpr int32 TimeSlicingCheckFrequency = 1024;
	TMap<PCGMetadataValueKey, TSoftObjectPtr<UStaticMesh>>& ValueKeyToMesh = Context.ValueKeyToMesh;

	// No partition needed if there are no overrides
	if (Settings->StaticMeshComponentPropertyOverrides.IsEmpty())
	{
		Context.bPartitionDone = true;
	}

	if (!Context.bPartitionDone)
	{
		// Validate all the selectors are actual FSoftISMComponentDescriptor properties
		TArray<FPCGAttributePropertySelector> ValidSelectorOverrides;
		for (const FPCGObjectPropertyOverrideDescription& PropertyOverride : Settings->StaticMeshComponentPropertyOverrides)
		{
			if (FSoftISMComponentDescriptor::StaticStruct()->FindPropertyByName(FName(PropertyOverride.PropertyTarget)))
			{
				ValidSelectorOverrides.Emplace_GetRef() = FPCGAttributePropertySelector::CreateFromOtherSelector<FPCGAttributePropertySelector>(PropertyOverride.InputSource);
			}
			else
			{
				PCGE_LOG_C(Error, GraphAndLog, &Context, FText::Format(LOCTEXT("OverriddenPropertyNotFound", "Property '{0}' not a valid property with an ISM Descriptor. It will be ignored."), FText::FromString(PropertyOverride.PropertyTarget)));
			}
		}

		// If there are valid overrides, partition the points on those attributes so that an instance can be created for each
		if (!ValidSelectorOverrides.IsEmpty())
		{
			Context.AttributeOverridePartition = PCGMetadataPartitionCommon::AttributeGenericPartition(InPointData, ValidSelectorOverrides, &Context, Settings->bSilenceOverrideAttributeNotFoundErrors);
		}

		// Set the descriptors to match the partition count. Uninitialized, because it will be copied from the template below
		Context.OverriddenDescriptors.Reserve(Context.AttributeOverridePartition.Num());
		for (int I = 0; I < Context.AttributeOverridePartition.Num(); ++I)
		{
			FSoftISMComponentDescriptor& Descriptor = Context.OverriddenDescriptors.Add_GetRef(TemplateDescriptor);

			// Use the Object Override to map the user's input selector and property to the descriptor
			FPCGObjectOverrides Overrides(&Descriptor);
			Overrides.Initialize(Settings->StaticMeshComponentPropertyOverrides, &Descriptor, InPointData, &Context);

			// Since they are already partitioned and identical, we can just use the value on the first point
			check(!Context.AttributeOverridePartition[I].IsEmpty());
			int32 AnyPointIndexOnThisPartition = Context.AttributeOverridePartition[I][0];
			Overrides.Apply(AnyPointIndexOnThisPartition);
		}

		// Given partitioning is expensive, check if we're out of time for this frame
		Context.bPartitionDone = true;
		if (Context.ShouldStop())
		{
			return false;
		}
	}

	const TArray<FPCGPoint>& Points = InPointData->GetPoints();

	while (CurrentPointIndex < Points.Num())
	{
		int32 ThisPointIndex = CurrentPointIndex++;
		const FPCGPoint& Point = Points[ThisPointIndex];

		const PCGMetadataValueKey ValueKey = AttributeBase->GetValueKey(Point.MetadataEntry);
		TSoftObjectPtr<UStaticMesh>* NewMesh = ValueKeyToMesh.Find(ValueKey);
		TSoftObjectPtr<UStaticMesh> Mesh = nullptr;

		// If this ValueKey has not been seen before, let's cache it for the future
		if (!NewMesh)
		{
			FSoftObjectPath MeshPath;
			MeshPathGetter(ValueKey, MeshPath);

			if (!MeshPath.IsNull())
			{
				Mesh = TSoftObjectPtr<UStaticMesh>(MeshPath);

				if (Mesh.IsNull())
				{
					PCGE_LOG_C(Error, GraphAndLog, &Context, FText::Format(LOCTEXT("InvalidMeshPath", "Invalid mesh path: '{0}'."), FText::FromString(MeshPath.ToString())));
				}
			}
			else
			{
				PCGE_LOG_C(Warning, LogOnly, &Context, LOCTEXT("TrivialInvalidMeshPath", "Trivially invalid mesh path used."));
			}

			ValueKeyToMesh.Add(ValueKey, Mesh);
		}
		else
		{
			Mesh = *NewMesh;
		}

		if (Mesh.IsNull())
		{
			continue;
		}

		// TODO: Revisit this when attribute partitioning is returned in a more optimized form
		// The partition index is used to assign the point to the correct partition's instance
		int32 CurrentPointPartitionIndex = INDEX_NONE;
		for (int PartitionIndex = 0; PartitionIndex < Context.AttributeOverridePartition.Num(); ++PartitionIndex)
		{
			TArray<int32>& Partition = Context.AttributeOverridePartition[PartitionIndex];
			if (Partition.Contains(ThisPointIndex))
			{
				CurrentPointPartitionIndex = PartitionIndex;
				break;
			}
		}

		// If the point wasn't found in a partition (likely due to no partitions or an invalid property target), just default to the template
		const FSoftISMComponentDescriptor& CurrentPartitionDescriptor = CurrentPointPartitionIndex != INDEX_NONE ? Context.OverriddenDescriptors[CurrentPointPartitionIndex] : TemplateDescriptor;
		const bool bReverseTransform = (Point.Transform.GetDeterminant() < 0);

		FPCGMeshInstanceList& InstanceList = PCGMeshSelectorAttribute::GetInstanceList(OutMeshInstances, CurrentPartitionDescriptor, Mesh, MaterialOverrideHelper.GetMaterialOverrides(Point.MetadataEntry), bReverseTransform, CurrentPointPartitionIndex);
		InstanceList.Instances.Emplace(Point.Transform);
		InstanceList.InstancesMetadataEntry.Emplace(Point.MetadataEntry);

		if (OutPointData && Settings->bApplyMeshBoundsToPoints)
		{
			TArray<int32>& PointIndices = Context.MeshToOutPoints.FindOrAdd(Mesh).FindOrAdd(OutPointData);
			PointIndices.Emplace(ThisPointIndex);
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

	return (CurrentPointIndex == Points.Num());
}

#undef LOCTEXT_NAMESPACE
