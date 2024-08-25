// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGLevelToAsset.h"

#include "PCGEditorModule.h"
#include "PCGEditorUtils.h"
#include "PCGAssetExporterUtils.h"

#include "Data/PCGPointData.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"

#include "ContentBrowserModule.h"
#include "FileHelpers.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

void UPCGLevelToAsset::CreateOrUpdatePCGAssets(const TArray<FAssetData>& WorldAssets, const FPCGAssetExporterParameters& InParameters, TSubclassOf<UPCGLevelToAsset> ExporterSubclass)
{
	TArray<UPackage*> PackagesToSave;
	FPCGAssetExporterParameters Parameters = InParameters;

	if (WorldAssets.Num() > 1)
	{
		Parameters.bOpenSaveDialog = false;
	}

	for (const FAssetData& WorldAsset : WorldAssets)
	{
		if (UPackage* Package = CreateOrUpdatePCGAsset(TSoftObjectPtr<UWorld>(WorldAsset.GetSoftObjectPath()), Parameters, ExporterSubclass))
		{
			PackagesToSave.Add(Package);
		}
	}

	// Save the file(s)
	if (!PackagesToSave.IsEmpty() && Parameters.bSaveOnExportEnded)
	{
		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, false);
	}
}

UPackage* UPCGLevelToAsset::CreateOrUpdatePCGAsset(TSoftObjectPtr<UWorld> WorldPath, const FPCGAssetExporterParameters& Parameters, TSubclassOf<UPCGLevelToAsset> ExporterSubclass)
{
	return CreateOrUpdatePCGAsset(WorldPath.LoadSynchronous(), Parameters, ExporterSubclass);
}

UPackage* UPCGLevelToAsset::CreateOrUpdatePCGAsset(UWorld* World, const FPCGAssetExporterParameters& InParameters, TSubclassOf<UPCGLevelToAsset> ExporterSubclass)
{
	if (!World)
	{
		return nullptr;
	}

	UPCGLevelToAsset* Exporter = nullptr;
	if (ExporterSubclass)
	{
		Exporter = NewObject<UPCGLevelToAsset>(GetTransientPackage(), ExporterSubclass);
	}
	else
	{
		Exporter = NewObject<UPCGLevelToAsset>(GetTransientPackage());
	}

	if (!Exporter)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("Unable to create Level to Settings exporter."));
		return nullptr;
	}

	Exporter->WorldToExport = World;

	FPCGAssetExporterParameters Parameters = InParameters;
	Parameters.AssetName = World->GetName() + TEXT("_PCG");

	if (InParameters.AssetPath.IsEmpty() && World->GetPackage())
	{
		Parameters.AssetPath = FPackageName::GetLongPackagePath(World->GetPackage()->GetName());
	}

	return UPCGAssetExporterUtils::CreateAsset(Exporter, Parameters);
}

UPackage* UPCGLevelToAsset::UpdateAsset(const FAssetData& PCGAsset)
{
	UPCGDataAsset* Asset = Cast<UPCGDataAsset>(PCGAsset.GetAsset());
	if (!Asset)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("Asset '%s' isn't a PCG data asset or could not be properly loaded."), *PCGAsset.GetObjectPathString());
		return nullptr;
	}

	UPackage* Package = Asset->GetPackage();
	if (!Package)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("Unable to retrieve package from Asset '%s'."), *PCGAsset.GetObjectPathString());
		return nullptr;
	}

	TSoftObjectPtr<UWorld> WorldPtr(Asset->ObjectPath);
	UWorld* World = WorldPtr.LoadSynchronous();

	if (!World)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("PCG asset was unable to load world '%s'."), *Asset->ObjectPath.ToString());
		return nullptr;
	}

	WorldToExport = World;

	if (ExportAsset(Package->GetPathName(), Asset))
	{
		FCoreUObjectDelegates::BroadcastOnObjectModified(Asset);
		return Package;
	}
	else
	{
		return nullptr;
	}
}

bool UPCGLevelToAsset::ExportAsset(const FString& PackageName, UPCGDataAsset* Asset)
{
	return BP_ExportWorld(WorldToExport, PackageName, Asset);
}

bool UPCGLevelToAsset::BP_ExportWorld_Implementation(UWorld* World, const FString& PackageName, UPCGDataAsset* Asset)
{
	check(World && Asset);
	Asset->ObjectPath = FSoftObjectPath(World);
	Asset->Description = FText::Format(NSLOCTEXT("PCGLevelToAsset", "DefaultDescriptionOnExportedLevel", "Generated from world: {0}"), FText::FromString(World->GetName()));
	Asset->ExporterClass = GetClass();

	FPCGDataCollection& DataCollection = Asset->Data;
	DataCollection.TaggedData.Reset();

	// Create Root Data
	UPCGPointData* RootPointData = NewObject<UPCGPointData>(Asset);
	UPCGMetadata* RootMetadata = RootPointData->MutableMetadata();
	TArray<FPCGPoint>& Roots = RootPointData->GetMutablePoints();
	RootMetadata->CreateAttribute<FString>(TEXT("Name"), World->GetName(), /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);
	RootMetadata->CreateAttribute<FSoftObjectPath>(TEXT("Source"), FSoftObjectPath(World), /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);

	// Add to data collection
	{
		FPCGTaggedData& RootsTaggedData = DataCollection.TaggedData.Emplace_GetRef();
		RootsTaggedData.Data = RootPointData;
		RootsTaggedData.Pin = TEXT("Root");
	}

	// Create PCGData
	UPCGPointData* PointData = NewObject<UPCGPointData>(Asset);
	UPCGMetadata* PointMetadata = PointData->MutableMetadata();
	TArray<FPCGPoint>& Points = PointData->GetMutablePoints();

	// Add to data collection
	{
		FPCGTaggedData& PointsTaggedData = DataCollection.TaggedData.Emplace_GetRef();
		PointsTaggedData.Data = PointData;
		PointsTaggedData.Pin = TEXT("Points");
	}

	// Common data shared across steps
	FBox AllActorBounds(EForceInit::ForceInit);

	// Hardcoded attributes
	const FName MaterialAttributeName = TEXT("Material");
	const FName MeshAttributeName = TEXT("Mesh");
	const FName HierarchyDepthAttributeName = TEXT("HierarchyDepth");
	const FName ActorIndexAttributeName = TEXT("ActorIndex");
	const FName ParentIndexAttributeName = TEXT("ParentIndex");
	const FName RelativeTransformAttributeName = TEXT("RelativeTransform");

	// Attribute setup on the points
	FPCGMetadataAttribute<FSoftObjectPath>* MaterialAttribute = PointMetadata->CreateAttribute<FSoftObjectPath>(MaterialAttributeName, FSoftObjectPath(), /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);
	FPCGMetadataAttribute<FSoftObjectPath>* MeshAttribute = PointMetadata->CreateAttribute<FSoftObjectPath>(MeshAttributeName, FSoftObjectPath(), /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);
	FPCGMetadataAttribute<int64>* HierarchyDepthAttribute = PointMetadata->CreateAttribute<int64>(HierarchyDepthAttributeName, 0, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);
	FPCGMetadataAttribute<int64>* ActorIndexAttribute = PointMetadata->CreateAttribute<int64>(ActorIndexAttributeName, -1, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);
	FPCGMetadataAttribute<int64>* ParentIndexAttribute = PointMetadata->CreateAttribute<int64>(ParentIndexAttributeName, -1, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);
	FPCGMetadataAttribute<FTransform>* RelativeTransformAttribute = PointMetadata->CreateAttribute<FTransform>(RelativeTransformAttributeName, FTransform::Identity, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);

	// Map from raw/unsanitized tag name to corresponding attribute.
	TMap<FName, FPCGMetadataAttributeBase*> TagToAttributeMap;

	// Relationship Tag:SanitizedName is many:1, so keep track of which sanitized names are created so we don't attempt to create the same one multiple times.
	TSet<FName> SanitizedAttributeNames;

	TSet<FName> ReservedTags;
	ReservedTags.Add(MaterialAttributeName);
	ReservedTags.Add(MeshAttributeName);
	ReservedTags.Add(HierarchyDepthAttributeName);
	ReservedTags.Add(ActorIndexAttributeName);
	ReservedTags.Add(ParentIndexAttributeName);
	ReservedTags.Add(RelativeTransformAttributeName);

	// Hierarchy root point
	{
		FPCGPoint& RootPoint = Points.Emplace_GetRef(FTransform::Identity, /*Density=*/1.0f, /*Seed=*/0);
		RootPoint.Steepness = 1.0f;
		RootPoint.BoundsMin = RootPoint.BoundsMax = FVector::Zero();

		PointMetadata->InitializeOnSet(RootPoint.MetadataEntry);
		ActorIndexAttribute->SetValue(RootPoint.MetadataEntry, 0);
	}

	// Build actor-index map
	TMap<AActor*, int> ActorIndexMap;
	int LastActorIndex = 1; // Since the root is the "first" point we'll have, we'll have the map start from 1.
	UPCGActorHelpers::ForEachActorInWorld(World, AActor::StaticClass(), [&ActorIndexMap, &LastActorIndex](AActor* Actor)
	{
		ActorIndexMap.Add(Actor, LastActorIndex++);
		return true;
	});

	// Create points
	UPCGActorHelpers::ForEachActorInWorld(World, AActor::StaticClass(), [&](AActor* Actor)
	{
		// TODO Actor-level decisions if any; if the actor is "consumed" at this step, make sure to update AllActorBounds as well.

		// Otherwise, parse "known" actor components
		TArray<UStaticMeshComponent*> SMCs;
		Actor->GetComponents(SMCs);

		if (SMCs.IsEmpty()) // early out
		{
			return true;
		}

		const FBox ActorBounds = PCGHelpers::GetActorBounds(Actor, /*bIgnorePCGCreatedComponents=*/true);
		AllActorBounds += ActorBounds;

		for (FName ActorTag : Actor->Tags)
		{
			if (ReservedTags.Contains(ActorTag) || TagToAttributeMap.Contains(ActorTag))
			{
				continue;
			}

			FString TagString = ActorTag.ToString();
			int32 DividerPosition = INDEX_NONE;
			
			if (TagString.FindChar(':', DividerPosition))
			{
				const FString LeftSide = TagString.Left(DividerPosition);
				const FString RightSide = TagString.RightChop(DividerPosition + 1);

				if (LeftSide.IsEmpty() || RightSide.IsEmpty())
				{
					continue;
				}

				const FName TagName(LeftSide);
				FString SanitizedAttributeNameString = LeftSide;
				const bool bSanitized = FPCGMetadataAttributeBase::SanitizeName(SanitizedAttributeNameString);
				const FName SanitizedAttributeName(SanitizedAttributeNameString);

				// Once sanitized, multiple tags can map to a single attribute name. The first tag will be used, remaining will be ignored.
				if (ReservedTags.Contains(SanitizedAttributeName) || TagToAttributeMap.Contains(TagName) || SanitizedAttributeNames.Contains(SanitizedAttributeName))
				{
					continue;
				}

				if (bSanitized)
				{
					UE_LOG(LogPCGEditor, Warning, TEXT("Sanitized tag string on actor '%s' to remove invalid characters: '%s' -> '%s'"), *Actor->GetName(), *LeftSide, *SanitizedAttributeNameString);
				}

				// Otherwise, create the attribute based on the type of the data after the colon.
				if (RightSide.IsNumeric())
				{
					TagToAttributeMap.Add(TagName, PointMetadata->CreateAttribute<double>(SanitizedAttributeName, 0.0f, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true));
				}
				else
				{
					TagToAttributeMap.Add(TagName, PointMetadata->CreateAttribute<FString>(SanitizedAttributeName, FString(), /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true));
				}

				SanitizedAttributeNames.Add(SanitizedAttributeName);
			}
			else
			{
				// Simple boolean attribute
				FString& SanitizedTagString = TagString;
				FPCGMetadataAttributeBase::SanitizeName(SanitizedTagString);
				const FName SanitizedTagName(SanitizedTagString);

				// Once sanitized, multiple tags can map to a single attribute name. The first tag will be used, remaining will be ignored.
				if (!ReservedTags.Contains(SanitizedTagName) && !SanitizedAttributeNames.Contains(SanitizedTagName))
				{
					TagToAttributeMap.Add(ActorTag, PointMetadata->CreateAttribute<bool>(SanitizedTagName, false, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true));

					SanitizedAttributeNames.Add(SanitizedTagName);
				}
			}
		}

		// Prepare actor-level data that's propagated to all points
		const FTransform& ActorTransform = Actor->GetTransform();
		const int64 ActorIndex = ActorIndexMap[Actor];
		AActor* ParentActor = Actor->GetAttachParentActor();
		const int64 ParentActorIndex = ParentActor ? ActorIndexMap[ParentActor] : 0;
		const FTransform RelativeTransform = ParentActor ? ActorTransform.GetRelativeTransform(ParentActor->GetTransform()) : ActorTransform;

		// Hierarchy depth, starts at 1 if the actor doesn't have a parent
		int HierarchyDepth = 1;
		while (ParentActor)
		{
			++HierarchyDepth;
			ParentActor = ParentActor->GetAttachParentActor();
		}

		auto MakePoint = [&](const FTransform& Transform, const FSoftObjectPath& MeshPath, const FBox& MeshBounds, const TArray<UMaterialInterface*>& MeshMaterials)
		{
			FPCGPoint& Point = Points.Emplace_GetRef(Transform, /*Density=*/1.0f, PCGHelpers::ComputeSeedFromPosition(Transform.GetLocation()));
			Point.BoundsMin = MeshBounds.Min;
			Point.BoundsMax = MeshBounds.Max;
			Point.Steepness = 1.0f;

			PointMetadata->InitializeOnSet(Point.MetadataEntry);
			MeshAttribute->SetValue(Point.MetadataEntry, MeshPath);

			if (!MeshMaterials.IsEmpty())
			{
				MaterialAttribute->SetValue(Point.MetadataEntry, FSoftObjectPath(MeshMaterials[0]));
			}

			ActorIndexAttribute->SetValue(Point.MetadataEntry, ActorIndex);
			ParentIndexAttribute->SetValue(Point.MetadataEntry, ParentActorIndex);
			RelativeTransformAttribute->SetValue(Point.MetadataEntry, RelativeTransform);
			HierarchyDepthAttribute->SetValue(Point.MetadataEntry, HierarchyDepth);

			// For all tags, if the tag is of format 'Name:Value' then create attribute Name and assign Value, otherwise
			// create a boolean attribute with the name given by the sanitized tag string.
			for (FName ActorTag : Actor->Tags)
			{
				const FString TagString = ActorTag.ToString();
				int32 DividerPosition = INDEX_NONE;

				if (TagString.FindChar(':', DividerPosition))
				{
					const FString LeftSide = TagString.Left(DividerPosition);
					const FString RightSide = TagString.RightChop(DividerPosition + 1);

					if (LeftSide.IsEmpty() || RightSide.IsEmpty())
					{
						continue;
					}

					const FName TagName(LeftSide);
					if (FPCGMetadataAttributeBase** Attribute = TagToAttributeMap.Find(TagName))
					{
						check(*Attribute);
						if (RightSide.IsNumeric() && (*Attribute)->GetTypeId() == PCG::Private::MetadataTypes<double>::Id)
						{
							double RightSideValue = FCString::Atod(*RightSide);
							static_cast<FPCGMetadataAttribute<double>*>(*Attribute)->SetValue(Point.MetadataEntry, RightSideValue);
						}
						else if ((*Attribute)->GetTypeId() == PCG::Private::MetadataTypes<FString>::Id)
						{
							static_cast<FPCGMetadataAttribute<FString>*>(*Attribute)->SetValue(Point.MetadataEntry, RightSide);
						}
					}
				}
				else if (FPCGMetadataAttributeBase** Attribute = TagToAttributeMap.Find(ActorTag))
				{
					if ((*Attribute)->GetTypeId() == PCG::Private::MetadataTypes<bool>::Id)
					{
						static_cast<FPCGMetadataAttribute<bool>*>(*Attribute)->SetValue(Point.MetadataEntry, true);
					}
				}
			}
		};

		for (UStaticMeshComponent* SMC : SMCs)
		{
			TObjectPtr<UStaticMesh> StaticMesh = SMC->GetStaticMesh();

			if (!StaticMesh)
			{
				continue;
			}

			const FSoftObjectPath MeshPath(StaticMesh);
			const FBox MeshBounds = StaticMesh->GetBoundingBox();
			TArray<UMaterialInterface*> Materials = SMC->GetMaterials();

			// For all instances (or a single instance if this is a static mesh and not an ISM)
			// if a static mesh -> use actor transform (which might be wrong?)
			// if ISM -> get instance transform in world space
			if (UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>(SMC))
			{
				const int InstanceCount = ISMC->GetNumInstances();
				for (int I = 0; I < InstanceCount; ++I)
				{
					FTransform InstanceTransform;
					ISMC->GetInstanceTransform(I, InstanceTransform, /*bWorldSpace=*/true);

					MakePoint(InstanceTransform, MeshPath, MeshBounds, Materials);
				}
			}
			else
			{
				MakePoint(ActorTransform, MeshPath, MeshBounds, Materials);
			}
		}

		return true;
	});

	// Finally, create root point in the root data
	{
		FPCGPoint& RootPoint = Roots.Emplace_GetRef(FTransform::Identity, 1.0f, 0);
		RootPoint.BoundsMin = AllActorBounds.Min;
		RootPoint.BoundsMax = AllActorBounds.Max;
	}

	return true;
}

void UPCGLevelToAsset::SetWorld(UWorld* World)
{
	WorldToExport = World;
}

UWorld* UPCGLevelToAsset::GetWorld() const
{
	return WorldToExport;
}