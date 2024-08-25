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

UInterchangeFactoryBase::FImportAssetResult UInterchangePhysicsAssetFactory::BeginImportAsset_GameThread(const FImportAssetObjectParams& Arguments)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UInterchangePhysicsAssetFactory::BeginImportAsset_GameThread);
	FImportAssetResult ImportAssetResult;
	UObject* PhysicsAsset = nullptr;

#if WITH_EDITORONLY_DATA
	if (!Arguments.AssetNode || !Arguments.AssetNode->GetObjectClass()->IsChildOf(GetFactoryClass()))
	{
		return ImportAssetResult;
	}

	UInterchangePhysicsAssetFactoryNode* PhysicsAssetNode = Cast<UInterchangePhysicsAssetFactoryNode>(Arguments.AssetNode);
	if (PhysicsAssetNode == nullptr)
	{
		return ImportAssetResult;
	}

	UObject* ExistingAsset = Arguments.ReimportObject;
	if (!ExistingAsset)
	{
		FSoftObjectPath ReferenceObject;
		if (PhysicsAssetNode->GetCustomReferenceObject(ReferenceObject))
		{
			ExistingAsset = ReferenceObject.TryLoad();
		}
	}

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
		return ImportAssetResult;
	}
	PhysicsAssetNode->SetCustomReferenceObject(FSoftObjectPath(PhysicsAsset));

	PhysicsAsset->PreEditChange(nullptr);
#endif //WITH_EDITORONLY_DATA

	ImportAssetResult.ImportedObject = PhysicsAsset;
	return ImportAssetResult;
}

UInterchangeFactoryBase::FImportAssetResult UInterchangePhysicsAssetFactory::ImportAsset_Async(const FImportAssetObjectParams& Arguments)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UInterchangePhysicsAssetFactory::ImportAsset_Async);
	FImportAssetResult ImportAssetResult;
#if !WITH_EDITORONLY_DATA

	UE_LOG(LogInterchangeImport, Error, TEXT("Cannot import PhysicsAsset asset at runtime. This is an editor-only feature."));
	return ImportAssetResult;

#else

	if (!Arguments.AssetNode || !Arguments.AssetNode->GetObjectClass()->IsChildOf(GetFactoryClass()))
	{
		return ImportAssetResult;
	}

	UInterchangePhysicsAssetFactoryNode* PhysicsAssetNode = Cast<UInterchangePhysicsAssetFactoryNode>(Arguments.AssetNode);
	if (PhysicsAssetNode == nullptr)
	{
		return ImportAssetResult;
	}

	UObject* PhysicsAssetObject = UE::Interchange::FFactoryCommon::AsyncFindObject(PhysicsAssetNode, GetFactoryClass(), Arguments.Parent, Arguments.AssetName);

	if (!PhysicsAssetObject)
	{
		UE_LOG(LogInterchangeImport, Error, TEXT("Could not import the PhysicsAsset asset %s because the asset does not exist."), *Arguments.AssetName);
		return ImportAssetResult;
	}

	UPhysicsAsset* PhysicsAsset = Cast<UPhysicsAsset>(PhysicsAssetObject);
	if (!ensure(PhysicsAsset))
	{
		UE_LOG(LogInterchangeImport, Error, TEXT("Could not cast to PhysicsAsset asset %s."), *Arguments.AssetName);
		return ImportAssetResult;
	}

	//Currently PhysicsAsset re-import will not touch the PhysicsAsset at all
	//TODO design a re-import process for the PhysicsAsset
	if(!Arguments.ReimportObject)
	{
		PhysicsAssetNode->SetCustomReferenceObject(FSoftObjectPath(PhysicsAsset));
	}
		
	//Getting the file Hash will cache it into the source data
	Arguments.SourceData->GetFileContentHash();

	ImportAssetResult.ImportedObject = PhysicsAssetObject;
	return ImportAssetResult;
#endif
}

