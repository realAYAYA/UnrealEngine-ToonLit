// Copyright Epic Games, Inc. All Rights Reserved. 
#include "Mesh/InterchangePhysicsAssetFactory.h"

#include "InterchangeImportCommon.h"
#include "InterchangeImportLog.h"
#include "InterchangePhysicsAssetFactoryNode.h"
#include "InterchangeSourceData.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "PhysicsEngine/PhysicsAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangePhysicsAssetFactory)

UClass* UInterchangePhysicsAssetFactory::GetFactoryClass() const
{
	return UPhysicsAsset::StaticClass();
}

UObject* UInterchangePhysicsAssetFactory::CreateEmptyAsset(const FCreateAssetParams& Arguments)
{
	UObject* PhysicsAsset = nullptr;

#if WITH_EDITORONLY_DATA
	if (!Arguments.AssetNode || !Arguments.AssetNode->GetObjectClass()->IsChildOf(GetFactoryClass()))
	{
		return nullptr;
	}

	UInterchangePhysicsAssetFactoryNode* PhysicsAssetNode = Cast<UInterchangePhysicsAssetFactoryNode>(Arguments.AssetNode);
	if (PhysicsAssetNode == nullptr)
	{
		return nullptr;
	}

	// create an asset if it doesn't exist
	UObject* ExistingAsset = StaticFindObject(nullptr, Arguments.Parent, *Arguments.AssetName);

	// create a new PhysicsAsset or overwrite existing asset, if possible
	if (!ExistingAsset)
	{
		PhysicsAsset = NewObject<UObject>(Arguments.Parent, UPhysicsAsset::StaticClass(), *Arguments.AssetName, RF_Public | RF_Standalone);
	}
	else if (ExistingAsset->GetClass()->IsChildOf(UPhysicsAsset::StaticClass()))
	{
		//This is a reimport, we are just re-updating the source data
		PhysicsAsset = ExistingAsset;
	}

	if (!PhysicsAsset)
	{
		UE_LOG(LogInterchangeImport, Warning, TEXT("Could not create PhysicsAsset asset %s"), *Arguments.AssetName);
		return nullptr;
	}
	PhysicsAssetNode->SetCustomReferenceObject(FSoftObjectPath(PhysicsAsset));

	PhysicsAsset->PreEditChange(nullptr);
#endif //WITH_EDITORONLY_DATA
	return PhysicsAsset;
}

UObject* UInterchangePhysicsAssetFactory::CreateAsset(const FCreateAssetParams& Arguments)
{
#if !WITH_EDITORONLY_DATA

	UE_LOG(LogInterchangeImport, Error, TEXT("Cannot import PhysicsAsset asset in runtime, this is an editor only feature."));
	return nullptr;

#else

	if (!Arguments.AssetNode || !Arguments.AssetNode->GetObjectClass()->IsChildOf(GetFactoryClass()))
	{
		return nullptr;
	}

	UInterchangePhysicsAssetFactoryNode* PhysicsAssetNode = Cast<UInterchangePhysicsAssetFactoryNode>(Arguments.AssetNode);
	if (PhysicsAssetNode == nullptr)
	{
		return nullptr;
	}

	const UClass* PhysicsAssetClass = PhysicsAssetNode->GetObjectClass();
	check(PhysicsAssetClass && PhysicsAssetClass->IsChildOf(GetFactoryClass()));

	// create an asset if it doesn't exist
	UObject* ExistingAsset = StaticFindObject(nullptr, Arguments.Parent, *Arguments.AssetName);

	UObject* PhysicsAssetObject = nullptr;
	// create a new PhysicsAsset or overwrite existing asset, if possible
	if (!ExistingAsset)
	{
		//NewObject is not thread safe, the asset registry directory watcher tick on the main thread can trig before we finish initializing the UObject and will crash
		//The UObject should have been create by calling CreateEmptyAsset on the main thread.
		check(IsInGameThread());
		PhysicsAssetObject = NewObject<UObject>(Arguments.Parent, PhysicsAssetClass, *Arguments.AssetName, RF_Public | RF_Standalone);
	}
	else if(ExistingAsset->GetClass()->IsChildOf(PhysicsAssetClass))
	{
		//This is a reimport, we are just re-updating the source data
		PhysicsAssetObject = ExistingAsset;
	}

	if (!PhysicsAssetObject)
	{
		UE_LOG(LogInterchangeImport, Warning, TEXT("Could not create PhysicsAsset asset %s"), *Arguments.AssetName);
		return nullptr;
	}

	if (PhysicsAssetObject)
	{
		//Currently PhysicsAsset re-import will not touch the PhysicsAsset at all
		//TODO design a re-import process for the PhysicsAsset
		if(!Arguments.ReimportObject)
		{
			UPhysicsAsset* PhysicsAsset = Cast<UPhysicsAsset>(PhysicsAssetObject);
			if (!ensure(PhysicsAsset))
			{
				UE_LOG(LogInterchangeImport, Warning, TEXT("Could not create PhysicsAsset asset %s"), *Arguments.AssetName);
				return nullptr;
			}
			
			PhysicsAssetNode->SetCustomReferenceObject(FSoftObjectPath(PhysicsAsset));
		}
		
		//Getting the file Hash will cache it into the source data
		Arguments.SourceData->GetFileContentHash();

		//The interchange completion task (call in the GameThread after the factories pass), will call PostEditChange.
	}
	else
	{
		//The PhysicsAsset is not a UPhysicsAsset
		PhysicsAssetObject->RemoveFromRoot();
		PhysicsAssetObject->MarkAsGarbage();
	}
	return PhysicsAssetObject;
#endif
}

