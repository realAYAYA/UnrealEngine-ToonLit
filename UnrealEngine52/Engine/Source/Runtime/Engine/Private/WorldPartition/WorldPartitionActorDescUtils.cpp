// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionActorDescUtils.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "Modules/ModuleManager.h"
#include "WorldPartition/WorldPartitionLog.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/CoreRedirects.h"
#include "Misc/Base64.h"
#include "WorldPartition/WorldPartitionActorDesc.h"

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

bool FWorldPartitionActorDescUtils::IsValidActorDescriptorFromAssetData(const FAssetData& InAssetData)
{
	return InAssetData.FindTag(NAME_ActorMetaDataClass) && InAssetData.FindTag(NAME_ActorMetaData);
}

UClass* FWorldPartitionActorDescUtils::GetActorNativeClassFromAssetData(const FAssetData& InAssetData)
{
	FString ActorMetaDataClass;
	if (InAssetData.GetTagValue(NAME_ActorMetaDataClass, ActorMetaDataClass))
	{
		FString ActorClassName;
		FString ActorPackageName;
		if (!ActorMetaDataClass.Split(TEXT("."), &ActorPackageName, &ActorClassName))
		{
			ActorClassName = *ActorMetaDataClass;
		}

		// Look for a class redirectors
		const FCoreRedirectObjectName OldClassName = FCoreRedirectObjectName(*ActorClassName, NAME_None, *ActorPackageName);
		const FCoreRedirectObjectName NewClassName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Class, OldClassName);
		
		// Handle deprecated short class names
		const FTopLevelAssetPath ClassPath = FAssetData::TryConvertShortClassNameToPathName(*NewClassName.ToString(), ELogVerbosity::Log);

		// Lookup the native class
		return UClass::TryFindTypeSlow<UClass>(ClassPath.ToString(), EFindFirstObjectOptions::ExactClass);
	}
	return nullptr;
}

TUniquePtr<FWorldPartitionActorDesc> FWorldPartitionActorDescUtils::GetActorDescriptorFromAssetData(const FAssetData& InAssetData)
{
	if (IsValidActorDescriptorFromAssetData(InAssetData))
	{
		FWorldPartitionActorDescInitData ActorDescInitData;
		ActorDescInitData.NativeClass = GetActorNativeClassFromAssetData(InAssetData);
		ActorDescInitData.PackageName = InAssetData.PackageName;
		ActorDescInitData.ActorPath = InAssetData.GetSoftObjectPath();

		FString ActorMetaDataStr;
		verify(InAssetData.GetTagValue(NAME_ActorMetaData, ActorMetaDataStr));
		verify(FBase64::Decode(ActorMetaDataStr, ActorDescInitData.SerializedData));

		TUniquePtr<FWorldPartitionActorDesc> NewActorDesc(AActor::StaticCreateClassActorDesc(ActorDescInitData.NativeClass ? ActorDescInitData.NativeClass : AActor::StaticClass()));

		NewActorDesc->Init(ActorDescInitData);
			
		if (!ActorDescInitData.NativeClass)
		{
			UE_LOG(LogWorldPartition, Log, TEXT("Invalid class for actor guid `%s` ('%s') from package '%s'"), *NewActorDesc->GetGuid().ToString(), *NewActorDesc->GetActorName().ToString(), *NewActorDesc->GetActorPackage().ToString());
			NewActorDesc->NativeClass.Reset();
			return NewActorDesc;
		}
		/*else if (UClass* Class = FindObject<UClass>(InAssetData.AssetClassPath); !Class)
		{
			// We can't detect mising BP classes for inactive plugins, etc.
			if (InAssetData.AssetClassPath.GetPackageName().ToString().StartsWith(TEXT("/Game/")))
			{
				TArray<FAssetData> BlueprintClass;
				IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
				AssetRegistry.GetAssetsByPackageName(*InAssetData.AssetClassPath.GetPackageName().ToString(), BlueprintClass, true);

				if (!BlueprintClass.Num())
				{
					UE_LOG(LogWorldPartition, Log, TEXT("Failed to find class '%s' for actor '%s"), *InAssetData.AssetClassPath.ToString(), *NewActorDesc->GetActorSoftPath().ToString());
					return nullptr;
				}
			}
		}*/

		return NewActorDesc;
	}

	return nullptr;
}

void FWorldPartitionActorDescUtils::AppendAssetDataTagsFromActor(const AActor* InActor, TArray<UObject::FAssetRegistryTag>& OutTags)
{
	check(InActor->IsPackageExternal());
	
	TUniquePtr<FWorldPartitionActorDesc> ActorDesc(InActor->CreateActorDesc());

	// If the actor is not added to a world, we can't retrieve its bounding volume, so try to get the existing one
	if (ULevel* Level = InActor->GetLevel(); !Level || !Level->Actors.Contains(InActor))
	{
		// Avoid an assert when calling StaticFindObject during save to retrieve the actor's class.
		// Since we are only looking for a native class, the call to StaticFindObject is legit.
		TGuardValue<bool> GIsSavingPackageGuard(GIsSavingPackage, false);

		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

		FARFilter Filter;
		Filter.bIncludeOnlyOnDiskAssets = true;
		Filter.PackageNames.Add(InActor->GetPackage()->GetFName());

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

	const FString ActorMetaDataClass = GetParentNativeClass(InActor->GetClass())->GetPathName();
	OutTags.Add(UObject::FAssetRegistryTag(NAME_ActorMetaDataClass, ActorMetaDataClass, UObject::FAssetRegistryTag::TT_Hidden));

	const FString ActorMetaData = GetAssetDataFromActorDescriptor(ActorDesc);
	OutTags.Add(UObject::FAssetRegistryTag(NAME_ActorMetaData, ActorMetaData, UObject::FAssetRegistryTag::TT_Hidden));
}

FString FWorldPartitionActorDescUtils::GetAssetDataFromActorDescriptor(TUniquePtr<FWorldPartitionActorDesc>& InActorDesc)
{
	TArray<uint8> SerializedData;
	InActorDesc->SerializeTo(SerializedData);
	return FBase64::Encode(SerializedData);
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

void FWorldPartitionActorDescUtils::ReplaceActorDescriptorPointerFromActor(const AActor* InOldActor, AActor* InNewActor, FWorldPartitionActorDesc* InActorDesc)
{
	check(!InNewActor || (InOldActor->GetActorGuid() == InNewActor->GetActorGuid()));
	check(!InNewActor || (InNewActor->GetActorGuid() == InActorDesc->GetGuid()));
	check(!InActorDesc->ActorPtr.IsValid() || (InActorDesc->ActorPtr == InOldActor));
	InActorDesc->ActorPtr = InNewActor;
}
#endif
