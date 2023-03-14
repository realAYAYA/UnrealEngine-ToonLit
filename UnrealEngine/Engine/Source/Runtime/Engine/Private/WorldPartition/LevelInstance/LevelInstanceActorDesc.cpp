// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/LevelInstance/LevelInstanceActorDesc.h"

#if WITH_EDITOR
#include "Engine/World.h"
#include "Engine/Level.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/ActorDescContainer.h"
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationErrorHandler.h"
#include "UObject/UE5ReleaseStreamObjectVersion.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "Logging/MessageLog.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/MapErrors.h"
#include "Misc/UObjectToken.h"

static int32 GLevelInstanceDebugForceLevelStreaming = 0;
static FAutoConsoleVariableRef CVarForceLevelStreaming(
	TEXT("levelinstance.debug.forcelevelstreaming"),
	GLevelInstanceDebugForceLevelStreaming,
	TEXT("Set to 1 to force Level Instance to be streamed instead of embedded in World Partition grid."));

FLevelInstanceActorDesc::FLevelInstanceActorDesc()
	: DesiredRuntimeBehavior(ELevelInstanceRuntimeBehavior::Partitioned)
	, LevelInstanceContainer(nullptr)	
{}

FLevelInstanceActorDesc::~FLevelInstanceActorDesc()
{
	UnregisterContainerInstance();
	check(!LevelInstanceContainer.IsValid());
}

void FLevelInstanceActorDesc::Init(const AActor* InActor)
{
	FWorldPartitionActorDesc::Init(InActor);

	const ILevelInstanceInterface* LevelInstance = CastChecked<ILevelInstanceInterface>(InActor);
	LevelPackage = *LevelInstance->GetWorldAssetPackage();
	LevelInstanceTransform = InActor->GetActorTransform();
	DesiredRuntimeBehavior = LevelInstance->GetDesiredRuntimeBehavior();
}

void FLevelInstanceActorDesc::Init(const FWorldPartitionActorDescInitData& DescData)
{
	AActor* CDO = DescData.NativeClass->GetDefaultObject<AActor>();
	ILevelInstanceInterface* LevelInstanceCDO = CastChecked<ILevelInstanceInterface>(CDO);
	DesiredRuntimeBehavior = LevelInstanceCDO->GetDefaultRuntimeBehavior();

	FWorldPartitionActorDesc::Init(DescData);
}

bool FLevelInstanceActorDesc::Equals(const FWorldPartitionActorDesc* Other) const
{
	if (FWorldPartitionActorDesc::Equals(Other))
	{
		const FLevelInstanceActorDesc* LevelInstanceActorDesc = (FLevelInstanceActorDesc*)Other;

		return
			LevelPackage == LevelInstanceActorDesc->LevelPackage &&
			LevelInstanceTransform.Equals(LevelInstanceActorDesc->LevelInstanceTransform, 0.1f) &&
			DesiredRuntimeBehavior == LevelInstanceActorDesc->DesiredRuntimeBehavior;
	}

	return false;
}

void FLevelInstanceActorDesc::UpdateBounds()
{
	ULevelInstanceSubsystem* LevelInstanceSubsystem = UWorld::GetSubsystem<ULevelInstanceSubsystem>(LevelInstanceContainer->GetWorld());
	check(LevelInstanceSubsystem);

	FTransform LevelInstancePivotOffsetTransform = FTransform(ULevel::GetLevelInstancePivotOffsetFromPackage(LevelPackage));
	FTransform FinalLevelTransform = LevelInstancePivotOffsetTransform * LevelInstanceTransform;
	FBox ContainerBounds = LevelInstanceSubsystem->GetContainerBounds(LevelPackage).TransformBy(FinalLevelTransform);

	ContainerBounds.GetCenterAndExtents(BoundsLocation, BoundsExtent);
}

void FLevelInstanceActorDesc::RegisterContainerInstance(UWorld* InWorld)
{
	if (InWorld)
	{
		check(!LevelInstanceContainer.IsValid());

		if (IsContainerInstance())
		{
			ULevelInstanceSubsystem* LevelInstanceSubsystem = UWorld::GetSubsystem<ULevelInstanceSubsystem>(InWorld);
			check(LevelInstanceSubsystem);

			LevelInstanceContainer = LevelInstanceSubsystem->RegisterContainer(LevelPackage);
			check(LevelInstanceContainer.IsValid());

			// Should only be called on RegisterContainerInstance before ActorDesc is hashed
			UpdateBounds();
		}
	}
}

void FLevelInstanceActorDesc::UnregisterContainerInstance()
{
	if (LevelInstanceContainer.IsValid())
	{
		ULevelInstanceSubsystem* LevelInstanceSubsystem = UWorld::GetSubsystem<ULevelInstanceSubsystem>(LevelInstanceContainer->GetWorld());
		check(LevelInstanceSubsystem);

		LevelInstanceSubsystem->UnregisterContainer(LevelInstanceContainer.Get());
		LevelInstanceContainer.Reset();
	}
}

void FLevelInstanceActorDesc::SetContainer(UActorDescContainer* InContainer)
{
	FWorldPartitionActorDesc::SetContainer(InContainer);

	if (Container)
	{
		RegisterContainerInstance(Container->GetWorld());
	}
	else
	{
		UnregisterContainerInstance();		
	}
}

bool FLevelInstanceActorDesc::IsContainerInstance() const
{
	if (DesiredRuntimeBehavior != ELevelInstanceRuntimeBehavior::Partitioned)
	{
		return false;
	}
	
	if (GLevelInstanceDebugForceLevelStreaming)
	{
		return false;
	}

	if (LevelPackage.IsNone())
	{
		return false;
	}
	
	if (!ULevel::GetIsLevelUsingExternalActorsFromPackage(LevelPackage))
	{
		return false;
	}

	return ULevelInstanceSubsystem::CanUsePackage(LevelPackage);
}

bool FLevelInstanceActorDesc::GetContainerInstance(const UActorDescContainer*& OutLevelContainer, FTransform& OutLevelTransform, EContainerClusterMode& OutClusterMode) const
{
	if (LevelInstanceContainer.IsValid())
	{
		OutLevelContainer = LevelInstanceContainer.Get();
		OutClusterMode = EContainerClusterMode::Partitioned;

		// Apply level instance pivot offset
		FTransform LevelInstancePivotOffsetTransform = FTransform(ULevel::GetLevelInstancePivotOffsetFromPackage(LevelInstanceContainer->GetContainerPackage()));
		OutLevelTransform = LevelInstancePivotOffsetTransform * LevelInstanceTransform;

		return true;
	}

	return false;
}

void FLevelInstanceActorDesc::CheckForErrors(IStreamingGenerationErrorHandler* ErrorHandler) const
{
	FWorldPartitionActorDesc::CheckForErrors(ErrorHandler);

	FPackagePath WorldAssetPath;
	if (!FPackagePath::TryFromPackageName(LevelPackage, WorldAssetPath) || !FPackageName::DoesPackageExist(WorldAssetPath))
	{
		ErrorHandler->OnLevelInstanceInvalidWorldAsset(this, LevelPackage, IStreamingGenerationErrorHandler::ELevelInstanceInvalidReason::WorldAssetNotFound);
	}
	else if (!ULevel::GetIsLevelUsingExternalActorsFromPackage(LevelPackage))
	{
		if (DesiredRuntimeBehavior != ELevelInstanceRuntimeBehavior::LevelStreaming)
		{
			ErrorHandler->OnLevelInstanceInvalidWorldAsset(this, LevelPackage, IStreamingGenerationErrorHandler::ELevelInstanceInvalidReason::WorldAssetNotUsingExternalActors);
		}
	}
	else if (ULevel::GetIsLevelPartitionedFromPackage(LevelPackage))
	{
		if ((DesiredRuntimeBehavior != ELevelInstanceRuntimeBehavior::Partitioned) || !ULevel::GetPartitionedLevelCanBeUsedByLevelInstanceFromPackage(LevelPackage))
		{
			ErrorHandler->OnLevelInstanceInvalidWorldAsset(this, LevelPackage, IStreamingGenerationErrorHandler::ELevelInstanceInvalidReason::WorldAssetImcompatiblePartitioned);
		}
	}
}

void FLevelInstanceActorDesc::TransferFrom(const FWorldPartitionActorDesc* From)
{
	FWorldPartitionActorDesc::TransferFrom(From);

	FLevelInstanceActorDesc* FromLevelInstanceActorDesc = (FLevelInstanceActorDesc*)From;

	// Use the Register/Unregister so callbacks are added/removed
	if (FromLevelInstanceActorDesc->LevelInstanceContainer.IsValid())
	{
		RegisterContainerInstance(Container->World);
		FromLevelInstanceActorDesc->UnregisterContainerInstance();
	}
}

void FLevelInstanceActorDesc::Serialize(FArchive& Ar)
{
	FWorldPartitionActorDesc::Serialize(Ar);
	Ar.UsingCustomVersion(FUE5ReleaseStreamObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);

	Ar << LevelPackage;

	if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) < FUE5ReleaseStreamObjectVersion::LargeWorldCoordinates)
	{
		FTransform3f LevelInstanceTransformFlt;
		Ar << LevelInstanceTransformFlt;
		LevelInstanceTransform = FTransform(LevelInstanceTransformFlt);
	}
	else
	{
		Ar << LevelInstanceTransform;
	}

	if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) >= FUE5ReleaseStreamObjectVersion::LevelInstanceSerializeRuntimeBehavior)
	{
		Ar << DesiredRuntimeBehavior;

		if (Ar.IsLoading() && DesiredRuntimeBehavior == ELevelInstanceRuntimeBehavior::Embedded_Deprecated)
		{
			DesiredRuntimeBehavior = ELevelInstanceRuntimeBehavior::Partitioned;
		}
	}

	if (Ar.IsLoading())
	{
		const bool bFixupOldVersion = (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::PackedLevelInstanceBoundsFix) && 
									  (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) < FUE5ReleaseStreamObjectVersion::PackedLevelInstanceBoundsFix);

		const AActor* CDO = GetActorNativeClass()->GetDefaultObject<AActor>();
		const ILevelInstanceInterface* LevelInstanceCDO = CastChecked<ILevelInstanceInterface>(CDO);
		if (!LevelPackage.IsNone() && (LevelInstanceCDO->IsLoadingEnabled() || bFixupOldVersion))
		{
			if (!IsContainerInstance())
			{
				FBox OutBounds;
				if (ULevelInstanceSubsystem::GetLevelInstanceBoundsFromPackage(LevelInstanceTransform, LevelPackage, OutBounds))
				{
					OutBounds.GetCenterAndExtents(BoundsLocation, BoundsExtent);
				}
			}
		}
	}
}

#endif