// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/LevelInstance/LevelInstanceActorDesc.h"

#if WITH_EDITOR
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "LevelInstance/LevelInstanceComponent.h"
#include "Misc/PackageName.h"
#include "WorldPartition/ActorDescContainerInstance.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "WorldPartition/WorldPartitionActorDescInstanceViewInterface.h"
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationErrorHandler.h"
#include "WorldPartition/WorldPartitionActorDescArchive.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/ActorDescContainerSubsystem.h"
#include "UObject/UE5ReleaseStreamObjectVersion.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "UObject/FortniteSeasonBranchObjectVersion.h"
#include "UObject/FortniteMainBranchObjectVersion.h"

static int32 GLevelInstanceDebugForceLevelStreaming = 0;
static FAutoConsoleVariableRef CVarForceLevelStreaming(
	TEXT("levelinstance.debug.forcelevelstreaming"),
	GLevelInstanceDebugForceLevelStreaming,
	TEXT("Set to 1 to force Level Instance to be streamed instead of embedded in World Partition grid."));

FLevelInstanceActorDesc::FLevelInstanceActorDesc()
	: DesiredRuntimeBehavior(ELevelInstanceRuntimeBehavior::Partitioned)
	, ChildContainer(nullptr)
	, bIsChildContainerInstance(false)
{}

FLevelInstanceActorDesc::~FLevelInstanceActorDesc()
{
	if (!IsEngineExitRequested())
	{
		UnregisterChildContainer();
		check(!ChildContainer.IsValid());
	}
}

void FLevelInstanceActorDesc::Init(const AActor* InActor)
{
	FWorldPartitionActorDesc::Init(InActor);

	const ILevelInstanceInterface* LevelInstance = CastChecked<ILevelInstanceInterface>(InActor);
	WorldAsset = LevelInstance->GetWorldAsset().ToSoftObjectPath();
	DesiredRuntimeBehavior = LevelInstance->GetDesiredRuntimeBehavior();
	Filter = LevelInstance->GetFilter();
	
	bIsChildContainerInstance = IsChildContainerInstanceInternal();
}

void FLevelInstanceActorDesc::Init(const FWorldPartitionActorDescInitData& DescData)
{
	AActor* CDO = DescData.NativeClass->GetDefaultObject<AActor>();
	ILevelInstanceInterface* LevelInstanceCDO = CastChecked<ILevelInstanceInterface>(CDO);
	DesiredRuntimeBehavior = LevelInstanceCDO->GetDefaultRuntimeBehavior();

	FWorldPartitionActorDesc::Init(DescData);
	
	bIsChildContainerInstance = IsChildContainerInstanceInternal();
}

FString FLevelInstanceActorDesc::GetChildContainerName() const
{
	return GetChildContainerPackage().ToString();
}

bool FLevelInstanceActorDesc::Equals(const FWorldPartitionActorDesc* Other) const
{
	if (FWorldPartitionActorDesc::Equals(Other))
	{
		const FLevelInstanceActorDesc* LevelInstanceActorDesc = (FLevelInstanceActorDesc*)Other;

		return
			WorldAsset == LevelInstanceActorDesc->WorldAsset &&
			DesiredRuntimeBehavior == LevelInstanceActorDesc->DesiredRuntimeBehavior;
	}

	return false;
}

void FLevelInstanceActorDesc::UpdateBounds()
{
	if (UActorDescContainer* ChildContainerPtr = ChildContainer.Get())
	{
		FBox ContainerBounds = UActorDescContainerSubsystem::GetChecked().GetContainerBounds(GetChildContainerName()).TransformBy(GetChildContainerTransform());

		ContainerBounds.GetCenterAndExtents(BoundsLocation, BoundsExtent);
	}
}

void FLevelInstanceActorDesc::RegisterChildContainer()
{
	check(!ChildContainer.IsValid());
	if (IsChildContainerInstance())
	{
		ChildContainer = UActorDescContainerSubsystem::GetChecked().RegisterContainer({ GetChildContainerName(), GetChildContainerPackage() });
		UpdateBounds();
	}
}

void FLevelInstanceActorDesc::UnregisterChildContainer()
{
	if (UActorDescContainer* ChildContainerPtr = ChildContainer.Get())
	{
		UActorDescContainerSubsystem::GetChecked().UnregisterContainer(ChildContainerPtr);
	}
	ChildContainer = nullptr;
}

void FLevelInstanceActorDesc::SetContainer(UActorDescContainer* InContainer)
{
	FWorldPartitionActorDesc::SetContainer(InContainer);

	if (Container)
	{
		RegisterChildContainer();
	}
	else
	{
		UnregisterChildContainer();
	}
}

bool FLevelInstanceActorDesc::IsChildContainerInstance() const
{
	return bIsChildContainerInstance;
}

bool FLevelInstanceActorDesc::IsChildContainerInstanceInternal() const
{
	if (DesiredRuntimeBehavior != ELevelInstanceRuntimeBehavior::Partitioned)
	{
		return false;
	}
	
	if (GLevelInstanceDebugForceLevelStreaming)
	{
		return false;
	}

	if (WorldAsset.IsNull())
	{
		return false;
	}
	
	if (!ULevel::GetIsLevelUsingExternalActorsFromPackage(GetChildContainerPackage()))
	{
		return false;
	}

	return true;
}

FTransform FLevelInstanceActorDesc::GetChildContainerTransform() const
{
	FTransform LevelInstancePivotOffsetTransform = FTransform(ULevel::GetLevelInstancePivotOffsetFromPackage(GetChildContainerPackage()));
	return LevelInstancePivotOffsetTransform * ActorTransform;
}

bool FLevelInstanceActorDesc::GetChildContainerInstance(const FWorldPartitionActorDescInstance* InActorDescInstance, FContainerInstance& OutContainerInstance) const
{
	if (UActorDescContainerInstance* ContainerInstance = InActorDescInstance->GetChildContainerInstance())
	{
		OutContainerInstance.ContainerInstance = ContainerInstance;
		OutContainerInstance.ClusterMode = EContainerClusterMode::Partitioned;

		// @todo_ow: this is to validate that new parenting of container instance code is equivalent
		OutContainerInstance.Transform = GetChildContainerTransform();

		return true;
	}

	return false;
}

void FLevelInstanceActorDesc::CheckForErrors(const IWorldPartitionActorDescInstanceView* InActorDescView, IStreamingGenerationErrorHandler* ErrorHandler) const
{
	FWorldPartitionActorDesc::CheckForErrors(InActorDescView, ErrorHandler);

	const FName ChildContainerPackage = InActorDescView->GetChildContainerPackage();

	FPackagePath WorldAssetPath;
	if (!FPackagePath::TryFromPackageName(ChildContainerPackage, WorldAssetPath) || !FPackageName::DoesPackageExist(WorldAssetPath))
	{
		ErrorHandler->OnLevelInstanceInvalidWorldAsset(*InActorDescView, ChildContainerPackage, IStreamingGenerationErrorHandler::ELevelInstanceInvalidReason::WorldAssetNotFound);
	}
	else if (!ULevel::GetIsLevelUsingExternalActorsFromPackage(ChildContainerPackage))
	{
		if (DesiredRuntimeBehavior != ELevelInstanceRuntimeBehavior::LevelStreaming)
		{
			ErrorHandler->OnLevelInstanceInvalidWorldAsset(*InActorDescView, ChildContainerPackage, IStreamingGenerationErrorHandler::ELevelInstanceInvalidReason::WorldAssetNotUsingExternalActors);
		}
	}
	else if (!ValidateCircularReference(InActorDescView->GetContainerInstance(), ChildContainerPackage))
	{
		ErrorHandler->OnLevelInstanceInvalidWorldAsset(*InActorDescView, ChildContainerPackage, IStreamingGenerationErrorHandler::ELevelInstanceInvalidReason::CirculalReference);
	}
}

void FLevelInstanceActorDesc::TransferFrom(const FWorldPartitionActorDesc* From)
{
	FWorldPartitionActorDesc::TransferFrom(From);

	FLevelInstanceActorDesc* FromLevelInstanceActorDesc = (FLevelInstanceActorDesc*)From;

	RegisterChildContainer();
	FromLevelInstanceActorDesc->UnregisterChildContainer();
}

UWorldPartition* FLevelInstanceActorDesc::GetLoadedChildWorldPartition(const FWorldPartitionActorDescInstance* InActorDescInstance) const
{
	if (const ILevelInstanceInterface* LevelInstance = Cast<ILevelInstanceInterface>(InActorDescInstance->GetActor()))
	{
		if (ULevel* LoadedLevel = LevelInstance->GetLoadedLevel())
		{
			return LoadedLevel->GetWorldPartition();
		}
	}

	return nullptr;
}

bool FLevelInstanceActorDesc::ValidateCircularReference(const UActorDescContainerInstance* InParentContainer, FName InChildContainerPackage)
{
	const UActorDescContainerInstance* CurrentParentContainerInstance = InParentContainer;
	while (CurrentParentContainerInstance)
	{
		if (CurrentParentContainerInstance->GetContainerPackage() == InChildContainerPackage)
		{
			// found a circular reference
			return false; 
		}
		CurrentParentContainerInstance = Cast<UActorDescContainerInstance>(CurrentParentContainerInstance->GetOuter());
	}

	return true;
}

UActorDescContainerInstance* FLevelInstanceActorDesc::CreateChildContainerInstance(const FWorldPartitionActorDescInstance* InActorDescInstance) const
{
	UActorDescContainerInstance* ContainerInstance = InActorDescInstance->GetContainerInstance();
	if (!ValidateCircularReference(ContainerInstance, InActorDescInstance->GetChildContainerPackage()))
	{
		return nullptr;
	}

	UActorDescContainerInstance* ChildContainerInstance = NewObject<UActorDescContainerInstance>(ContainerInstance, NAME_None, RF_Transient);
	
	// When a child container instance is created we create the whole hierarchy (for generate streaming)
	const bool bInCreateContainerInstanceHierarchy = true;
	UActorDescContainerInstance::FInitializeParams InitParams(InActorDescInstance->GetChildContainerPackage(), bInCreateContainerInstanceHierarchy);
	InitParams.SetParent(ContainerInstance, InActorDescInstance->GetGuid())
		.SetTransform(GetChildContainerTransform() * ContainerInstance->GetTransform());

	ChildContainerInstance->Initialize(InitParams);
		
	return ChildContainerInstance;
}

void FLevelInstanceActorDesc::Serialize(FArchive& Ar)
{
	FWorldPartitionActorDesc::Serialize(Ar);

	Ar.UsingCustomVersion(FUE5ReleaseStreamObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
	Ar.UsingCustomVersion(FFortniteSeasonBranchObjectVersion::GUID);

	if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::WorldPartitionActorDescSerializeSoftObjectPathSupport)
	{
		Ar << TDeltaSerialize<FSoftObjectPath, FName>(WorldAsset, [](FSoftObjectPath& Value, const FName& DeprecatedValue)
		{
			Value = FSoftObjectPath(*DeprecatedValue.ToString());
		});
	}
	else
	{
		Ar << TDeltaSerialize<FSoftObjectPath>(WorldAsset);
	}

	if (!bIsDefaultActorDesc)
	{
		if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) < FUE5ReleaseStreamObjectVersion::LargeWorldCoordinates)
		{
			FTransform3f ActorTransformFlt;
			Ar << ActorTransformFlt;
			ActorTransform = FTransform(ActorTransformFlt);
		}
		else if (Ar.CustomVer(FFortniteSeasonBranchObjectVersion::GUID) < FFortniteSeasonBranchObjectVersion::WorldPartitionActorDescActorTransformSerialization)
		{
			Ar << ActorTransform;
		}
	}

	if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) >= FUE5ReleaseStreamObjectVersion::LevelInstanceSerializeRuntimeBehavior)
	{
		Ar << TDeltaSerialize<ELevelInstanceRuntimeBehavior>(DesiredRuntimeBehavior);

		if (Ar.IsLoading() && DesiredRuntimeBehavior == ELevelInstanceRuntimeBehavior::Embedded_Deprecated)
		{
			DesiredRuntimeBehavior = ELevelInstanceRuntimeBehavior::Partitioned;
		}
	}

	if (!bIsDefaultActorDesc)
	{
		if (Ar.IsLoading())
		{
			const bool bFixupOldVersion = (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::PackedLevelInstanceBoundsFix) && 
										  (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) < FUE5ReleaseStreamObjectVersion::PackedLevelInstanceBoundsFix);

			const AActor* CDO = GetActorNativeClass()->GetDefaultObject<AActor>();
			const ILevelInstanceInterface* LevelInstanceCDO = CastChecked<ILevelInstanceInterface>(CDO);
			if (WorldAsset.IsValid() && (LevelInstanceCDO->IsLoadingEnabled() || bFixupOldVersion))
			{
				if (!IsChildContainerInstance())
				{
					FBox OutBounds;
					if (ULevelInstanceSubsystem::GetLevelInstanceBoundsFromPackage(ActorTransform, GetChildContainerPackage(), OutBounds))
					{
						OutBounds.GetCenterAndExtents(BoundsLocation, BoundsExtent);
					}
				}
			}
		}
	}

	if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) >= FFortniteMainBranchObjectVersion::WorldPartitionActorFilter)
	{
		if(Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) >= FFortniteMainBranchObjectVersion::LevelInstanceActorDescDeltaSerializeFilter)
		{
			Ar << TDeltaSerialize<FWorldPartitionActorFilter>(Filter);
		}
		else
		{
			Ar << Filter;
		}
	}
}
#endif
