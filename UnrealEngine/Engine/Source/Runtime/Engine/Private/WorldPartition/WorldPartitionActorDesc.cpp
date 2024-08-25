// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/WorldPartitionActorDescArchive.h"

#include "Misc/Paths.h"
#include "Misc/ArchiveMD5.h"
#include "UObject/MetaData.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryHelpers.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "UObject/UE5ReleaseStreamObjectVersion.h"
#include "UObject/FortniteSeasonBranchObjectVersion.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "WorldPartition/WorldPartitionActorDescInstanceViewInterface.h"
#include "WorldPartition/WorldPartitionLog.h"
#include "WorldPartition/ActorDescContainer.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/ExternalDataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerUtils.h"
#include "WorldPartition/ContentBundle/ContentBundlePaths.h"
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationErrorHandler.h"
#include "ActorReferencesUtils.h"

#define LOCTEXT_NAMESPACE "FWorldPartitionActorDesc"

TMap<TSubclassOf<AActor>, FWorldPartitionActorDesc::FActorDescDeprecator> FWorldPartitionActorDesc::Deprecators;

static FGuid GetDefaultActorDescGuid(const FWorldPartitionActorDesc* ActorDesc)
{
	FArchiveMD5 ArMD5;
	FString ClassPath = ActorDesc->GetBaseClass().IsValid() ? ActorDesc->GetBaseClass().ToString() : ActorDesc->GetNativeClass().ToString();	
	ArMD5 << ClassPath;
	return ArMD5.GetGuidFromHash();
}

FWorldPartitionActorDesc::FWorldPartitionActorDesc()
	: bIsSpatiallyLoaded(false)
	, bActorIsEditorOnly(false)
	, bActorIsRuntimeOnly(false)
	, bActorIsMainWorldOnly(false)
	, bActorIsHLODRelevant(false)
	, bActorIsListedInSceneOutliner(true)
	, bIsUsingDataLayerAsset(false)
	, bIsBoundsValid(false)
	, ActorNativeClass(nullptr)
	, Container(nullptr)
	, bIsDefaultActorDesc(false)
{}

void FWorldPartitionActorDesc::Init(const AActor* InActor)
{	
	check(IsValid(InActor));

	UClass* ActorClass = InActor->GetClass();

	// Get the first native class in the hierarchy
	ActorNativeClass = GetParentNativeClass(ActorClass);
	NativeClass = *ActorNativeClass->GetPathName();
	
	// For native class, don't set this
	if (!ActorClass->IsNative())
	{
		BaseClass = *InActor->GetClass()->GetPathName();
	}

	if (InActor->HasAnyFlags(RF_ArchetypeObject | RF_ClassDefaultObject))
	{
		check(!InActor->IsPackageExternal());
		check(!InActor->GetActorGuid().IsValid());
		Guid = GetDefaultActorDescGuid(this);
		bIsDefaultActorDesc = true;
	}
	else
	{
		check(InActor->IsPackageExternal());
		check(InActor->GetActorGuid().IsValid());
		Guid = InActor->GetActorGuid();
	}

	check(Guid.IsValid());

	ActorTransform = InActor->GetActorTransform();

	const FBox StreamingBounds = !bIsDefaultActorDesc ? InActor->GetStreamingBounds() : FBox(ForceInit);
	StreamingBounds.GetCenterAndExtents(BoundsLocation, BoundsExtent);
	bIsBoundsValid = StreamingBounds.IsValid == 1;

	RuntimeGrid = InActor->GetRuntimeGrid();
	bIsSpatiallyLoaded = InActor->GetIsSpatiallyLoaded();
	bActorIsEditorOnly = InActor->IsEditorOnly();
	bActorIsRuntimeOnly = InActor->IsRuntimeOnly();
	bActorIsHLODRelevant = InActor->IsHLODRelevant();
	bActorIsListedInSceneOutliner = InActor->IsListedInSceneOutliner();
	bActorIsMainWorldOnly = InActor->bIsMainWorldOnly;
	HLODLayer = InActor->GetHLODLayer() ? FSoftObjectPath(InActor->GetHLODLayer()->GetPathName()) : FSoftObjectPath();
	
	// DataLayers
	{
		TArray<FName> LocalDataLayerAssetPaths;
		TArray<FName> LocalDataLayerInstanceNames;

		if (UWorldPartition* ActorWorldPartition = FWorldPartitionHelpers::GetWorldPartition(InActor))
		{
			const bool bIncludeExternalDataLayerAsset = false;
			TArray<const UDataLayerAsset*> DataLayerAssets = InActor->GetDataLayerAssets(bIncludeExternalDataLayerAsset);
			LocalDataLayerAssetPaths.Reserve(DataLayerAssets.Num());
			for (const UDataLayerAsset* DataLayerAsset : DataLayerAssets)
			{
				if (DataLayerAsset)
				{
					LocalDataLayerAssetPaths.Add(*DataLayerAsset->GetPathName());
				}
			}
			
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			// If the deprected ActorDataLayers is empty, consider the ActorDesc to be is using DataLayerAssets (with an empty array)
			bIsUsingDataLayerAsset = (LocalDataLayerAssetPaths.Num() > 0) || (InActor->GetActorDataLayers().IsEmpty());
			if (!bIsUsingDataLayerAsset)
			{
				// Use Actor's DataLayerManager since the fixup is relative to this level
				const UDataLayerManager* DataLayerManager = ActorWorldPartition->GetDataLayerManager();
				if (ensure(DataLayerManager))
				{
					// Pass Actor Level when resolving the DataLayerInstance as FWorldPartitionActorDesc always represents the state of the actor local to its outer level
					LocalDataLayerInstanceNames = DataLayerManager->GetDataLayerInstanceNames(InActor->GetActorDataLayers());
				}
			}
			PRAGMA_ENABLE_DEPRECATION_WARNINGS

			// Init DataLayers persistent info
			DataLayers = bIsUsingDataLayerAsset ? MoveTemp(LocalDataLayerAssetPaths) : MoveTemp(LocalDataLayerInstanceNames);
		}
		else
		{
			// Possible there is no World Partition for regular OFPA levels that haven't been converted to support Data Layers
			bIsUsingDataLayerAsset = true;
			DataLayers.Empty();
		}

		// Initialize ExternalDataLayerAsset
		ExternalDataLayerAsset = InActor->GetExternalDataLayerAsset() ? FSoftObjectPath(InActor->GetExternalDataLayerAsset()->GetPathName()) : FSoftObjectPath();
	}

	Tags = InActor->Tags;

	check(Properties.IsEmpty());
	InActor->GetActorDescProperties(Properties);

	ActorPackage = InActor->GetPackage()->GetFName();
	ActorPath = bIsDefaultActorDesc ? *InActor->GetClass()->GetPathName() : *InActor->GetPathName();
	ActorName = InActor->GetFName();

	ContentBundleGuid = InActor->GetContentBundleGuid();

	if (!bIsDefaultActorDesc)
	{
		FolderPath = InActor->GetFolderPath();
		FolderGuid = InActor->GetFolderGuid();

		const AActor* AttachParentActor = InActor->GetAttachParentActor();
		if (AttachParentActor)
		{
			while (AttachParentActor->GetParentActor())
			{
				AttachParentActor = AttachParentActor->GetParentActor();
			}
			ParentActor = AttachParentActor->GetActorGuid();
		}

		const ActorsReferencesUtils::FGetActorReferencesParams Params = ActorsReferencesUtils::FGetActorReferencesParams(const_cast<AActor*>(InActor))
			.SetRequiredFlags(RF_HasExternalPackage);
		TArray<ActorsReferencesUtils::FActorReference> ActorReferences = ActorsReferencesUtils::GetActorReferences(Params);

		if (ActorReferences.Num())
		{
			References.Reserve(ActorReferences.Num());
			for (const ActorsReferencesUtils::FActorReference& ActorReference : ActorReferences)
			{
				const FGuid ActorReferenceGuid = ActorReference.Actor->GetActorGuid();

				References.Add(ActorReferenceGuid);

				if (ActorReference.bIsEditorOnly)
				{
					EditorOnlyReferences.Add(ActorReferenceGuid);
				}
			}
		}

		ActorLabel = *InActor->GetActorLabel(false);
	}

	Container = nullptr;
}

void FWorldPartitionActorDesc::Init(const FWorldPartitionActorDescInitData& DescData)
{
	ActorPackage = DescData.PackageName;
	ActorPath = DescData.ActorPath;
	ActorNativeClass = DescData.NativeClass;
	NativeClass = *DescData.NativeClass->GetPathName();
	ActorName = *FPaths::GetExtension(ActorPath.ToString());

	// Serialize actor metadata
	FMemoryReader MetadataAr(DescData.SerializedData, true);

	// Serialize metadata custom versions
	FCustomVersionContainer CustomVersions;
	CustomVersions.Serialize(MetadataAr);
	MetadataAr.SetCustomVersions(CustomVersions);
	
	// Serialize metadata payload
	FActorDescArchive ActorDescAr(MetadataAr, this);
	ActorDescAr.Init();

	Serialize(ActorDescAr);

	// Call registered deprecator
	TSubclassOf<AActor> DeprecatedClass = ActorNativeClass;
	while (DeprecatedClass)
	{
		if (FActorDescDeprecator* Deprecator = Deprecators.Find(DeprecatedClass))
		{
			(*Deprecator)(MetadataAr, this);
			break;
		}
		DeprecatedClass = DeprecatedClass->GetSuperClass();
	}

	Container = nullptr;
}

void FWorldPartitionActorDesc::Patch(const FWorldPartitionActorDescInitData& DescData, TArray<uint8>& OutData, FWorldPartitionAssetDataPatcher* InAssetDataPatcher)
{
	// Serialize actor metadata
	FMemoryReader MetadataAr(DescData.SerializedData, true);

	// Serialize metadata custom versions
	FCustomVersionContainer CustomVersions;
	CustomVersions.Serialize(MetadataAr);
	MetadataAr.SetCustomVersions(CustomVersions);
	
	// Patch metadata payload
	TArray<uint8> PatchedPayloadData;
	FMemoryWriter PatchedPayloadAr(PatchedPayloadData, true);

	TUniquePtr<FWorldPartitionActorDesc> ActorDesc(AActor::StaticCreateClassActorDesc(DescData.NativeClass ? DescData.NativeClass : AActor::StaticClass()));
	FActorDescArchivePatcher ActorDescAr(MetadataAr, ActorDesc.Get(), PatchedPayloadAr, InAssetDataPatcher);	
	FTopLevelAssetPath ActorClassPath(TEXT("/Script/Engine.Actor"));
	ActorDescAr.Init(ActorClassPath);

	ActorDesc->Serialize(ActorDescAr);

	// Serialize custom versions
	TArray<uint8> HeaderData;
	FMemoryWriter HeaderAr(HeaderData);
	CustomVersions.Serialize(HeaderAr); 

	// Append data
	OutData = MoveTemp(HeaderData);
	OutData.Append(PatchedPayloadData);
}

bool FWorldPartitionActorDesc::Equals(const FWorldPartitionActorDesc* Other) const
{
	return
		Guid == Other->Guid &&
		BaseClass == Other->BaseClass &&
		NativeClass == Other->NativeClass &&
		ActorPackage == Other->ActorPackage &&
		ActorPath == Other->ActorPath &&
		ActorLabel == Other->ActorLabel &&
		bIsBoundsValid == Other->bIsBoundsValid &&
		ActorTransform.Equals(Other->ActorTransform, 0.1f) &&
		BoundsLocation.Equals(Other->BoundsLocation, 0.1f) &&
		BoundsExtent.Equals(Other->BoundsExtent, 0.1f) &&
		RuntimeGrid == Other->RuntimeGrid &&
		bIsSpatiallyLoaded == Other->bIsSpatiallyLoaded &&
		bActorIsEditorOnly == Other->bActorIsEditorOnly &&
		bActorIsRuntimeOnly == Other->bActorIsRuntimeOnly &&
		bActorIsMainWorldOnly == Other->bActorIsMainWorldOnly &&
		bActorIsHLODRelevant == Other->bActorIsHLODRelevant &&
		bActorIsListedInSceneOutliner == Other->bActorIsListedInSceneOutliner &&
		bIsUsingDataLayerAsset == Other->bIsUsingDataLayerAsset &&
		HLODLayer == Other->HLODLayer &&
		FolderPath == Other->FolderPath &&
		FolderGuid == Other->FolderGuid &&
		ParentActor == Other->ParentActor &&
		ContentBundleGuid == Other->ContentBundleGuid &&
		CompareUnsortedArrays(DataLayers, Other->DataLayers) &&
		CompareUnsortedArrays(References, Other->References) &&
		CompareUnsortedArrays(EditorOnlyReferences, Other->EditorOnlyReferences) &&
		CompareUnsortedArrays(Tags, Other->Tags) &&
		Properties == Other->Properties &&
		ExternalDataLayerAsset == Other->ExternalDataLayerAsset;
}

bool FWorldPartitionActorDesc::ShouldResave(const FWorldPartitionActorDesc* Other) const
{
	check(Guid == Other->Guid);
	check(ActorPackage == Other->ActorPackage);
	check(ActorPath == Other->ActorPath);

	if (RuntimeGrid != Other->RuntimeGrid ||
		bIsSpatiallyLoaded != Other->bIsSpatiallyLoaded ||
		bActorIsEditorOnly != Other->bActorIsEditorOnly ||
		bActorIsRuntimeOnly != Other->bActorIsRuntimeOnly ||
		bActorIsMainWorldOnly != Other->bActorIsMainWorldOnly ||
		bIsBoundsValid != Other->bIsBoundsValid ||
		HLODLayer != Other->HLODLayer ||
		ParentActor != Other->ParentActor ||
		ContentBundleGuid != Other->ContentBundleGuid ||
		!CompareUnsortedArrays(DataLayers, Other->DataLayers) ||
		!CompareUnsortedArrays(References, Other->References) ||
		!CompareUnsortedArrays(EditorOnlyReferences, Other->EditorOnlyReferences) ||
		Properties != Other->Properties ||
		ExternalDataLayerAsset != Other->ExternalDataLayerAsset)
	{
		return true;
	}

	// Tolerate up to 5% for bounds change
	if (bIsBoundsValid)
	{
		const FBox ThisBounds = GetRuntimeBounds();
		const FBox OtherBounds = Other->GetRuntimeBounds();
		const FVector BoundsChangeTolerance = ThisBounds.GetSize() * 0.05f;
		const FVector MinDiff = FVector(OtherBounds.Min - ThisBounds.Min).GetAbs();
		const FVector MaxDiff = FVector(OtherBounds.Max - ThisBounds.Max).GetAbs();
		if ((MinDiff.X > BoundsChangeTolerance.X) || (MaxDiff.X > BoundsChangeTolerance.X) ||
			(MinDiff.Y > BoundsChangeTolerance.Y) || (MaxDiff.Y > BoundsChangeTolerance.Y) ||
			(MinDiff.Z > BoundsChangeTolerance.Z) || (MaxDiff.Z > BoundsChangeTolerance.Z))
		{
			return true;
		}
	}

	// If the actor descriptor says the actor is HLOD relevant but in reality it's not, this will incur a loading time penalty during HLOD generation
	// but will not affect the final result, as the value from the loaded actor will be used instead, so don't consider this as affecting streaming generation.
	return !bActorIsHLODRelevant && Other->bActorIsHLODRelevant;
}

void FWorldPartitionActorDesc::SerializeTo(TArray<uint8>& OutData) const
{
	FWorldPartitionActorDesc* MutableThis = const_cast<FWorldPartitionActorDesc*>(this);

	// Serialize to archive and gather custom versions
	TArray<uint8> PayloadData;
	FMemoryWriter PayloadAr(PayloadData, true);
	FActorDescArchive ActorDescAr(PayloadAr, MutableThis);
	ActorDescAr.Init();

	MutableThis->Serialize(ActorDescAr);

	// Serialize custom versions
	TArray<uint8> HeaderData;
	FMemoryWriter HeaderAr(HeaderData);
	FCustomVersionContainer CustomVersions = ActorDescAr.GetCustomVersions();
	CustomVersions.Serialize(HeaderAr); 

	// Append data
	OutData = MoveTemp(HeaderData);
	OutData.Append(PayloadData);
}

TArray<FName> FWorldPartitionActorDesc::GetDataLayers(bool bIncludeExternalDataLayer) const
{
	const FName ExternalDataLayer = GetExternalDataLayer();
	if (!bIncludeExternalDataLayer || ExternalDataLayer.IsNone())
	{
		return DataLayers;
	}

	TArray<FName> AllDataLayers;
	AllDataLayers.Reserve(DataLayers.Num() + 1);
	AllDataLayers.Add(ExternalDataLayer);
	AllDataLayers.Append(DataLayers);
	return AllDataLayers;
}

FName FWorldPartitionActorDesc::GetExternalDataLayer() const
{
	const bool bHasExternalDataLayerAsset = bIsUsingDataLayerAsset && ExternalDataLayerAsset.IsValid();
	return bHasExternalDataLayerAsset ? FName(ExternalDataLayerAsset.GetAssetPath().ToString()) : NAME_None;
}

void FWorldPartitionActorDesc::TransferFrom(const FWorldPartitionActorDesc* From)
{
	Container = From->Container;
}

void FWorldPartitionActorDesc::RegisterActorDescDeprecator(TSubclassOf<AActor> ActorClass, const FActorDescDeprecator& Deprecator)
{
	check(!Deprecators.Contains(ActorClass));
	Deprecators.Add(ActorClass, Deprecator);
}

FString FWorldPartitionActorDesc::ToString(EToStringMode Mode) const
{
	auto GetBoolStr = [](bool bValue) -> const TCHAR*
	{
		return bValue ? TEXT("1") : TEXT("0");
	};

	TStringBuilder<1024> Result;
	Result.Appendf(TEXT("Guid:%s"), *Guid.ToString());

	if (Mode >= EToStringMode::Compact)
	{
		FString BoundsStr;

		if (bIsBoundsValid)
		{
			const FBox EditorBounds = GetEditorBounds();
			const FBox RuntimeBounds = GetRuntimeBounds();
			if (EditorBounds.Equals(RuntimeBounds))
			{
				BoundsStr = RuntimeBounds.ToString();
			}
			else
			{
				BoundsStr = *FString::Printf(TEXT("(Editor:%s Runtime:%s)"), *EditorBounds.ToString(), *RuntimeBounds.ToString());
			}
		}
		else
		{
			BoundsStr = TEXT("Invalid");
		}

		Result.Appendf(
			TEXT(" BaseClass:%s NativeClass:%s Name:%s Label:%s SpatiallyLoaded:%s Bounds:%s RuntimeGrid:%s EditorOnly:%s RuntimeOnly:%s HLODRelevant:%s ListedInSceneOutliner:%s IsMainWorldOnly:%s"),
			*BaseClass.ToString(), 
			*NativeClass.ToString(), 
			*GetActorName().ToString(),
			*GetActorLabel().ToString(),
			GetBoolStr(bIsSpatiallyLoaded),
			*BoundsStr,
			*RuntimeGrid.ToString(),
			GetBoolStr(bActorIsEditorOnly),
			GetBoolStr(bActorIsRuntimeOnly),
			GetBoolStr(bActorIsHLODRelevant),
			GetBoolStr(bActorIsListedInSceneOutliner),
			GetBoolStr(IsMainWorldOnly())
		);

		if (ParentActor.IsValid())
		{
			Result.Appendf(TEXT(" Parent:%s"), *ParentActor.ToString());
		}

		if (HLODLayer.IsValid())
		{
			Result.Appendf(TEXT(" HLODLayer:%s"), *HLODLayer.ToString());
		}

		if (!FolderPath.IsNone())
		{
			Result.Appendf(TEXT(" FolderPath:%s"), *FolderPath.ToString());
		}

		if (FolderGuid.IsValid())
		{
			Result.Appendf(TEXT(" FolderGuid:%s"), *FolderGuid.ToString());
		}

		if (Mode >= EToStringMode::Full)
		{
			if (References.Num())
			{
				Result.Appendf(TEXT(" References:%s"), *FString::JoinBy(References, TEXT(","), [&](const FGuid& ReferenceGuid) { return ReferenceGuid.ToString(); }));
			}

			if (EditorOnlyReferences.Num())
			{
				Result.Appendf(TEXT(" EditorOnlyReferences:%s"), *FString::JoinBy(EditorOnlyReferences, TEXT(","), [&](const FGuid& ReferenceGuid) { return ReferenceGuid.ToString(); }));
			}

			if (Tags.Num())
			{
				Result.Appendf(TEXT(" Tags:%s"), *FString::JoinBy(Tags, TEXT(","), [&](const FName& TagName) { return TagName.ToString(); }));
			}

			if (Properties.Num())
			{
				Result.Appendf(TEXT(" Properties:%s"), *Properties.ToString());
			}

			if (DataLayers.Num())
			{
				Result.Appendf(TEXT(" DataLayers:%s"), *FString::JoinBy(DataLayers, TEXT(","), [&](const FName& DataLayerName) { return DataLayerName.ToString(); }));
			}

			if (ExternalDataLayerAsset.IsValid())
			{
				Result.Appendf(TEXT(" ExternalDataLayerAsset:%s"), *ExternalDataLayerAsset.ToString());
			}
		}
	}

	return Result.ToString();
}

void FWorldPartitionActorDesc::Serialize(FArchive& Ar)
{
	check(Ar.IsPersistent());

	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5ReleaseStreamObjectVersion::GUID);
	Ar.UsingCustomVersion(FFortniteSeasonBranchObjectVersion::GUID);
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);

	if (bIsDefaultActorDesc)
	{
		if (Ar.IsLoading())
		{
			Guid = GetDefaultActorDescGuid(this);

			if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::WorldPartitionClasDescGuidTransient)
			{
				FGuid ClassDescGuid;
				Ar << ClassDescGuid;
			}
		}
	}
	else
	{
		Ar << Guid;

		if (Ar.CustomVer(FFortniteSeasonBranchObjectVersion::GUID) >= FFortniteSeasonBranchObjectVersion::WorldPartitionActorDescActorTransformSerialization)
		{
			Ar << ActorTransform;
		}
	}

	if(Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) < FUE5ReleaseStreamObjectVersion::LargeWorldCoordinates)
	{
		FVector3f BoundsLocationFlt, BoundsExtentFlt;
		Ar << BoundsLocationFlt << BoundsExtentFlt;
		BoundsLocation = FVector(BoundsLocationFlt);
		BoundsExtent = FVector(BoundsExtentFlt);
		bIsBoundsValid = true;
	}
	else if (!bIsDefaultActorDesc)
	{
		if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::WorldPartitionActorDescSerializeInvalidBounds)
		{
			bIsBoundsValid = true;
		}
		else
		{
			Ar << bIsBoundsValid;
		}

		if (bIsBoundsValid)
		{
			Ar << BoundsLocation << BoundsExtent;
		}
	}
	
	if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) < FUE5ReleaseStreamObjectVersion::ConvertedActorGridPlacementToSpatiallyLoadedFlag)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		EActorGridPlacement GridPlacement;
		Ar << (__underlying_type(EActorGridPlacement)&)GridPlacement;
		bIsSpatiallyLoaded = GridPlacement != EActorGridPlacement::AlwaysLoaded;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	else
	{
		Ar << TDeltaSerialize<bool>(bIsSpatiallyLoaded);
	}

	Ar << TDeltaSerialize<FName>(RuntimeGrid);
	Ar << TDeltaSerialize<bool>(bActorIsEditorOnly);

	if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) >= FFortniteMainBranchObjectVersion::WorldPartitionActorDescSerializeActorIsRuntimeOnly)
	{
		Ar << TDeltaSerialize<bool>(bActorIsRuntimeOnly);
	}
	else
	{
		bActorIsRuntimeOnly = false;
	}

	if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::WorldPartitionActorDescRemoveBoundsRelevantSerialization)
	{
		bool bLevelBoundsRelevant;
		Ar << bLevelBoundsRelevant;
	}
	
	if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::WorldPartitionActorDescSerializeDataLayers)
	{
		TArray<FName> Deprecated_Layers;
		Ar << Deprecated_Layers;
	}

	Ar << References;

	if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) >= FFortniteMainBranchObjectVersion::WorldPartitionActorDescSerializeEditorOnlyReferences)
	{
		Ar << EditorOnlyReferences;
	}

	if (Ar.CustomVer(FFortniteSeasonBranchObjectVersion::GUID) >= FFortniteSeasonBranchObjectVersion::WorldPartitionActorDescTagsSerialization)
	{
		Ar << TDeltaSerialize<TArray<FName>>(Tags);
	}

	if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::WorldPartitionActorDescSerializeArchivePersistent)
	{
		Ar << ActorPackage;
		if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::WorldPartitionActorDescActorAndClassPaths)
		{ 
			FName ActorPathName;
			Ar << ActorPathName;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			ActorPath = FSoftObjectPath(ActorPathName);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
		else
		{
			Ar << ActorPath;
		}
	}

	if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) >= FUE5MainStreamObjectVersion::WorldPartitionActorDescSerializeDataLayers)
	{
		Ar << TDeltaSerialize<TArray<FName>>(DataLayers);
	}

	if (Ar.CustomVer(FFortniteSeasonBranchObjectVersion::GUID) >= FFortniteSeasonBranchObjectVersion::WorldPartitionActorDescSerializeDataLayerAssets)
	{
		Ar << TDeltaSerialize<bool>(bIsUsingDataLayerAsset);
	}

	if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) >= FUE5MainStreamObjectVersion::WorldPartitionActorDescSerializeActorLabel)
	{
		Ar << TDeltaSerialize<FName>(ActorLabel);
	}

	if ((Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) >= FUE5MainStreamObjectVersion::WorldPartitionActorDescSerializeHLODInfo) ||
		(Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) >= FUE5ReleaseStreamObjectVersion::WorldPartitionActorDescSerializeHLODInfo))
	{
		Ar << TDeltaSerialize<bool>(bActorIsHLODRelevant);

		if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::WorldPartitionActorDescSerializeSoftObjectPathSupport)
		{
			Ar << TDeltaSerialize<FSoftObjectPath, FName>(HLODLayer, [](FSoftObjectPath& Value, const FName& DeprecatedValue)
			{
				Value = FSoftObjectPath(*DeprecatedValue.ToString());
			});
		}
		else
		{
			Ar << TDeltaSerialize<FSoftObjectPath>(HLODLayer);
		}
	}
	else
	{
		bActorIsHLODRelevant = true;
		HLODLayer = FSoftObjectPath();
	}

	if (!bIsDefaultActorDesc)
	{
		if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) >= FUE5ReleaseStreamObjectVersion::WorldPartitionActorDescSerializeActorFolderPath)
		{
			Ar << FolderPath;
		}

		if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) >= FUE5ReleaseStreamObjectVersion::WorldPartitionActorDescSerializeAttachParent)
		{
			Ar << ParentActor;
		}

		if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) >= FUE5ReleaseStreamObjectVersion::AddLevelActorFolders)
		{
			Ar << FolderGuid;
		}

		if (Ar.CustomVer(FFortniteSeasonBranchObjectVersion::GUID) >= FFortniteSeasonBranchObjectVersion::WorldPartitionActorDescPropertyMapSerialization)
		{
			Ar << Properties;
		}
	}

	if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::WorldPartitionActorDescSerializeContentBundleGuid)
	{
		ContentBundleGuid = ContentBundlePaths::GetContentBundleGuidFromExternalActorPackagePath(ActorPackage.ToString());
	}
	else
	{
		Ar << TDeltaSerialize<FGuid>(ContentBundleGuid);

		// @todo_ow: remove once we find why some actors end up with invalid ContentBundleGuids
		if (Ar.IsLoading())
		{
			FGuid FixupContentBundleGuid = ContentBundlePaths::GetContentBundleGuidFromExternalActorPackagePath(ActorPackage.ToString());
			if (ContentBundleGuid != FixupContentBundleGuid)
			{
				UE_LOG(LogWorldPartition, Log, TEXT("ActorDesc ContentBundleGuid was fixed up: %s"), *GetActorName().ToString());
				ContentBundleGuid = FixupContentBundleGuid;
			}
		}
	}

	if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) >= FFortniteMainBranchObjectVersion::WorldPartitionActorDescIsMainWorldOnly)
	{
		Ar << TDeltaSerialize<bool>(bActorIsMainWorldOnly);
	}

	if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) >= FUE5ReleaseStreamObjectVersion::WorldPartitionActorDescSerializeActorIsListedInSceneOutliner)
	{
		Ar << TDeltaSerialize<bool>(bActorIsListedInSceneOutliner);
	}

    if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) >= FUE5ReleaseStreamObjectVersion::WorldPartitionExternalDataLayers)
	{
		Ar << TDeltaSerialize<FSoftObjectPath>(ExternalDataLayerAsset);
	}

	// Fixup redirected data layer asset paths
	if (Ar.IsLoading() && bIsUsingDataLayerAsset)
	{
		for (FName& DataLayer : DataLayers)
		{
			UAssetRegistryHelpers::FixupRedirectedAssetPath(DataLayer);
		}
	}
}

FBox FWorldPartitionActorDesc::GetEditorBounds() const
{
	return bIsBoundsValid ? FBox(BoundsLocation - BoundsExtent, BoundsLocation + BoundsExtent) : FBox(ForceInit);
}

FBox FWorldPartitionActorDesc::GetRuntimeBounds() const
{
	return bIsBoundsValid ? FBox(BoundsLocation - BoundsExtent, BoundsLocation + BoundsExtent) : FBox(ForceInit);
}

FName FWorldPartitionActorDesc::GetActorName() const
{
	return ActorName;
}

FName FWorldPartitionActorDesc::GetActorLabelOrName() const
{
	return GetActorLabel().IsNone() ? GetActorName() : GetActorLabel();
}

FName FWorldPartitionActorDesc::GetDisplayClassName() const
{
	auto GetCleanClassName = [](FTopLevelAssetPath ClassPath) -> FName
	{
		// Should this just return ClassPath.GetAssetName() with _C removed if necessary?
		int32 Index;
		FString ClassNameStr(ClassPath.ToString());
		if (ClassNameStr.FindLastChar(TCHAR('.'), Index))
		{
			FString CleanClassName = ClassNameStr.Mid(Index + 1);
			CleanClassName.RemoveFromEnd(TEXT("_C"));
			return *CleanClassName;
		}
		return *ClassPath.ToString(); 
	};

	return BaseClass.IsNull() ? GetCleanClassName(NativeClass) : GetCleanClassName(BaseClass);
}

FGuid FWorldPartitionActorDesc::GetContentBundleGuid() const
{
	return ContentBundleGuid;
}

void FWorldPartitionActorDesc::CheckForErrors(const IWorldPartitionActorDescInstanceView* InActorDescView, IStreamingGenerationErrorHandler* ErrorHandler) const
{
	if (IsResaveNeeded())
	{
		ErrorHandler->OnActorNeedsResave(*InActorDescView);
	}
}

bool FWorldPartitionActorDesc::IsMainWorldOnly() const
{
	return bActorIsMainWorldOnly || CastChecked<AActor>(ActorNativeClass->GetDefaultObject())->IsMainWorldOnly();
}

bool FWorldPartitionActorDesc::IsListedInSceneOutliner() const
{
	return bActorIsListedInSceneOutliner;
}

bool FWorldPartitionActorDesc::IsEditorRelevant(const FWorldPartitionActorDescInstance* InActorDescInstance) const
{
	if (GetActorIsRuntimeOnly())
	{
		return false;
	}

	if (IsMainWorldOnly())
	{
		return InActorDescInstance->GetContainerInstance()->GetContainerID().IsMainContainer();
	}

	return true;
}

bool FWorldPartitionActorDesc::IsRuntimeRelevant(const FWorldPartitionActorDescInstance* InActorDescInstance) const
{
	return !IsMainWorldOnly() || InActorDescInstance->GetContainerInstance()->GetContainerID().IsMainContainer();
}

#undef LOCTEXT_NAMESPACE

#endif
