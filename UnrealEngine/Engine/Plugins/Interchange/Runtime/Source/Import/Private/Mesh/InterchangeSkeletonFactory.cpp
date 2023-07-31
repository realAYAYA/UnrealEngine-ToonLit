// Copyright Epic Games, Inc. All Rights Reserved. 
#include "Mesh/InterchangeSkeletonFactory.h"

#include "Animation/Skeleton.h"
#include "InterchangeImportCommon.h"
#include "InterchangeImportLog.h"
#include "InterchangeSkeletonFactoryNode.h"
#include "InterchangeSourceData.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Rendering/SkeletalMeshLODImporterData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeSkeletonFactory)

UClass* UInterchangeSkeletonFactory::GetFactoryClass() const
{
	return USkeleton::StaticClass();
}

UObject* UInterchangeSkeletonFactory::CreateEmptyAsset(const FCreateAssetParams& Arguments)
{
	UObject* Skeleton = nullptr;

#if WITH_EDITORONLY_DATA
	if (!Arguments.AssetNode || !Arguments.AssetNode->GetObjectClass()->IsChildOf(GetFactoryClass()))
	{
		return nullptr;
	}

	UInterchangeSkeletonFactoryNode* SkeletonNode = Cast<UInterchangeSkeletonFactoryNode>(Arguments.AssetNode);
	if (SkeletonNode == nullptr)
	{
		return nullptr;
	}

	// create an asset if it doesn't exist
	UObject* ExistingAsset = StaticFindObject(nullptr, Arguments.Parent, *Arguments.AssetName);

	// create a new skeleton or overwrite existing asset, if possible
	if (!ExistingAsset)
	{
		Skeleton = NewObject<UObject>(Arguments.Parent, USkeleton::StaticClass(), *Arguments.AssetName, RF_Public | RF_Standalone);
	}
	else if (ExistingAsset->GetClass()->IsChildOf(USkeleton::StaticClass()))
	{
		//This is a reimport, we are just re-updating the source data
		Skeleton = ExistingAsset;
	}

	if (!Skeleton)
	{
		UE_LOG(LogInterchangeImport, Warning, TEXT("Could not create Skeleton asset %s"), *Arguments.AssetName);
		return nullptr;
	}
	SkeletonNode->SetCustomReferenceObject(FSoftObjectPath(Skeleton));

	Skeleton->PreEditChange(nullptr);
#endif //WITH_EDITORONLY_DATA
	return Skeleton;
}

UObject* UInterchangeSkeletonFactory::CreateAsset(const FCreateAssetParams& Arguments)
{
#if !WITH_EDITORONLY_DATA

	UE_LOG(LogInterchangeImport, Error, TEXT("Cannot import Skeleton asset in runtime, this is an editor only feature."));
	return nullptr;

#else

	if (!Arguments.AssetNode || !Arguments.AssetNode->GetObjectClass()->IsChildOf(GetFactoryClass()))
	{
		return nullptr;
	}

	UInterchangeSkeletonFactoryNode* SkeletonNode = Cast<UInterchangeSkeletonFactoryNode>(Arguments.AssetNode);
	if (SkeletonNode == nullptr)
	{
		return nullptr;
	}

	const UClass* SkeletonClass = SkeletonNode->GetObjectClass();
	check(SkeletonClass && SkeletonClass->IsChildOf(GetFactoryClass()));

	// create an asset if it doesn't exist
	UObject* ExistingAsset = StaticFindObject(nullptr, Arguments.Parent, *Arguments.AssetName);
	
	bool bIsReImport = Arguments.ReimportObject != nullptr;
	
	UObject* SkeletonObject = nullptr;
	// create a new skeleton or overwrite existing asset, if possible
	if (!ExistingAsset)
	{
		//NewObject is not thread safe, the asset registry directory watcher tick on the main thread can trig before we finish initializing the UObject and will crash
		//The UObject should have been create by calling CreateEmptyAsset on the main thread.
		check(IsInGameThread());
		//We should not do a NewObject if we are doing a reimport
		check(!bIsReImport);
		SkeletonObject = NewObject<UObject>(Arguments.Parent, SkeletonClass, *Arguments.AssetName, RF_Public | RF_Standalone);
	}
	else if(ExistingAsset->GetClass()->IsChildOf(SkeletonClass))
	{
		//This is a reimport, we are just re-updating the source data
		SkeletonObject = ExistingAsset;
	}

	if (!SkeletonObject)
	{
		UE_LOG(LogInterchangeImport, Warning, TEXT("Could not create Skeleton asset %s"), *Arguments.AssetName);
		return nullptr;
	}

	if (SkeletonObject)
	{
		USkeleton* Skeleton = Cast<USkeleton>(SkeletonObject);
		if (!ensure(Skeleton))
		{
			UE_LOG(LogInterchangeImport, Warning, TEXT("Could not create Skeleton asset %s"), *Arguments.AssetName);
			return nullptr;
		}
		FString RootJointUid;
		if (!SkeletonNode->GetCustomRootJointUid(RootJointUid))
		{
			UE_LOG(LogInterchangeImport, Warning, TEXT("Could not create Skeleton asset %s, because there is no valid root joint node"), *Arguments.AssetName);
			return nullptr;
		}
		if (bIsReImport)
		{
			if (const UInterchangeBaseNode* RootJointNode = Arguments.NodeContainer->GetNode(RootJointUid))
			{
				FName RootJointName = FName(*RootJointNode->GetDisplayLabel());
				if (RootJointName != Skeleton->GetReferenceSkeleton().GetBoneName(0))
				{
					return nullptr;
				}
			}
		}
		//The joint will be added by the skeletalmesh factory since we need a valid skeletalmesh to add joint to a skeleton
		SkeletonNode->SetCustomReferenceObject(FSoftObjectPath(Skeleton));
		
		//Getting the file Hash will cache it into the source data
		Arguments.SourceData->GetFileContentHash();

		//The interchange completion task (call in the GameThread after the factories pass), will call PostEditChange which will trig another asynchronous system that will build all skeleton in parallel
	}
	else
	{
		//The skeleton is not a USkeleton
		SkeletonObject->RemoveFromRoot();
		SkeletonObject->MarkAsGarbage();
	}
	return SkeletonObject;
#endif
}

