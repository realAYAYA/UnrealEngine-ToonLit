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


UInterchangeFactoryBase::FImportAssetResult UInterchangeDatasmithSceneFactory::BeginImportAsset_GameThread(const FImportAssetObjectParams& Arguments)
{
	FImportAssetResult ImportAssetResult;
	UDatasmithScene* DatasmithScene = nullptr;

#if WITH_EDITORONLY_DATA
	if (!Arguments.AssetNode)
	{
		UE_LOG(LogInterchangeDatasmith, Error, TEXT("UInterchangeDatasmithSceneFactory: Asset node parameter is null."));
		return ImportAssetResult;
	}

	const UClass* DatasmithSceneClass = Arguments.AssetNode->GetObjectClass();
	if (!DatasmithSceneClass || !DatasmithSceneClass->IsChildOf(UDatasmithScene::StaticClass()))
	{
		UE_LOG(LogInterchangeDatasmith, Error, TEXT("UInterchangeDatasmithSceneFactory: Asset node parameter class doesnt derive from UDatasmithScene."));
		return ImportAssetResult;
	}

	UObject* ExistingAsset = Arguments.ReimportObject;
	if (!ExistingAsset)
	{
		FSoftObjectPath ReferenceObject;
		if (Arguments.AssetNode->GetCustomReferenceObject(ReferenceObject))
		{
			ExistingAsset = ReferenceObject.TryLoad();
		}
	}

	// create a new texture or overwrite existing asset, if possible
	if (!ExistingAsset)
	{
		DatasmithScene = NewObject<UDatasmithScene>(Arguments.Parent, DatasmithSceneClass, *Arguments.AssetName, RF_Public | RF_Standalone);
	}
	else
	{
		//This is a reimport, we are just re-updating the source data
		DatasmithScene = static_cast<UDatasmithScene*>(ExistingAsset);
	}

	if (!DatasmithScene)
	{
		UE_LOG(LogInterchangeDatasmith, Warning, TEXT("Could not create Datasmith Scene asset %s"), *Arguments.AssetName);
		return ImportAssetResult;
	}
#endif //WITH_EDITORONLY_DATA

	ImportAssetResult.ImportedObject = DatasmithScene;
	return ImportAssetResult;
}

UInterchangeFactoryBase::FImportAssetResult UInterchangeDatasmithSceneFactory::ImportAsset_Async(const FImportAssetObjectParams& Arguments)
{
	FImportAssetResult ImportAssetResult;

 #if !WITH_EDITORONLY_DATA
 	UE_LOG(LogInterchangeDatasmith, Error, TEXT("Cannot import datasmith scene asset in runtime, this is an editor only feature."));
 	return ImportAssetResult;
 #else //WITH_EDITORONLY_DATA

	TRACE_CPUPROFILER_EVENT_SCOPE(UInterchangeTextureFactory::CreateAsset);

	if (!Arguments.AssetNode)
	{
		UE_LOG(LogInterchangeDatasmith, Error, TEXT("UInterchangeDatasmithSceneFactory: Asset node parameter is null."));
		return ImportAssetResult;
	}

	const UClass* DatasmithSceneClass = Arguments.AssetNode->GetObjectClass();
	if (!DatasmithSceneClass || !DatasmithSceneClass->IsChildOf(UDatasmithScene::StaticClass()))
	{
		UE_LOG(LogInterchangeDatasmith, Error, TEXT("UInterchangeDatasmithSceneFactory: Asset node parameter class doesnt derive from UDatasmithScene."));
		return ImportAssetResult;
	}

	UClass* SupportedFactoryNodeClass = Arguments.AssetNode->GetClass() == UInterchangeDatasmithSceneFactoryNode::StaticClass()
		? Arguments.AssetNode->GetClass()
		: nullptr;
	if (SupportedFactoryNodeClass == nullptr)
	{
		UE_LOG(LogInterchangeDatasmith, Error, TEXT("UInterchangeDatasmithSceneFactory: Asset node parameter is not a UInterchangeDatasmithSceneFactoryNode."));
		return ImportAssetResult;
	}

	UObject* ExistingAsset = nullptr;
	FSoftObjectPath ReferenceObject;
	if (Arguments.AssetNode->GetCustomReferenceObject(ReferenceObject))
	{
		ExistingAsset = ReferenceObject.TryLoad();
	}
	UDatasmithScene* DatasmithScene = nullptr;

	if (ExistingAsset && ExistingAsset->GetClass()->IsChildOf(DatasmithSceneClass))
	{
		//This is a reimport, we are just re-updating the source data
		DatasmithScene = static_cast<UDatasmithScene*>(ExistingAsset);
	}

	if (!DatasmithScene)
	{
		UE_LOG(LogInterchangeDatasmith, Error, TEXT("UInterchangeDatasmithSceneFactory: Could not create datasmith scene asset %s"), *Arguments.AssetName);
		return ImportAssetResult;
	}

	// TODO add InterchangeAssetImportData or DatasmithAssetImportData.
	// TODO link the created asset to the factory.

	ImportAssetResult.ImportedObject = DatasmithScene;
	return ImportAssetResult;
#endif //WITH_EDITORONLY_DATA
}

void UInterchangeDatasmithSceneFactory::SetupObject_GameThread(const FSetupObjectParams& Arguments)
{

}