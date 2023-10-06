// Copyright Epic Games, Inc. All Rights Reserved. 
#include "InterchangeGenericAssetsPipelineSharedSettings.h"

#include "Animation/Skeleton.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "InterchangePipelineLog.h"
#include "InterchangeSkeletonFactoryNode.h"
#include "InterchangeSkeletonHelper.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Nodes/InterchangeFactoryBaseNode.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

UInterchangeSkeletonFactoryNode* UInterchangeGenericCommonSkeletalMeshesAndAnimationsProperties::CreateSkeletonFactoryNode(UInterchangeBaseNodeContainer* BaseNodeContainer, const FString& RootJointUid)
{
	const UInterchangeBaseNode* RootJointNode = BaseNodeContainer->GetNode(RootJointUid);
	if (!RootJointNode)
	{
		return nullptr;
	}

	FString DisplayLabel = RootJointNode->GetDisplayLabel() + TEXT("_Skeleton");
	FString SkeletonUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(RootJointNode->GetUniqueID());

	UInterchangeSkeletonFactoryNode* SkeletonFactoryNode = nullptr;
	if (BaseNodeContainer->IsNodeUidValid(SkeletonUid))
	{
		//The node already exist, just return it
		SkeletonFactoryNode = Cast<UInterchangeSkeletonFactoryNode>(BaseNodeContainer->GetFactoryNode(SkeletonUid));
		if (!ensure(SkeletonFactoryNode))
		{
			//Log an error
			return nullptr;
		}
		FString ExistingSkeletonRootJointUid;
		SkeletonFactoryNode->GetCustomRootJointUid(ExistingSkeletonRootJointUid);
		if (!ensure(ExistingSkeletonRootJointUid.Equals(RootJointUid)))
		{
			//Log an error
			return nullptr;
		}
	}
	else
	{
		SkeletonFactoryNode = NewObject<UInterchangeSkeletonFactoryNode>(BaseNodeContainer, NAME_None);
		if (!ensure(SkeletonFactoryNode))
		{
			return nullptr;
		}
		SkeletonFactoryNode->InitializeSkeletonNode(SkeletonUid, DisplayLabel, USkeleton::StaticClass()->GetName());
		SkeletonFactoryNode->SetCustomRootJointUid(RootJointNode->GetUniqueID());
		SkeletonFactoryNode->SetCustomUseTimeZeroForBindPose(bUseT0AsRefPose);
		BaseNodeContainer->AddNode(SkeletonFactoryNode);
	}

	//If we have a specified skeleton
	if (Skeleton.IsValid())
	{
		SkeletonFactoryNode->SetEnabled(false);
		SkeletonFactoryNode->SetCustomReferenceObject(FSoftObjectPath(Skeleton.Get()));
	}
#if WITH_EDITOR
	//Iterate all joints to set the meta data value in the skeleton node
	UE::Interchange::Private::FSkeletonHelper::RecursiveAddSkeletonMetaDataValues(BaseNodeContainer, SkeletonFactoryNode, RootJointUid);
#endif //WITH_EDITOR

	return SkeletonFactoryNode;
}