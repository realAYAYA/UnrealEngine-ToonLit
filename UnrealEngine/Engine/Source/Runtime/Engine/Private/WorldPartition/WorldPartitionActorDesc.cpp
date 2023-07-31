// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/WorldPartitionActorDescArchive.h"

#include "Misc/HashBuilder.h"
#include "UObject/LinkerInstancingContext.h"
#include "UObject/UObjectHash.h"
#include "UObject/MetaData.h"
#include "AssetRegistry/AssetData.h"
#include "Algo/Transform.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Misc/PackageName.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "UObject/UE5ReleaseStreamObjectVersion.h"
#include "UObject/FortniteNCBranchObjectVersion.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "WorldPartition/WorldPartitionLog.h"
#include "WorldPartition/ActorDescContainer.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerUtils.h"
#include "WorldPartition/ContentBundle/ContentBundlePaths.h"
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationErrorHandler.h"
#include "ActorReferencesUtils.h"

TMap<TSubclassOf<AActor>, FWorldPartitionActorDesc::FActorDescDeprecator> FWorldPartitionActorDesc::Deprecators;

FWorldPartitionActorDesc::FWorldPartitionActorDesc()
	: bIsUsingDataLayerAsset(false)
	, SoftRefCount(0)
	, HardRefCount(0)
	, Container(nullptr)
	, bIsForcedNonSpatiallyLoaded(false)
{}

void FWorldPartitionActorDesc::Init(const AActor* InActor)
{	
	check(InActor->IsPackageExternal());

	Guid = InActor->GetActorGuid();
	check(Guid.IsValid());

	UClass* ActorClass = InActor->GetClass();

	// Get the first native class in the hierarchy
	ActorNativeClass = GetParentNativeClass(ActorClass);
	NativeClass = *ActorNativeClass->GetPathName();
	
	// For native class, don't set this
	if (!ActorClass->IsNative())
	{
		BaseClass = *InActor->GetClass()->GetPathName();
	}

	const FBox StreamingBounds = InActor->GetStreamingBounds();
	StreamingBounds.GetCenterAndExtents(BoundsLocation, BoundsExtent);

	RuntimeGrid = InActor->GetRuntimeGrid();
	bIsSpatiallyLoaded = InActor->GetIsSpatiallyLoaded();
	bActorIsEditorOnly = InActor->IsEditorOnly();
	bActorIsRuntimeOnly = InActor->IsRuntimeOnly();
	bActorIsHLODRelevant = InActor->IsHLODRelevant();
	HLODLayer = InActor->GetHLODLayer() ? FName(InActor->GetHLODLayer()->GetPathName()) : FName();
	
	// DataLayers
	{
		TArray<FName> LocalDataLayerAssetPaths;
		TArray<FName> LocalDataLayerInstanceNames;
		ULevel* Level = InActor->GetLevel();
		if (UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(InActor->GetWorld()))
		{
			LocalDataLayerAssetPaths.Reserve(InActor->GetDataLayerAssets().Num());
			for (const TObjectPtr<const UDataLayerAsset>& DataLayerAsset : InActor->GetDataLayerAssets())
			{
				// Pass Actor Level when resolving the DataLayerInstance as FWorldPartitionActorDesc always represents the state of the actor local to its outer level
				if (DataLayerAsset && DataLayerSubsystem->GetDataLayerInstance(DataLayerAsset, Level))
				{
					LocalDataLayerAssetPaths.Add(*DataLayerAsset->GetPathName());
				}
			}

			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			// Pass Actor Level when resolving the DataLayerInstance as FWorldPartitionActorDesc always represents the state of the actor local to its outer level
			LocalDataLayerInstanceNames = DataLayerSubsystem->GetDataLayerInstanceNames(InActor->GetActorDataLayers(), Level);
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}

		// Validation
		const bool bHasDataLayerAssets = LocalDataLayerAssetPaths.Num() > 0;
		const bool bHasDeprecatedDataLayers = LocalDataLayerInstanceNames.Num() > 0;
		check((!bHasDataLayerAssets && !bHasDeprecatedDataLayers) || (bHasDataLayerAssets != bHasDeprecatedDataLayers));

		// Init DataLayers persistent info
		bIsUsingDataLayerAsset = bHasDataLayerAssets;
		DataLayers = bIsUsingDataLayerAsset ? MoveTemp(LocalDataLayerAssetPaths) : MoveTemp(LocalDataLayerInstanceNames);

		// Init DataLayers transient info
		DataLayerInstanceNames = FDataLayerUtils::ResolvedDataLayerInstanceNames(this, TArray<const FWorldDataLayersActorDesc*>(), InActor->GetWorld());
	}

	Tags = InActor->Tags;

	check(Properties.IsEmpty());
	InActor->GetActorDescProperties(Properties);

	ActorPackage = InActor->GetPackage()->GetFName();
	ActorPath = *InActor->GetPathName();
	FolderPath = InActor->GetFolderPath();
	FolderGuid = InActor->GetFolderGuid();

	const AActor* AttachParentActor = InActor->GetAttachParentActor();
	if (AttachParentActor)
	{
		ParentActor = AttachParentActor->GetActorGuid();
	}

	ContentBundleGuid = InActor->GetContentBundleGuid();
	
	TArray<AActor*> ActorReferences = ActorsReferencesUtils::GetExternalActorReferences(const_cast<AActor*>(InActor));

	if (ActorReferences.Num())
	{
		References.Empty(ActorReferences.Num());
		for(AActor* ActorReference: ActorReferences)
		{
			References.Add(ActorReference->GetActorGuid());
		}
	}

	ActorLabel = *InActor->GetActorLabel(false);

	Container = nullptr;
	ActorPtr = const_cast<AActor*>(InActor);
}

void FWorldPartitionActorDesc::Init(const FWorldPartitionActorDescInitData& DescData)
{
	ActorPackage = DescData.PackageName;
	ActorPath = DescData.ActorPath;
	ActorNativeClass = DescData.NativeClass;
	NativeClass = *DescData.NativeClass->GetPathName();

	// Serialize actor metadata
	FMemoryReader MetadataAr(DescData.SerializedData, true);

	// Serialize metadata custom versions
	FCustomVersionContainer CustomVersions;
	CustomVersions.Serialize(MetadataAr);
	MetadataAr.SetCustomVersions(CustomVersions);
	
	// Serialize metadata payload
	FActorDescArchive ActorDescAr(MetadataAr);
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

bool FWorldPartitionActorDesc::Equals(const FWorldPartitionActorDesc* Other) const
{
	return
		Guid == Other->Guid &&
		BaseClass == Other->BaseClass &&
		NativeClass == Other->NativeClass &&
		ActorPackage == Other->ActorPackage &&
		ActorPath == Other->ActorPath &&
		ActorLabel == Other->ActorLabel &&
		BoundsLocation.Equals(Other->BoundsLocation, 0.1f) &&
		BoundsExtent.Equals(Other->BoundsExtent, 0.1f) &&
		RuntimeGrid == Other->RuntimeGrid &&
		bActorIsEditorOnly == Other->bActorIsEditorOnly &&
		bActorIsRuntimeOnly == Other->bActorIsRuntimeOnly &&
		bActorIsHLODRelevant == Other->bActorIsHLODRelevant &&
		bIsUsingDataLayerAsset == Other->bIsUsingDataLayerAsset &&
		HLODLayer == Other->HLODLayer &&
		FolderPath == Other->FolderPath &&
		FolderGuid == Other->FolderGuid &&
		ParentActor == Other->ParentActor &&
		CompareUnsortedArrays(DataLayers, Other->DataLayers) &&
		CompareUnsortedArrays(References, Other->References) &&
		CompareUnsortedArrays(Tags, Other->Tags) &&
		Properties == Other->Properties;
}

void FWorldPartitionActorDesc::SerializeTo(TArray<uint8>& OutData)
{
	// Serialize to archive and gather custom versions
	TArray<uint8> PayloadData;
	FMemoryWriter PayloadAr(PayloadData, true);
	FActorDescArchive ActorDescAr(PayloadAr);

	// Serialize actor descriptor
	Serialize(ActorDescAr);

	// Serialize custom versions
	TArray<uint8> HeaderData;
	FMemoryWriter HeaderAr(HeaderData);
	FCustomVersionContainer CustomVersions = ActorDescAr.GetCustomVersions();
	CustomVersions.Serialize(HeaderAr);

	// Append data
	OutData = MoveTemp(HeaderData);
	OutData.Append(PayloadData);
}

void FWorldPartitionActorDesc::RegisterActorDescDeprecator(TSubclassOf<AActor> ActorClass, const FActorDescDeprecator& Deprecator)
{
	check(!Deprecators.Contains(ActorClass));
	Deprecators.Add(ActorClass, Deprecator);
}

void FWorldPartitionActorDesc::TransformInstance(const FString& From, const FString& To, const FTransform& InstanceTransform)
{
	check(!HardRefCount);

	ActorPath = *ActorPath.ToString().Replace(*From, *To);

	// Transform BoundsLocation and BoundsExtent if necessary
	if (!InstanceTransform.Equals(FTransform::Identity))
	{
		//@todo_ow: This will result in a new BoundsExtent that is larger than it should. To fix this, we would need the Object Oriented BoundingBox of the actor (the BV of the actor without rotation)
		const FVector BoundsMin = BoundsLocation - BoundsExtent;
		const FVector BoundsMax = BoundsLocation + BoundsExtent;
		const FBox NewBounds = FBox(BoundsMin, BoundsMax).TransformBy(InstanceTransform);
		NewBounds.GetCenterAndExtents(BoundsLocation, BoundsExtent);
	}
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
		Result.Appendf(
			TEXT(" BaseClass:%s NativeClass:%s Name:%s Label:%s SpatiallyLoaded:%s Bounds:%s RuntimeGrid:%s EditorOnly:%s RuntimeOnly:%s HLODRelevant:%s"),
			*BaseClass.ToString(), 
			*NativeClass.ToString(), 
			*GetActorName().ToString(),
			*GetActorLabel().ToString(),
			GetBoolStr(bIsSpatiallyLoaded),
			*GetBounds().ToString(),
			*RuntimeGrid.ToString(),
			GetBoolStr(bActorIsEditorOnly),
			GetBoolStr(bActorIsRuntimeOnly),
			GetBoolStr(bActorIsHLODRelevant)
		);

		if (ParentActor.IsValid())
		{
			Result.Appendf(TEXT(" Parent:%s"), *ParentActor.ToString());
		}

		if (!HLODLayer.IsNone())
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
		}
	}

	return Result.ToString();
}

void FWorldPartitionActorDesc::Serialize(FArchive& Ar)
{
	check(Ar.IsPersistent());

	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5ReleaseStreamObjectVersion::GUID);
	Ar.UsingCustomVersion(FFortniteNCBranchObjectVersion::GUID);
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);

	if (Ar.CustomVer(FFortniteNCBranchObjectVersion::GUID) >= FFortniteNCBranchObjectVersion::WorldPartitionActorDescNativeBaseClassSerialization)
	{
		if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::WorldPartitionActorDescActorAndClassPaths)
		{
			FName BaseClassPathName;
			Ar << BaseClassPathName;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			BaseClass = FAssetData::TryConvertShortClassNameToPathName(BaseClassPathName);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
		else
		{
			Ar << BaseClass;
		}
	}

	if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::WorldPartitionActorDescActorAndClassPaths)
	{
		FName NativeClassPathName;
		Ar << NativeClassPathName;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		NativeClass = FAssetData::TryConvertShortClassNameToPathName(NativeClassPathName);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	else
	{
		Ar << NativeClass;
	}

	Ar << Guid;

	if(Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) < FUE5ReleaseStreamObjectVersion::LargeWorldCoordinates)
	{
		FVector3f BoundsLocationFlt, BoundsExtentFlt;
		Ar << BoundsLocationFlt << BoundsExtentFlt;
		BoundsLocation = FVector(BoundsLocationFlt);
		BoundsExtent = FVector(BoundsExtentFlt);
	}
	else
	{
		Ar << BoundsLocation << BoundsExtent;
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
		Ar << bIsSpatiallyLoaded;
	}
		
	Ar << RuntimeGrid << bActorIsEditorOnly;

	if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) >= FFortniteMainBranchObjectVersion::WorldPartitionActorDescSerializeActorIsRuntimeOnly)
	{
		Ar << bActorIsRuntimeOnly;
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

	if (Ar.CustomVer(FFortniteNCBranchObjectVersion::GUID) >= FFortniteNCBranchObjectVersion::WorldPartitionActorDescTagsSerialization)
	{
		Ar << Tags;
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
		Ar << DataLayers;
	}

	if (Ar.CustomVer(FFortniteNCBranchObjectVersion::GUID) >= FFortniteNCBranchObjectVersion::WorldPartitionActorDescSerializeDataLayerAssets)
	{
		Ar << bIsUsingDataLayerAsset;
	}

	if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) >= FUE5MainStreamObjectVersion::WorldPartitionActorDescSerializeActorLabel)
	{
		Ar << ActorLabel;
	}

	if ((Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) >= FUE5MainStreamObjectVersion::WorldPartitionActorDescSerializeHLODInfo) ||
		(Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) >= FUE5ReleaseStreamObjectVersion::WorldPartitionActorDescSerializeHLODInfo))
	{
		Ar << bActorIsHLODRelevant;
		Ar << HLODLayer;
	}
	else
	{
		bActorIsHLODRelevant = true;
		HLODLayer = FName();
	}

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

	if (Ar.CustomVer(FFortniteNCBranchObjectVersion::GUID) >= FFortniteNCBranchObjectVersion::WorldPartitionActorDescPropertyMapSerialization)
	{
		Ar << Properties;
	}

	if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::WorldPartitionActorDescSerializeContentBundleGuid)
	{
		ContentBundleGuid = ContentBundlePaths::GetContentBundleGuidFromExternalActorPackagePath(ActorPackage.ToString());
	}
	else
	{
		Ar << ContentBundleGuid;
	}
}

FBox FWorldPartitionActorDesc::GetBounds() const
{
	return FBox(BoundsLocation - BoundsExtent, BoundsLocation + BoundsExtent);
}

FName FWorldPartitionActorDesc::GetActorName() const
{
	return *FPaths::GetExtension(ActorPath.ToString());
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

void FWorldPartitionActorDesc::CheckForErrors(IStreamingGenerationErrorHandler* ErrorHandler) const
{
	if (IsResaveNeeded())
	{
		ErrorHandler->OnActorNeedsResave(this);
	}
}

bool FWorldPartitionActorDesc::IsLoaded(bool bEvenIfPendingKill) const
{
	if (ActorPtr.IsExplicitlyNull() || ActorPtr.IsStale())
	{
		ActorPtr = FindObject<AActor>(nullptr, *ActorPath.ToString());
	}

	return ActorPtr.IsValid(bEvenIfPendingKill);
}

AActor* FWorldPartitionActorDesc::GetActor(bool bEvenIfPendingKill, bool bEvenIfUnreachable) const
{
	if (ActorPtr.IsExplicitlyNull() || ActorPtr.IsStale())
	{
		ActorPtr = FindObject<AActor>(nullptr, *ActorPath.ToString());
	}

	return bEvenIfUnreachable ? ActorPtr.GetEvenIfUnreachable() : ActorPtr.Get(bEvenIfPendingKill);
}

AActor* FWorldPartitionActorDesc::Load() const
{
	if (ActorPtr.IsExplicitlyNull() || ActorPtr.IsStale())
	{
		// First, try to find the existing actor which could have been loaded by another actor (through standard serialization)
		ActorPtr = FindObject<AActor>(nullptr, *ActorPath.ToString());
	}

	// Then, if the actor isn't loaded, load it
	if (ActorPtr.IsExplicitlyNull())
	{
		const FLinkerInstancingContext* InstancingContext = Container ? Container->GetInstancingContext() : nullptr;

		UPackage* Package = nullptr;

		if (InstancingContext)
		{
			FName RemappedPackageName = InstancingContext->RemapPackage(ActorPackage);
			check(RemappedPackageName != ActorPath.GetLongPackageFName());

			Package = CreatePackage(*RemappedPackageName.ToString());
		}

		Package = LoadPackage(Package, *ActorPackage.ToString(), LOAD_None, nullptr, InstancingContext);

		if (Package)
		{
			ActorPtr = FindObject<AActor>(nullptr, *ActorPath.ToString());
			if (!ActorPtr.IsValid())
			{
				UE_LOG(LogWorldPartition, Warning, TEXT("Can't load actor guid `%s` ('%s') from package '%s'"), *Guid.ToString(), *GetActorName().ToString(), *ActorPackage.ToString());
			}
		}
	}

	return ActorPtr.Get();
}

void FWorldPartitionActorDesc::Unload()
{
	if (AActor* Actor = GetActor())
	{
		// At this point, it can happen that an actor isn't in an external package:
		//
		// PIE travel: 
		//		in this case, actors referenced by the world package (an example is the level script) will be duplicated as part of the PIE world duplication and will end up
		//		not being using an external package, which is fine because in that case they are considered as always loaded.
		//
		// FWorldPartitionCookPackageSplitter:
		//		should mark each FWorldPartitionActorDesc as moved, and the splitter should take responsbility for calling ClearFlags on every object in 
		//		the package when it does the move

		if (Actor->IsPackageExternal())
		{
			ForEachObjectWithPackage(Actor->GetPackage(), [](UObject* Object)
			{
				if (Object->HasAnyFlags(RF_Public | RF_Standalone))
				{
					CastChecked<UMetaData>(Object)->ClearFlags(RF_Public | RF_Standalone);
				}
				return true;
			}, false);
		}

		ActorPtr = nullptr;
	}
}
#endif
