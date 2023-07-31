// Copyright Epic Games, Inc. All Rights Reserved. 
#include "InterchangeSkeletonHelper.h"

#if WITH_EDITOR
#include "Animation/Skeleton.h"
#include "InterchangeCommonPipelineDataFactoryNode.h"
#include "InterchangeImportLog.h"
#include "InterchangeSceneNode.h"
#include "ReferenceSkeleton.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Nodes/InterchangeUserDefinedAttribute.h"

namespace UE::Interchange::Private
{

	bool FSkeletonHelper::ProcessImportMeshSkeleton(const USkeleton* SkeletonAsset, FReferenceSkeleton& RefSkeleton, int32& SkeletalDepth, const UInterchangeBaseNodeContainer* NodeContainer, const FString& RootJointNodeId, TArray<SkeletalMeshImportData::FBone>& RefBonesBinary, const bool bUseTimeZeroAsBindPose, bool& bOutDiffPose)
	{
		auto FixupBoneName = [](FString BoneName)
		{
			BoneName.TrimStartAndEndInline();
			BoneName.ReplaceInline(TEXT(" "), TEXT("-"), ESearchCase::IgnoreCase);
			return BoneName;
		};

		RefBonesBinary.Empty();
		// Setup skeletal hierarchy + names structure.
		RefSkeleton.Empty();

		FReferenceSkeletonModifier RefSkelModifier(RefSkeleton, SkeletonAsset);
		TArray <FJointInfo> JointInfos;
		RecursiveAddBones(NodeContainer, RootJointNodeId, JointInfos, INDEX_NONE, RefBonesBinary, bUseTimeZeroAsBindPose, bOutDiffPose);
		if (bOutDiffPose)
		{
			//bOutDiffPose can only be true if the user ask to bind with time zero transform.
			ensure(bUseTimeZeroAsBindPose);
		}
		// Digest bones to the serializable format.
		for (int32 b = 0; b < JointInfos.Num(); b++)
		{
			const FJointInfo& BinaryBone = JointInfos[b];

			const FString BoneName = FixupBoneName(BinaryBone.Name);
			const FMeshBoneInfo BoneInfo(FName(*BoneName, FNAME_Add), BinaryBone.Name, BinaryBone.ParentIndex);
			const FTransform BoneTransform(BinaryBone.LocalTransform);
			if (RefSkeleton.FindRawBoneIndex(BoneInfo.Name) != INDEX_NONE)
			{
				UE_LOG(LogInterchangeImport, Error, TEXT("Invalid Skeleton because of non-unique bone names [%s]"), *BoneInfo.Name.ToString());
				return false;
			}
			RefSkelModifier.Add(BoneInfo, BoneTransform);
		}

		// Add hierarchy index to each bone and detect max depth.
		SkeletalDepth = 0;

		TArray<int32> SkeletalDepths;
		SkeletalDepths.Empty(JointInfos.Num());
		SkeletalDepths.AddZeroed(JointInfos.Num());
		for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetRawBoneNum(); ++BoneIndex)
		{
			int32 Parent = RefSkeleton.GetRawParentIndex(BoneIndex);
			int32 Depth = 1.0f;

			SkeletalDepths[BoneIndex] = 1.0f;
			if (Parent != INDEX_NONE)
			{
				Depth += SkeletalDepths[Parent];
			}
			if (SkeletalDepth < Depth)
			{
				SkeletalDepth = Depth;
			}
			SkeletalDepths[BoneIndex] = Depth;
		}

		return true;
	}

	bool FSkeletonHelper::IsCompatibleSkeleton(const USkeleton* Skeleton, const FString RootJoinUid, const UInterchangeBaseNodeContainer* BaseNodeContainer)
	{
		// at least % of bone should match 
		int32 NumOfBoneMatches = 0;
		//Make sure the specified Skeleton fit this skeletal mesh
		const FReferenceSkeleton& SkeletonRef = Skeleton->GetReferenceSkeleton();
		const int32 SkeletonBoneCount = SkeletonRef.GetRawBoneNum();

		TArray<FMeshBoneInfo> SkeletalLodRawInfos;
		SkeletalLodRawInfos.Reserve(SkeletonBoneCount);
		RecursiveBuildSkeletalSkeleton(RootJoinUid, INDEX_NONE, BaseNodeContainer, SkeletalLodRawInfos);
		const int32 SkeletalLodBoneCount = SkeletalLodRawInfos.Num();

		// first ensure the parent exists for each bone
		for (int32 MeshBoneIndex = 0; MeshBoneIndex < SkeletalLodBoneCount; MeshBoneIndex++)
		{
			FName MeshBoneName = SkeletalLodRawInfos[MeshBoneIndex].Name;
			// See if Mesh bone exists in Skeleton.
			int32 SkeletonBoneIndex = SkeletonRef.FindBoneIndex(MeshBoneName);

			// if found, increase num of bone matches count
			if (SkeletonBoneIndex != INDEX_NONE)
			{
				++NumOfBoneMatches;

				// follow the parent chain to verify the chain is same
				if (!DoesParentChainMatch(SkeletonBoneIndex, SkeletonRef, SkeletalLodRawInfos))
				{
					//Not compatible
					return false;
				}
			}
			else
			{
				int32 CurrentBoneId = MeshBoneIndex;
				// if not look for parents that matches
				while (SkeletonBoneIndex == INDEX_NONE && CurrentBoneId != INDEX_NONE)
				{
					// find Parent one see exists
					const int32 ParentMeshBoneIndex = SkeletalLodGetParentIndex(SkeletalLodRawInfos, CurrentBoneId);
					if (ParentMeshBoneIndex != INDEX_NONE)
					{
						// @TODO: make sure RefSkeleton's root ParentIndex < 0 if not, I'll need to fix this by checking TreeBoneIdx
						FName ParentBoneName = SkeletalLodGetBoneName(SkeletalLodRawInfos, ParentMeshBoneIndex);
						SkeletonBoneIndex = SkeletonRef.FindBoneIndex(ParentBoneName);
					}

					// root is reached
					if (ParentMeshBoneIndex == 0)
					{
						break;
					}
					else
					{
						CurrentBoneId = ParentMeshBoneIndex;
					}
				}

				// still no match, return false, no parent to look for
				if (SkeletonBoneIndex == INDEX_NONE)
				{
					return false;
				}

				// second follow the parent chain to verify the chain is same
				if (!DoesParentChainMatch(SkeletonBoneIndex, SkeletonRef, SkeletalLodRawInfos))
				{
					return false;
				}
			}
		}

		// originally we made sure at least matches more than 50% 
		// but then follower components can't play since they're only partial
		// if the hierarchy matches, and if it's more then 1 bone, we allow
		return (NumOfBoneMatches > 0);
	}

	void FSkeletonHelper::RecursiveAddSkeletonMetaDataValues(UInterchangeBaseNodeContainer* NodeContainer, UInterchangeBaseNode* DestinationNode, const FString& JointUid)
	{
		const UInterchangeSceneNode* SceneNode = Cast<UInterchangeSceneNode>(NodeContainer->GetNode(JointUid));
		if (!SceneNode || !SceneNode->IsSpecializedTypeContains(FSceneNodeStaticData::GetJointSpecializeTypeString()))
		{
			return;
		}
		constexpr bool bAddSourceNodeName = true;
		UInterchangeUserDefinedAttributesAPI::DuplicateAllUserDefinedAttribute(SceneNode, DestinationNode, bAddSourceNodeName);

		//Iterate childrens
		const TArray<FString> ChildrenIds = NodeContainer->GetNodeChildrenUids(JointUid);
		for (int32 ChildIndex = 0; ChildIndex < ChildrenIds.Num(); ++ChildIndex)
		{
			RecursiveAddSkeletonMetaDataValues(NodeContainer, DestinationNode, ChildrenIds[ChildIndex]);
		}
	}

	void FSkeletonHelper::RecursiveAddBones(const UInterchangeBaseNodeContainer* NodeContainer, const FString& JointNodeId, TArray <FJointInfo>& JointInfos, int32 ParentIndex, TArray<SkeletalMeshImportData::FBone>& RefBonesBinary, const bool bUseTimeZeroAsBindPose, bool& bOutDiffPose)
	{
		const UInterchangeSceneNode* JointNode = Cast<UInterchangeSceneNode>(NodeContainer->GetNode(JointNodeId));
		if (!JointNode)
		{
			UE_LOG(LogInterchangeImport, Warning, TEXT("Invalid Skeleton Joint"));
			return;
		}

		int32 JointInfoIndex = JointInfos.Num();
		FJointInfo& Info = JointInfos.AddZeroed_GetRef();
		Info.Name = JointNode->GetDisplayLabel();

		FTransform LocalTransform;
		FTransform TimeZeroLocalTransform;
		bool bHasTimeZeroTransform = false;
		FTransform BindPoseLocalTransform;
		bool bHasBindPoseTransform = false;

		ensure(JointNode->GetCustomLocalTransform(LocalTransform));
		bHasTimeZeroTransform = JointNode->GetCustomTimeZeroLocalTransform(TimeZeroLocalTransform);
		bHasBindPoseTransform = JointNode->GetCustomBindPoseLocalTransform(BindPoseLocalTransform);

		if (ParentIndex == INDEX_NONE)
		{
			FTransform GlobalOffsetTransform = FTransform::Identity;
			bool bBakeMeshes = false;
			if (UInterchangeCommonPipelineDataFactoryNode* CommonPipelineDataFactoryNode = UInterchangeCommonPipelineDataFactoryNode::GetUniqueInstance(NodeContainer))
			{
				CommonPipelineDataFactoryNode->GetCustomGlobalOffsetTransform(GlobalOffsetTransform);
				CommonPipelineDataFactoryNode->GetBakeMeshes(bBakeMeshes);
			}

			if (bBakeMeshes)
			{
				LocalTransform = FTransform::Identity;
				ensure(JointNode->GetCustomGlobalTransform(NodeContainer, GlobalOffsetTransform, LocalTransform));
				bHasTimeZeroTransform = JointNode->GetCustomTimeZeroGlobalTransform(NodeContainer, GlobalOffsetTransform, TimeZeroLocalTransform);
				bHasBindPoseTransform = JointNode->GetCustomBindPoseGlobalTransform(NodeContainer, GlobalOffsetTransform, BindPoseLocalTransform);
			}
		}

		Info.LocalTransform = bHasBindPoseTransform ? BindPoseLocalTransform : LocalTransform;
		//If user want to bind the mesh at time zero try to get the time zero transform
		if (bUseTimeZeroAsBindPose && bHasTimeZeroTransform)
		{
			if (bHasBindPoseTransform)
			{
				if (!TimeZeroLocalTransform.Equals(Info.LocalTransform))
				{
					bOutDiffPose = true;
				}
			}
			Info.LocalTransform = TimeZeroLocalTransform;
		}
		else if (bHasBindPoseTransform)
		{
			Info.LocalTransform = BindPoseLocalTransform;
		}

		Info.ParentIndex = ParentIndex;

		SkeletalMeshImportData::FBone& Bone = RefBonesBinary.AddZeroed_GetRef();
		Bone.Name = Info.Name;
		Bone.BonePos.Transform = FTransform3f(Info.LocalTransform);
		Bone.ParentIndex = ParentIndex;
		//Fill the scrap we do not need
		Bone.BonePos.Length = 0.0f;
		Bone.BonePos.XSize = 1.0f;
		Bone.BonePos.YSize = 1.0f;
		Bone.BonePos.ZSize = 1.0f;

		const TArray<FString> ChildrenIds = NodeContainer->GetNodeChildrenUids(JointNodeId);
		Bone.NumChildren = ChildrenIds.Num();
		for (int32 ChildIndex = 0; ChildIndex < ChildrenIds.Num(); ++ChildIndex)
		{
			RecursiveAddBones(NodeContainer, ChildrenIds[ChildIndex], JointInfos, JointInfoIndex, RefBonesBinary, bUseTimeZeroAsBindPose, bOutDiffPose);
		}
	}

	FName FSkeletonHelper::SkeletalLodGetBoneName(const TArray<FMeshBoneInfo>& SkeletalLodRawInfos, int32 BoneIndex)
	{
		if (SkeletalLodRawInfos.IsValidIndex(BoneIndex))
		{
			return SkeletalLodRawInfos[BoneIndex].Name;
		}
		return NAME_None;
	}

	int32 FSkeletonHelper::SkeletalLodFindBoneIndex(const TArray<FMeshBoneInfo>& SkeletalLodRawInfos, FName BoneName)
	{
		const int32 BoneCount = SkeletalLodRawInfos.Num();
		for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
		{
			if (SkeletalLodRawInfos[BoneIndex].Name == BoneName)
			{
				return BoneIndex;
			}
		}
		return INDEX_NONE;
	}

	int32 FSkeletonHelper::SkeletalLodGetParentIndex(const TArray<FMeshBoneInfo>& SkeletalLodRawInfos, int32 BoneIndex)
	{
		if (SkeletalLodRawInfos.IsValidIndex(BoneIndex))
		{
			return SkeletalLodRawInfos[BoneIndex].ParentIndex;
		}
		return INDEX_NONE;
	}

	bool FSkeletonHelper::DoesParentChainMatch(int32 StartBoneIndex, const FReferenceSkeleton& SkeletonRef, const TArray<FMeshBoneInfo>& SkeletalLodRawInfos)
	{
		// if start is root bone
		if (StartBoneIndex == 0)
		{
			// verify name of root bone matches
			return (SkeletonRef.GetBoneName(0) == SkeletalLodGetBoneName(SkeletalLodRawInfos, 0));
		}

		int32 SkeletonBoneIndex = StartBoneIndex;
		// If skeleton bone is not found in mesh, fail.
		int32 MeshBoneIndex = SkeletalLodFindBoneIndex(SkeletalLodRawInfos, SkeletonRef.GetBoneName(SkeletonBoneIndex));
		if (MeshBoneIndex == INDEX_NONE)
		{
			return false;
		}
		do
		{
			// verify if parent name matches
			int32 ParentSkeletonBoneIndex = SkeletonRef.GetParentIndex(SkeletonBoneIndex);
			int32 ParentMeshBoneIndex = SkeletalLodGetParentIndex(SkeletalLodRawInfos, MeshBoneIndex);

			// if one of the parents doesn't exist, make sure both end. Otherwise fail.
			if ((ParentSkeletonBoneIndex == INDEX_NONE) || (ParentMeshBoneIndex == INDEX_NONE))
			{
				return (ParentSkeletonBoneIndex == ParentMeshBoneIndex);
			}

			// If parents are not named the same, fail.
			if (SkeletonRef.GetBoneName(ParentSkeletonBoneIndex) != SkeletalLodGetBoneName(SkeletalLodRawInfos, ParentMeshBoneIndex))
			{
				return false;
			}

			// move up
			SkeletonBoneIndex = ParentSkeletonBoneIndex;
			MeshBoneIndex = ParentMeshBoneIndex;
		} while (true);

		return true;
	}

	void FSkeletonHelper::RecursiveBuildSkeletalSkeleton(const FString JoinToAddUid, const int32 ParentIndex, const UInterchangeBaseNodeContainer* BaseNodeContainer, TArray<FMeshBoneInfo>& SkeletalLodRawInfos)
	{
		const UInterchangeSceneNode* SceneNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(JoinToAddUid));
		if (!SceneNode || !SceneNode->IsSpecializedTypeContains(FSceneNodeStaticData::GetJointSpecializeTypeString()))
		{
			return;
		}

		int32 JoinIndex = SkeletalLodRawInfos.Num();
		FMeshBoneInfo& Info = SkeletalLodRawInfos.AddZeroed_GetRef();
		Info.Name = *SceneNode->GetDisplayLabel();
		Info.ParentIndex = ParentIndex;
#if WITH_EDITORONLY_DATA
		Info.ExportName = Info.Name.ToString();
#endif
		//Iterate childrens
		const TArray<FString> ChildrenIds = BaseNodeContainer->GetNodeChildrenUids(JoinToAddUid);
		for (int32 ChildIndex = 0; ChildIndex < ChildrenIds.Num(); ++ChildIndex)
		{
			RecursiveBuildSkeletalSkeleton(ChildrenIds[ChildIndex], JoinIndex, BaseNodeContainer, SkeletalLodRawInfos);
		}
	}
} //namespace UE::Interchange::Private

#endif //WITH_EDITOR