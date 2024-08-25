// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionActorDescUtils.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"
#include "WorldPartition/WorldPartitionLog.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/CoreRedirects.h"
#include "Misc/Base64.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/ActorDescContainerInstance.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"

static FName NAME_ActorMetaDataClass(TEXT("ActorMetaDataClass"));
static FName NAME_ActorMetaData(TEXT("ActorMetaData"));

FName FWorldPartitionActorDescUtils::ActorMetaDataClassTagName()
{
	return NAME_ActorMetaDataClass;
}

FName FWorldPartitionActorDescUtils::ActorMetaDataTagName()
{
	return NAME_ActorMetaData;
}

static FString ResolveClassRedirector(const FString& InClassName)
{
	FString ClassName;
	FString ClassPackageName;
	if (!InClassName.Split(TEXT("."), &ClassPackageName, &ClassName))
	{
		ClassName = *InClassName;
	}

	// Look for a class redirectors
	const FCoreRedirectObjectName OldClassName = FCoreRedirectObjectName(*ClassName, NAME_None, *ClassPackageName);
	const FCoreRedirectObjectName NewClassName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Class, OldClassName);

	return NewClassName.ToString();
}

bool FWorldPartitionActorDescUtils::IsValidActorDescriptorFromAssetData(const FAssetData& InAssetData)
{
	return InAssetData.FindTag(NAME_ActorMetaDataClass) && InAssetData.FindTag(NAME_ActorMetaData);
}

UClass* FWorldPartitionActorDescUtils::GetActorNativeClassFromAssetData(const FAssetData& InAssetData)
{
	FString ActorMetaDataClass;
	if (InAssetData.GetTagValue(NAME_ActorMetaDataClass, ActorMetaDataClass))
	{
		// Avoid an assert when calling StaticFindObject during save to retrieve the actor's class.
		// Since we are only looking for a native class, the call to StaticFindObject is legit.
		TGuardValue<bool> GIsSavingPackageGuard(GIsSavingPackage, false);

		// Look for a class redirectors
		const FString ActorNativeClassName = ResolveClassRedirector(ActorMetaDataClass);
		
		// Handle deprecated short class names
		const FTopLevelAssetPath ClassPath = FAssetData::TryConvertShortClassNameToPathName(*ActorNativeClassName, ELogVerbosity::Log);

		// Lookup the native class
		return UClass::TryFindTypeSlow<UClass>(ClassPath.ToString(), EFindFirstObjectOptions::ExactClass);
	}
	return nullptr;
}

TUniquePtr<FWorldPartitionActorDesc> FWorldPartitionActorDescUtils::GetActorDescriptorFromAssetData(const FAssetData& InAssetData)
{
	if (IsValidActorDescriptorFromAssetData(InAssetData))
	{
		// Fixing here for now until following jira tasks are closed to unblock users that are renaming plugins:
		// @todo_ow: remove once https://jira.it.epicgames.com/browse/UE-168245 & https://jira.it.epicgames.com/browse/UE-144431 are closed
		FSoftObjectPath ActorPath = InAssetData.GetSoftObjectPath();
		ActorPath.FixupCoreRedirects();

		FWorldPartitionActorDescInitData ActorDescInitData = FWorldPartitionActorDescInitData()
			.SetNativeClass(GetActorNativeClassFromAssetData(InAssetData))
			.SetPackageName(InAssetData.PackageName)
			.SetActorPath(ActorPath);

		FString ActorMetaDataStr;
		verify(InAssetData.GetTagValue(NAME_ActorMetaData, ActorMetaDataStr));
		verify(FBase64::Decode(ActorMetaDataStr, ActorDescInitData.SerializedData));

		TUniquePtr<FWorldPartitionActorDesc> NewActorDesc(AActor::StaticCreateClassActorDesc(ActorDescInitData.NativeClass ? ActorDescInitData.NativeClass : AActor::StaticClass()));

		NewActorDesc->Init(ActorDescInitData);
			
		if (!ActorDescInitData.NativeClass)
		{
			UE_LOG(LogWorldPartition, Warning, TEXT("Invalid class for actor guid `%s` ('%s') from package '%s'"), *NewActorDesc->GetGuid().ToString(), *NewActorDesc->GetActorName().ToString(), *NewActorDesc->GetActorPackage().ToString());
			NewActorDesc->NativeClass.Reset();
		}

		return NewActorDesc;
	}

	return nullptr;
}

void FWorldPartitionActorDescUtils::AppendAssetDataTagsFromActor(const AActor* InActor, TArray<UObject::FAssetRegistryTag>& OutTags)
{
	FAssetRegistryTagsContextData Context(InActor, EAssetRegistryTagsCaller::Uncategorized);
	AppendAssetDataTagsFromActor(InActor, Context);
	for (TPair<FName, UObject::FAssetRegistryTag>& Pair : Context.Tags)
	{
		OutTags.Add(MoveTemp(Pair.Value));
	}
}

void FWorldPartitionActorDescUtils::AppendAssetDataTagsFromActor(const AActor* InActor, FAssetRegistryTagsContext Context)
{
	// Avoid an assert when calling StaticFindObject during save to retrieve the actor's class.
	// Since we are only looking for a native class, the call to StaticFindObject is legit.
	TGuardValue<bool> GIsSavingPackageGuard(GIsSavingPackage, false);

	TUniquePtr<FWorldPartitionActorDesc> ActorDesc(InActor->CreateActorDesc());

	if (!InActor->HasAnyFlags(RF_ArchetypeObject | RF_ClassDefaultObject))
	{
		// If the actor is not added to a world, we can't retrieve its bounding volume, so try to get the existing one
		if (ULevel* Level = InActor->GetLevel(); !Level || !Level->Actors.Contains(InActor))
		{
			IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

			FARFilter Filter;
			Filter.bIncludeOnlyOnDiskAssets = true;
			Filter.PackageNames.Add(InActor->GetPackage()->GetLoadedPath().GetPackageFName());

			TArray<FAssetData> Assets;
			AssetRegistry.GetAssets(Filter, Assets);

			if (Assets.Num() == 1)
			{
				if (TUniquePtr<FWorldPartitionActorDesc> NewActorDesc = FWorldPartitionActorDescUtils::GetActorDescriptorFromAssetData(Assets[0]))
				{
					ActorDesc->TransferWorldData(NewActorDesc.Get());
				}
			}
		}
	}

	const FString ActorMetaDataClass = GetParentNativeClass(InActor->GetClass())->GetPathName();
	Context.AddTag(UObject::FAssetRegistryTag(NAME_ActorMetaDataClass, ActorMetaDataClass, UObject::FAssetRegistryTag::TT_Hidden));

	const FString ActorMetaData = GetAssetDataFromActorDescriptor(ActorDesc);
	Context.AddTag(UObject::FAssetRegistryTag(NAME_ActorMetaData, ActorMetaData, UObject::FAssetRegistryTag::TT_Hidden));
}

FString FWorldPartitionActorDescUtils::GetAssetDataFromActorDescriptor(TUniquePtr<FWorldPartitionActorDesc>& InActorDesc)
{
	TArray<uint8> SerializedData;
	InActorDesc->SerializeTo(SerializedData);
	return FBase64::Encode(SerializedData);
}

bool FWorldPartitionActorDescUtils::GetPatchedAssetDataFromAssetData(const FAssetData& InAssetData, FString& OutAssetData, FWorldPartitionAssetDataPatcher* InAssetDataPatcher)
{
	if (IsValidActorDescriptorFromAssetData(InAssetData))
	{
		FWorldPartitionActorDescInitData ActorDescInitData = FWorldPartitionActorDescInitData()
			.SetNativeClass(GetActorNativeClassFromAssetData(InAssetData))
			.SetPackageName(InAssetData.PackageName)
			.SetActorPath(InAssetData.GetSoftObjectPath());

		FString ActorMetaDataStr;
		verify(InAssetData.GetTagValue(NAME_ActorMetaData, ActorMetaDataStr));
		verify(FBase64::Decode(ActorMetaDataStr, ActorDescInitData.SerializedData));

		TArray<uint8> PatchedData;
		FWorldPartitionActorDesc::Patch(ActorDescInitData, PatchedData, InAssetDataPatcher);
		OutAssetData = FBase64::Encode(PatchedData);

		return OutAssetData != ActorMetaDataStr;
	}

	return false;
}

void FWorldPartitionActorDescUtils::UpdateActorDescriptorFromActor(const AActor* InActor, TUniquePtr<FWorldPartitionActorDesc>& OutActorDesc)
{
	TUniquePtr<FWorldPartitionActorDesc> NewActorDesc(InActor->CreateActorDesc());
	UpdateActorDescriptorFromActorDescriptor(NewActorDesc, OutActorDesc);
}

void FWorldPartitionActorDescUtils::UpdateActorDescriptorFromActorDescriptor(TUniquePtr<FWorldPartitionActorDesc>& InActorDesc, TUniquePtr<FWorldPartitionActorDesc>& OutActorDesc)
{
	InActorDesc->TransferFrom(OutActorDesc.Get());
	OutActorDesc = MoveTemp(InActorDesc);
}

void FWorldPartitionActorDescUtils::ReplaceActorDescriptorPointerFromActor(const AActor* InOldActor, AActor* InNewActor, FWorldPartitionActorDescInstance* InActorDescInstance)
{
	if (InNewActor)
	{
		checkf(InOldActor->GetActorGuid() == InNewActor->GetActorGuid(), TEXT("Mismatching new actor GUID: old=%s new=%s"), *InOldActor->GetActorGuid().ToString(), *InNewActor->GetActorGuid().ToString());
		checkf(InNewActor->GetActorGuid() == InActorDescInstance->GetGuid(), TEXT("Mismatching desc actor GUID: desc=%s new=%s"), *InActorDescInstance->GetGuid().ToString(), *InNewActor->GetActorGuid().ToString());
	}

	if (InActorDescInstance->ActorPtr.IsValid())
	{
		checkf(InActorDescInstance->ActorPtr == InOldActor, TEXT("Mismatching old desc actor: desc=%s old=%s"), *InActorDescInstance->ActorPtr->GetActorNameOrLabel(), *InOldActor->GetActorNameOrLabel());
	}

	InActorDescInstance->ActorPtr = InNewActor;
}

bool FWorldPartitionActorDescUtils::FixupRedirectedAssetPath(FName& InOutAssetPath)
{
	UAssetRegistryHelpers::FixupRedirectedAssetPath(InOutAssetPath);
	return true;
}
#endif
