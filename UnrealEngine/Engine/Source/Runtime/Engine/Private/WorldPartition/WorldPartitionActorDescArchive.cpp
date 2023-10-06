// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionActorDescArchive.h"
#include "WorldPartition/WorldPartitionClassDescRegistry.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionLog.h"
#include "Misc/RedirectCollector.h"
#include "UObject/CoreRedirects.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "UObject/FortniteSeasonBranchObjectVersion.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "AssetRegistry/AssetData.h"

FActorDescArchive::FActorDescArchive(FArchive& InArchive, FWorldPartitionActorDesc* InActorDesc)
	: FArchiveProxy(InArchive)
	, ActorDesc(InActorDesc)
	, bIsMissingClassDesc(false)
{
	check(InArchive.IsPersistent());

	SetIsPersistent(true);
	SetIsLoading(InArchive.IsLoading());

	UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);
	UsingCustomVersion(FFortniteSeasonBranchObjectVersion::GUID);
	UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);

	if (CustomVer(FFortniteMainBranchObjectVersion::GUID) >= FFortniteMainBranchObjectVersion::WorldPartitionActorClassDescSerialize)
	{
		InnerArchive << InActorDesc->bIsDefaultActorDesc;
	}

	if (CustomVer(FFortniteSeasonBranchObjectVersion::GUID) >= FFortniteSeasonBranchObjectVersion::WorldPartitionActorDescNativeBaseClassSerialization)
	{
		if (CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::WorldPartitionActorDescActorAndClassPaths)
		{
			FName BaseClassPathName;
			InnerArchive << BaseClassPathName;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			InActorDesc->BaseClass = FAssetData::TryConvertShortClassNameToPathName(BaseClassPathName);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
		else
		{
			InnerArchive << InActorDesc->BaseClass;
		}
	}

	if (CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::WorldPartitionActorDescActorAndClassPaths)
	{
		FName NativeClassPathName;
		InnerArchive << NativeClassPathName;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		InActorDesc->NativeClass = FAssetData::TryConvertShortClassNameToPathName(NativeClassPathName);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	else
	{
		InnerArchive << InActorDesc->NativeClass;
	}

	if (IsLoading())
	{
		auto TryRedirectClass = [](FTopLevelAssetPath& InOutClassPath)
		{
			if (InOutClassPath.IsValid())
			{
				FCoreRedirectObjectName ClassRedirect(InOutClassPath);
				FCoreRedirectObjectName RedirectedClassRedirect = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Class, ClassRedirect);

				if (ClassRedirect != RedirectedClassRedirect)
				{
					InOutClassPath = FTopLevelAssetPath(RedirectedClassRedirect.ToString());
				}

				FSoftObjectPath FoundRedirection = GRedirectCollector.GetAssetPathRedirection(FSoftObjectPath(InOutClassPath.ToString()));
				if (FoundRedirection.IsValid())
				{
					InOutClassPath = FoundRedirection.GetAssetPath();
				}
			}
		};

		TryRedirectClass(InActorDesc->NativeClass);
		TryRedirectClass(InActorDesc->BaseClass);
	}

	// Get the class descriptor to do delta serialization
	FWorldPartitionClassDescRegistry& ClassDescRegistry = FWorldPartitionClassDescRegistry::Get();
	const FTopLevelAssetPath ClassPath = InActorDesc->BaseClass.IsValid() ? InActorDesc->BaseClass : InActorDesc->NativeClass;
	ClassDesc = InActorDesc->bIsDefaultActorDesc ? ClassDescRegistry.GetClassDescDefaultForClass(ClassPath) : ClassDescRegistry.GetClassDescDefaultForActor(ClassPath);
	if (!ClassDesc)
	{
		if (IsLoading())
		{
			bIsMissingClassDesc = true;

			ClassDesc = ClassDescRegistry.GetClassDescDefault(FTopLevelAssetPath(TEXT("/Script/Engine.Actor")));
			check(ClassDesc);

			UE_LOG(LogWorldPartition, Log, TEXT("Can't find class descriptor '%s' for loading '%s', using '%s'"), *ClassPath.ToString(), *InActorDesc->GetActorSoftPath().ToString(), *ClassDesc->GetActorSoftPath().ToString());
		}
		else
		{
			UE_LOG(LogWorldPartition, Log, TEXT("Can't find class descriptor '%s' for saving '%s'"), *ClassPath.ToString(), *InActorDesc->GetActorSoftPath().ToString());
		}
	}

	ClassDescSizeof = ClassDesc ? ClassDesc->GetSizeOf() : 0;
}

FArchive& FActorDescArchive::operator<<(FSoftObjectPath& Value)
{
	Value.SerializePathWithoutFixup(*this);

	if (IsLoading())
	{
		FWorldPartitionHelpers::FixupRedirectedAssetPath(Value);
	}

	return *this;
}
#endif