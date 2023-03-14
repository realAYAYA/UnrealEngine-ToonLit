// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeDatasmithSceneFactory.h"

#include "InterchangeDatasmithLog.h"
#include "InterchangeDatasmithSceneFactoryNode.h"

#include "DatasmithScene.h"
#include "Logging/LogMacros.h"
#include "Nodes/InterchangeBaseNode.h"

UClass* UInterchangeDatasmithSceneFactory::GetFactoryClass() const
{
	return UDatasmithScene::StaticClass();
}


UObject* UInterchangeDatasmithSceneFactory::CreateEmptyAsset(const FCreateAssetParams& Arguments)
{
	UDatasmithScene* DatasmithScene = nullptr;

#if WITH_EDITORONLY_DATA
	if (!Arguments.AssetNode)
	{
		UE_LOG(LogInterchangeDatasmith, Error, TEXT("UInterchangeDatasmithSceneFactory: Asset node parameter is null."));
		return nullptr;
	}

	const UClass* DatasmithSceneClass = Arguments.AssetNode->GetObjectClass();
	if (!DatasmithSceneClass || !DatasmithSceneClass->IsChildOf(UDatasmithScene::StaticClass()))
	{
		UE_LOG(LogInterchangeDatasmith, Error, TEXT("UInterchangeDatasmithSceneFactory: Asset node parameter class doesnt derive from UDatasmithScene."));
		return nullptr;
	}

	// create an asset if it doesn't exist
	UObject* ExistingAsset = StaticFindObject(nullptr, Arguments.Parent, *Arguments.AssetName);

	// create a new texture or overwrite existing asset, if possible
	if (!ExistingAsset)
	{
		DatasmithScene = NewObject<UDatasmithScene>(Arguments.Parent, DatasmithSceneClass, *Arguments.AssetName, RF_Public | RF_Standalone);
	}
	else if (ExistingAsset->GetClass()->IsChildOf(DatasmithSceneClass))
	{
		//This is a reimport, we are just re-updating the source data
		DatasmithScene = static_cast<UDatasmithScene*>(ExistingAsset);
	}

	if (!DatasmithScene)
	{
		UE_LOG(LogInterchangeDatasmith, Warning, TEXT("Could not create Datasmith Scene asset %s"), *Arguments.AssetName);
		return nullptr;
	}
#endif //WITH_EDITORONLY_DATA

	return DatasmithScene;
}

UObject* UInterchangeDatasmithSceneFactory::CreateAsset(const FCreateAssetParams& Arguments)
{
 #if !WITH_EDITORONLY_DATA
 	UE_LOG(LogInterchangeDatasmith, Error, TEXT("Cannot import datasmith scene asset in runtime, this is an editor only feature."));
 	return nullptr; 
 #else //WITH_EDITORONLY_DATA

	TRACE_CPUPROFILER_EVENT_SCOPE(UInterchangeTextureFactory::CreateAsset);

	if (!Arguments.AssetNode)
	{
		UE_LOG(LogInterchangeDatasmith, Error, TEXT("UInterchangeDatasmithSceneFactory: Asset node parameter is null."));
		return nullptr;
	}

	const UClass* DatasmithSceneClass = Arguments.AssetNode->GetObjectClass();
	if (!DatasmithSceneClass || !DatasmithSceneClass->IsChildOf(UDatasmithScene::StaticClass()))
	{
		UE_LOG(LogInterchangeDatasmith, Error, TEXT("UInterchangeDatasmithSceneFactory: Asset node parameter class doesnt derive from UDatasmithScene."));
		return nullptr;
	}

	UClass* SupportedFactoryNodeClass = Arguments.AssetNode->GetClass() == UInterchangeDatasmithSceneFactoryNode::StaticClass()
		? Arguments.AssetNode->GetClass()
		: nullptr;
	if (SupportedFactoryNodeClass == nullptr)
	{
		UE_LOG(LogInterchangeDatasmith, Error, TEXT("UInterchangeDatasmithSceneFactory: Asset node parameter is not a UInterchangeDatasmithSceneFactoryNode."));
		return nullptr;
	}

	// create an asset if it doesn't exist
	UObject* ExistingAsset = StaticFindObject(nullptr, Arguments.Parent, *Arguments.AssetName);
	UDatasmithScene* DatasmithScene = nullptr;

	// create a new texture or overwrite existing asset, if possible
	if (!ExistingAsset)
	{
		//NewObject is not thread safe, the asset registry directory watcher tick on the main thread can trig before we finish initializing the UObject and will crash
		//The UObject should have been create by calling CreateEmptyAsset on the main thread.
		check(IsInGameThread());
		DatasmithScene = NewObject<UDatasmithScene>(Arguments.Parent, DatasmithSceneClass, *Arguments.AssetName, RF_Public | RF_Standalone);
	}
	else if (ExistingAsset->GetClass()->IsChildOf(DatasmithSceneClass))
	{
		//This is a reimport, we are just re-updating the source data
		DatasmithScene = static_cast<UDatasmithScene*>(ExistingAsset);
	}

	if (!DatasmithScene)
	{
		UE_LOG(LogInterchangeDatasmith, Error, TEXT("UInterchangeDatasmithSceneFactory: Could not create datasmith scene asset %s"), *Arguments.AssetName);
		return nullptr;
	}

	// TODO add InterchangeAssetImportData or DatasmithAssetImportData.
	// TODO link the created asset to the factory.

	return DatasmithScene;
#endif //WITH_EDITORONLY_DATA
}

void UInterchangeDatasmithSceneFactory::PreImportPreCompletedCallback(const FImportPreCompletedCallbackParams& Arguments)
{

}