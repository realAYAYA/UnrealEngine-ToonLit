// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Editor/View/DisplayUtils.h"

#include "ConcertLogGlobal.h"
#include "Replication/ObjectUtils.h"
#include "Replication/Editor/Model/IReplicationStreamModel.h"
#include "Replication/Editor/Model/Object/IObjectNameModel.h"

#include "Styling/SlateIconFinder.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/BlueprintSupport.h"
#include "GameFramework/Actor.h"
#endif

#if WITH_EDITORONLY_DATA
#include "UObject/UnrealType.h"
#endif

namespace UE::ConcertSharedSlate::DisplayUtils
{
	FText GetObjectDisplayText(const FSoftObjectPath& Object, IObjectNameModel* Model)
	{
		if (Model)
		{
			return Model->GetObjectDisplayName(Object);
		}
		
#if WITH_EDITOR
		// Important! The object may not be loaded, yet. So only resolve, do not TryLoad. This could be if the asset is using a level that was not opened.
		if (const UObject* LoadedObject = Object.ResolveObject())
		{
			return FText::FromString(GetObjectDisplayString(*LoadedObject));
		}
#endif

		return ExtractObjectDisplayTextFromPath(Object);
	}
	
	FText ExtractObjectDisplayTextFromPath(const FSoftObjectPath& Object)
	{
		return FText::FromString(ObjectUtils::ExtractObjectDisplayStringFromPath(Object));
	}

	FString GetObjectDisplayString(const UObject& Object)
	{
		// In editor builds we have access to actor label so display them (that's also what's displayed in the scene outliner)
#if WITH_EDITOR 
		if (const AActor* AsActor = Cast<AActor>(&Object))
		{
			return AsActor->GetActorLabel();
		}
#endif
		// If we're not in the editor (or the object is not an actor) then whatever the name is in the soft object path will be good enough.
		return Object.GetName();
	}

	FText GetObjectTypeText(const IReplicationStreamModel& Model, const FSoftObjectPath& Object)
	{
		const FSoftClassPath ClassPath = Model.GetObjectClass(Object);
#if WITH_EDITOR
		if (ClassPath.IsValid())
		{
			// For C++ classes or loaded Blueprints
			if (UClass* Class = ClassPath.ResolveClass())
			{
				return Class->GetDisplayNameText();
			}

			// Assuming it is a Blueprint, try to get its display name from the registry tags
			const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			const IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
			
			const FAssetData BlueprintData = AssetRegistry.GetAssetByObjectPath(ClassPath);
			if (BlueprintData.IsValid())
			{
				FString DisplayName;
				return BlueprintData.GetTagValue(FBlueprintTags::BlueprintDisplayName, DisplayName)
					? FText::FromString(DisplayName)
					: FText::GetEmpty();
			}

			// TODO: Check redirectors
			// It was not a Blueprint - the class was most likely removed
			UE_LOG(LogConcert, Error, TEXT("Failed to get display name for unknown class \"%s\""), *ClassPath.ToString());
		}
		
		return FText::FromName(NAME_None);
#else
		// On server the type info is not available. We can just display what it says in the path.
		return FText::FromString(ClassPath.GetAssetName());
#endif

	}

	FSlateIcon GetObjectIcon(const IReplicationStreamModel& Model, const FSoftObjectPath& Object)
	{
		if (const FSoftClassPath ClassPath = Model.GetObjectClass(Object); ClassPath.IsValid())
		{
			UClass* Class = ClassPath.ResolveClass();
			return Class
				? FSlateIconFinder::FindIconForClass(Class)
				: FSlateIcon();
		}
		return FSlateIcon();
	}

	FSlateIcon GetObjectIcon(UObject& Object)
	{
		return FSlateIconFinder::FindIconForClass(Object.GetClass());
	}
	
	FText GetPropertyDisplayText(const FConcertPropertyChain& Property, UStruct* Class /*=nullptr*/)
	{
#if WITH_EDITORONLY_DATA
		// Class will (likely) be valid on editor builds but null on server
		const FProperty* ResolvedProperty = Class ? Property.ResolveProperty(*Class) : nullptr;
		return ResolvedProperty
			? ResolvedProperty->GetDisplayNameText()
			:
#else
		return 
#endif
		FText::FromString(GetPropertyDisplayString(Property, Class));
    }
	
	FString GetPropertyDisplayString(const FConcertPropertyChain& Property, UStruct* Class /*=nullptr*/)
	{
#if WITH_EDITORONLY_DATA
		// Class will (likely) be valid on editor builds but null on server
		const FProperty* ResolvedProperty = Class ? Property.ResolveProperty(*Class) : nullptr;
		return ResolvedProperty
			? ResolvedProperty->GetDisplayNameText().ToString()
			:
#else
		return 
#endif
		Property.ToString(FConcertPropertyChain::EToStringMethod::LeafProperty);
	}
}
