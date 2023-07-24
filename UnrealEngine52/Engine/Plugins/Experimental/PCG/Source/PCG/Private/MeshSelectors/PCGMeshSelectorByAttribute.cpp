// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshSelectors/PCGMeshSelectorByAttribute.h"

#include "Data/PCGPointData.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Elements/PCGStaticMeshSpawnerContext.h"
#include "Data/PCGSpatialData.h"
#include "MeshSelectors/PCGMeshSelectorBase.h"

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
		bool bReverseCulling)
	{
		for (FPCGMeshInstanceList& InstanceList : InstanceLists)
		{
			if (InstanceList.Descriptor.StaticMesh == Mesh &&
				InstanceList.Descriptor.bReverseCulling == bReverseCulling &&
				InstanceList.Descriptor.OverrideMaterials == MaterialOverrides)
			{
				return InstanceList;
			}
		}

		FPCGMeshInstanceList& NewInstanceList = InstanceLists.Emplace_GetRef(TemplateDescriptor);
		NewInstanceList.Descriptor.StaticMesh = Mesh;
		NewInstanceList.Descriptor.OverrideMaterials = MaterialOverrides;
		NewInstanceList.Descriptor.bReverseCulling = bReverseCulling;

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

	if (AttributeBase->GetTypeId() != PCG::Private::MetadataTypes<FString>::Id)
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, FText::Format(LOCTEXT("AttributeInvalidType", "Attribute '{0}' is not of valid type FString"), FText::FromName(AttributeName)));
		return true;
	}

	const FPCGMetadataAttribute<FString>* Attribute = static_cast<const FPCGMetadataAttribute<FString>*>(AttributeBase);

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

	const TArray<FPCGPoint>& Points = InPointData->GetPoints();
	while(CurrentPointIndex < Points.Num())
	{
		const FPCGPoint& Point = Points[CurrentPointIndex++];

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
			FString MeshSoftObjectPath = Attribute->GetValue(ValueKey);

			if (!MeshSoftObjectPath.IsEmpty() && MeshSoftObjectPath != TEXT("None"))
			{
				FSoftObjectPath MeshPath(MeshSoftObjectPath);
				Mesh = TSoftObjectPtr<UStaticMesh>(MeshPath);

				if (Mesh.IsNull())
				{
					PCGE_LOG_C(Error, GraphAndLog, &Context, FText::Format(LOCTEXT("InvalidMeshPath", "Invalid mesh path: '{0}'."), FText::FromString(MeshSoftObjectPath)));
				}
			}
			else
			{
				PCGE_LOG_C(Warning, LogOnly, &Context, FText::Format(LOCTEXT("TrivialInvalidMeshPath", "Trivially invalid mesh path used: '{0}'"), FText::FromString(MeshSoftObjectPath)));
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

		const bool bReverseTransform = (Point.Transform.GetDeterminant() < 0);
		FPCGMeshInstanceList& InstanceList = PCGMeshSelectorAttribute::GetInstanceList(OutMeshInstances, TemplateDescriptor, Mesh, MaterialOverrideHelper.GetMaterialOverrides(Point.MetadataEntry), bReverseTransform);
		InstanceList.Instances.Emplace(Point.Transform);
		InstanceList.InstancesMetadataEntry.Emplace(Point.MetadataEntry);

		// Check if we should stop here and continue in a subsequent call
		if(CurrentPointIndex - LastCheckpointIndex >= TimeSlicingCheckFrequency)
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
