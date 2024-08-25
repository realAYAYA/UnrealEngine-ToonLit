// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangePipelineMeshesUtilities.h"

#include "CoreMinimal.h"
#include "Nodes/InterchangeBaseNode.h"
#include "InterchangeMeshNode.h"
#include "InterchangeSceneNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangePipelineMeshesUtilities)

namespace UE::Private::InterchangeMeshPipeline
{
	void FindNamedLodGroup(UInterchangeBaseNodeContainer* BaseNodeContainer, TMap<FString, TArray<FString>>& SceneMeshNodeUidsPerLodParentUidMap)
	{
		const FString LodPrefix = TEXT("LOD");
		BaseNodeContainer->IterateNodes(
			[&BaseNodeContainer, &SceneMeshNodeUidsPerLodParentUidMap, &LodPrefix](const FString& NodeUid, UInterchangeBaseNode* Node)
			{
				if (Node->GetNodeContainerType() == EInterchangeNodeContainerType::TranslatedScene)
				{
					const UInterchangeSceneNode* SceneNode = Cast<UInterchangeSceneNode>(Node);
					if (!SceneNode)
					{
						return;
					}
					FString MeshUid;
					if (!SceneNode->GetCustomAssetInstanceUid(MeshUid))
					{
						return;
					}
					const UInterchangeBaseNode* MeshNode = BaseNodeContainer->GetNode(MeshUid);
					if (!MeshNode || !MeshNode->IsA<UInterchangeMeshNode>())
					{
						return;
					}
					FString ParentUniqueID = SceneNode->GetParentUid();
					const UInterchangeSceneNode* ParentSceneNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(ParentUniqueID));
					if (!ParentSceneNode)
					{
						return;
					}

					//Skip this node if there is a parent in the hierarchy that is a specialize lod group
					const UInterchangeSceneNode* ParentSceneNodeHierarchy = ParentSceneNode;
					while (ParentSceneNodeHierarchy)
					{
						if (ParentSceneNodeHierarchy->IsSpecializedTypeContains(UE::Interchange::FSceneNodeStaticData::GetLodGroupSpecializeTypeString()))
						{
							return;
						}
						ParentSceneNodeHierarchy = Cast<const UInterchangeSceneNode>(BaseNodeContainer->GetNode(ParentSceneNodeHierarchy->GetParentUid()));
					}

					if (BaseNodeContainer->GetNodeChildrenCount(ParentUniqueID) <= 1)
					{
						//Do not create custom named LOD group that have only one lod
						return;
					}

					FString SceneNodeName = SceneNode->GetDisplayLabel();
					if (SceneNodeName.Len() > 5 && SceneNodeName.StartsWith(LodPrefix, ESearchCase::CaseSensitive) && SceneNodeName[4] == '_')
					{
						FString LODXNumber = SceneNodeName.RightChop(3).Left(1);
						if (LODXNumber.IsNumeric())
						{
							int32 LodNumber = FPlatformString::Atoi(*LODXNumber);
							FString MatchName = ParentUniqueID;
							
							TArray<FString>& LodChildUids = SceneMeshNodeUidsPerLodParentUidMap.FindOrAdd(MatchName);
							//Add LOD at the correct index
							if (LodNumber >= LodChildUids.Num())
							{
								int32 AddCount = LodNumber + 1 - LodChildUids.Num();
								LodChildUids.AddDefaulted(AddCount);
							}
							LodChildUids[LodNumber] = SceneNode->GetUniqueID();
						}
					}
				}
			}
		);

		//Remove all empty entry, we use empty entry to set all lod in the correct order
		for (TPair<FString, TArray<FString>>& LodPrefixNodePair : SceneMeshNodeUidsPerLodParentUidMap)
		{
			TArray<FString>& LodChildUids = LodPrefixNodePair.Value;
			for (int32 ChildLodIndex = LodChildUids.Num() - 1; ChildLodIndex >= 0; ChildLodIndex--)
			{
				if (LodChildUids[ChildLodIndex].IsEmpty())
				{
					LodChildUids.RemoveAt(ChildLodIndex, 1, EAllowShrinking::No);
				}
			}
			//Shrink the array to the correct size
			LodChildUids.Shrink();
		}
	}

	void CollectAllChildren(UInterchangeBaseNodeContainer* BaseNodeContainer, const FString SceneNodeUids, TArray<FString>& AllChildrenUids)
	{
		AllChildrenUids.Add(SceneNodeUids);
		TArray<FString> ChildrenUids = BaseNodeContainer->GetNodeChildrenUids(SceneNodeUids);
		for (const FString& ChildUid : ChildrenUids)
		{
			CollectAllChildren(BaseNodeContainer, ChildUid, AllChildrenUids);
		}
	}

	bool IsSceneNodeNestedInSkeleton(UInterchangeBaseNodeContainer* BaseNodeContainer, const UInterchangeSceneNode* SceneNode)
	{
		auto IsJointHookToDeformer = [&BaseNodeContainer](const FString JointNodeUid)->bool
		{
			TArray<FString> AllChildrenUids;
			CollectAllChildren(BaseNodeContainer, JointNodeUid, AllChildrenUids);
			bool bResult = false;
			BaseNodeContainer->BreakableIterateNodesOfType<UInterchangeMeshNode>([&AllChildrenUids, &bResult](const FString& MeshUid, UInterchangeMeshNode* MeshNode)
			{
				if (MeshNode->IsSkinnedMesh())
				{
					TArray<FString> SkeletonDependencies;
					MeshNode->GetSkeletonDependencies(SkeletonDependencies);
					for (const FString& MeshJointUid : SkeletonDependencies)
					{
						if (AllChildrenUids.Contains(MeshJointUid))
						{
							bResult = true;
							return true;
						}
					}
				}
				return false;
			});

			return bResult;
		};

		if (SceneNode->IsSpecializedTypeContains(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString()))
		{
			if(IsJointHookToDeformer(SceneNode->GetUniqueID()))
			{
				return true;
			}
		}
		FString ParentUid = SceneNode->GetParentUid();
		const UInterchangeSceneNode* ParentNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(ParentUid));
		while (ParentNode)
		{
			if (ParentNode->IsSpecializedTypeContains(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString()))
			{
				if (IsJointHookToDeformer(ParentNode->GetUniqueID()))
				{
					return true;
				}
			}
			ParentUid = ParentNode->GetParentUid();
			ParentNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(ParentUid));
		}
		return false;
	}

}

static bool IsSceneNodeASocket(const UInterchangeSceneNode* SceneNode)
{
	// Generic pipeline determines where a scene node should be considered as a socket from its naming convention.
	// This is no longer decided by the translator.
	FString NodeDisplayName = SceneNode->GetDisplayLabel();
	return NodeDisplayName.StartsWith(TEXT("SOCKET_"));
}

bool FInterchangePipelineMeshesUtilitiesContext::IsStaticMeshInstance(const FInterchangeMeshInstance& MeshInstance, UInterchangeBaseNodeContainer* BaseNodeContainer)
{
	return !IsSkeletalMeshInstance(MeshInstance, BaseNodeContainer);
}

bool FInterchangePipelineMeshesUtilitiesContext::IsSkeletalMeshInstance(const FInterchangeMeshInstance& MeshInstance, UInterchangeBaseNodeContainer* BaseNodeContainer)
{
	bool bOutIsStaticMeshNestedInSkeleton = false;
	return IsSkeletalMeshInstance(MeshInstance, BaseNodeContainer, bOutIsStaticMeshNestedInSkeleton);
}

bool FInterchangePipelineMeshesUtilitiesContext::IsSkeletalMeshInstance(const FInterchangeMeshInstance& MeshInstance, UInterchangeBaseNodeContainer* BaseNodeContainer, bool& bOutIsStaticMeshNestedInSkeleton)
{
	bOutIsStaticMeshNestedInSkeleton = false;
	if (bConvertSkeletalMeshToStaticMesh)
	{
		return false;
	}
	if ((bConvertStaticMeshToSkeletalMesh || MeshInstance.bReferenceSkinnedMesh || (bConvertStaticsWithMorphTargetsToSkeletals && MeshInstance.bHasMorphTargets)) && !MeshInstance.bReferenceMorphTarget)
	{
		return true;
	}
	else if (bImportMeshesInBoneHierarchy)
	{
		if (MeshInstance.SceneNodePerLodIndex.Contains(0))
		{
			const FInterchangeLodSceneNodeContainer& LodSceneNodeContainer = MeshInstance.SceneNodePerLodIndex.FindChecked(0);
			for (const TObjectPtr<const UInterchangeSceneNode>& SceneNode : LodSceneNodeContainer.SceneNodes)
			{
				if (UE::Private::InterchangeMeshPipeline::IsSceneNodeNestedInSkeleton(BaseNodeContainer, SceneNode))
				{
					bOutIsStaticMeshNestedInSkeleton = true;
					return true;
				}
			}
		}
	}
	return false;
}

bool FInterchangePipelineMeshesUtilitiesContext::IsStaticMeshGeometry(const FInterchangeMeshGeometry& MeshGeometry)
{
	if (bQueryGeometryOnlyIfNoInstance && MeshGeometry.ReferencingMeshInstanceUids.Num() > 0)
	{
		return false;
	}
	if (MeshGeometry.MeshNode->IsMorphTarget())
	{
		return false;
	}
	return !IsSkeletalMeshGeometry(MeshGeometry);
}

bool FInterchangePipelineMeshesUtilitiesContext::IsSkeletalMeshGeometry(const FInterchangeMeshGeometry& MeshGeometry)
{
	if (bConvertSkeletalMeshToStaticMesh)
	{
		return false;
	}

	if (bQueryGeometryOnlyIfNoInstance && MeshGeometry.ReferencingMeshInstanceUids.Num() > 0)
	{
		return false;
	}

	if (MeshGeometry.MeshNode->IsMorphTarget())
	{
		return false;
	}

	if (bConvertStaticMeshToSkeletalMesh
		|| MeshGeometry.MeshNode->IsSkinnedMesh()
		|| (bConvertStaticsWithMorphTargetsToSkeletals && (MeshGeometry.MeshNode->GetMorphTargetDependeciesCount() > 0)))
	{
		return true;
	}
	return false;
}

UInterchangePipelineMeshesUtilities* UInterchangePipelineMeshesUtilities::CreateInterchangePipelineMeshesUtilities(UInterchangeBaseNodeContainer* BaseNodeContainer)
{
	check(BaseNodeContainer);
	UInterchangePipelineMeshesUtilities* PipelineMeshesUtilities = NewObject<UInterchangePipelineMeshesUtilities>(GetTransientPackage(), NAME_None);
	
	//Set the container
	PipelineMeshesUtilities->BaseNodeContainer = BaseNodeContainer;

	TArray<FString> SkeletonRootNodeUids;
	
	//Find all translated node we need for this pipeline
	BaseNodeContainer->IterateNodes(
		[&PipelineMeshesUtilities, &BaseNodeContainer, &SkeletonRootNodeUids](const FString& NodeUid, UInterchangeBaseNode* Node)
		{
			if (Node->GetNodeContainerType() == EInterchangeNodeContainerType::TranslatedAsset)
			{
				if (const UInterchangeMeshNode* MeshNode = Cast<UInterchangeMeshNode>(Node))
				{
					FInterchangeMeshGeometry& MeshGeometry = PipelineMeshesUtilities->MeshGeometriesPerMeshUid.FindOrAdd(NodeUid);
					MeshGeometry.MeshUid = NodeUid;
					MeshGeometry.MeshNode = MeshNode;
				}
			}
		}
	);

	TMap<FString, TArray<FString>> SceneMeshNodeUidsPerLodParentUidMap;
	UE::Private::InterchangeMeshPipeline::FindNamedLodGroup(BaseNodeContainer, SceneMeshNodeUidsPerLodParentUidMap);

	//Find all translated scene node we need for this pipeline
	bool bHasSockets = false;
	BaseNodeContainer->IterateNodes(
		[&PipelineMeshesUtilities, &BaseNodeContainer, &SceneMeshNodeUidsPerLodParentUidMap, &SkeletonRootNodeUids, &bHasSockets](const FString& NodeUid, const UInterchangeBaseNode* Node)
		{
			if (Node->GetNodeContainerType() == EInterchangeNodeContainerType::TranslatedScene)
			{
				if (const UInterchangeSceneNode* SceneNode = Cast<UInterchangeSceneNode>(Node))
				{
					if (SceneNode->IsSpecializedTypeContains(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString()))
					{
						const UInterchangeSceneNode* ParentJointNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(SceneNode->GetParentUid()));
						if (!ParentJointNode || !ParentJointNode->IsSpecializedTypeContains(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString()))
						{
							SkeletonRootNodeUids.Add(SceneNode->GetUniqueID());
						}
					}

					if (IsSceneNodeASocket(SceneNode))
					{
						bHasSockets = true;
					}

					FString MeshUid;
					if (SceneNode->GetCustomAssetInstanceUid(MeshUid))
					{
						const UInterchangeBaseNode* MeshNode = BaseNodeContainer->GetNode(MeshUid);

						if (MeshNode && MeshNode->IsA<UInterchangeMeshNode>())
						{
							const UInterchangeSceneNode* ParentMeshSceneNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(SceneNode->GetParentUid()));
							if (ParentMeshSceneNode)
							{
								const UInterchangeSceneNode* LodGroupNode = nullptr;
								int32 LodIndex = 0;
								if (SceneMeshNodeUidsPerLodParentUidMap.Contains(ParentMeshSceneNode->GetUniqueID()))
								{
									LodGroupNode = ParentMeshSceneNode;
									const TArray<FString>& LodChildUids = SceneMeshNodeUidsPerLodParentUidMap.FindChecked(ParentMeshSceneNode->GetUniqueID());
									for (int32 ChildLodIndex = 0; ChildLodIndex < LodChildUids.Num(); ++ChildLodIndex)
									{
										const FString& ChildrenUid = LodChildUids[ChildLodIndex];
										if (ChildrenUid.Equals(SceneNode->GetUniqueID()))
										{
											LodIndex = ChildLodIndex;
											break;
										}
									}
								}
								else
								{
									FString LastChildUid = SceneNode->GetUniqueID();
									do
									{
										if (ParentMeshSceneNode->IsSpecializedTypeContains(UE::Interchange::FSceneNodeStaticData::GetLodGroupSpecializeTypeString()))
										{
											LodGroupNode = ParentMeshSceneNode;
											TArray<FString> LodGroupChildrens = BaseNodeContainer->GetNodeChildrenUids(ParentMeshSceneNode->GetUniqueID());
											for (int32 ChildLodIndex = 0; ChildLodIndex < LodGroupChildrens.Num(); ++ChildLodIndex)
											{
												const FString& ChildrenUid = LodGroupChildrens[ChildLodIndex];
												if (ChildrenUid.Equals(LastChildUid))
												{
													LodIndex = ChildLodIndex;
													break;
												}
											}
											break;
										}
										LastChildUid = ParentMeshSceneNode->GetUniqueID();
										ParentMeshSceneNode = Cast<const UInterchangeSceneNode>(BaseNodeContainer->GetNode(ParentMeshSceneNode->GetParentUid()));
									} while (ParentMeshSceneNode);
								}
								FInterchangeMeshGeometry& MeshGeometry = PipelineMeshesUtilities->MeshGeometriesPerMeshUid.FindChecked(MeshUid);
								if (LodGroupNode)
								{
									//We have a LOD
									FInterchangeMeshInstance& MeshInstance = PipelineMeshesUtilities->MeshInstancesPerMeshInstanceUid.FindOrAdd(LodGroupNode->GetUniqueID());
									if (MeshInstance.LodGroupNode != nullptr)
									{
										//This LodGroup was already created, verify everything is ok
										checkSlow(MeshInstance.LodGroupNode == LodGroupNode);
										checkSlow(MeshInstance.MeshInstanceUid.Equals(LodGroupNode->GetUniqueID()));
									}
									else
									{
										MeshInstance.LodGroupNode = LodGroupNode;
										MeshInstance.MeshInstanceUid = LodGroupNode->GetUniqueID();
									}
									FInterchangeLodSceneNodeContainer& InstancedSceneNodes = MeshInstance.SceneNodePerLodIndex.FindOrAdd(LodIndex);
									InstancedSceneNodes.SceneNodes.AddUnique(SceneNode);
									MeshGeometry.ReferencingMeshInstanceUids.Add(MeshInstance.MeshInstanceUid);
									MeshInstance.ReferencingMeshGeometryUids.Add(MeshUid);
									MeshInstance.bReferenceSkinnedMesh |= MeshGeometry.MeshNode->IsSkinnedMesh();
									MeshInstance.bReferenceMorphTarget |= MeshGeometry.MeshNode->IsMorphTarget();
									MeshInstance.bHasMorphTargets |= MeshGeometry.MeshNode->GetMorphTargetDependeciesCount() > 0;
								}
								else
								{
									FInterchangeMeshInstance& MeshInstance = PipelineMeshesUtilities->MeshInstancesPerMeshInstanceUid.FindOrAdd(NodeUid);
									MeshInstance.LodGroupNode = nullptr;
									MeshInstance.MeshInstanceUid = NodeUid;
									FInterchangeLodSceneNodeContainer& InstancedSceneNodes = MeshInstance.SceneNodePerLodIndex.FindOrAdd(LodIndex);
									InstancedSceneNodes.SceneNodes.AddUnique(SceneNode);
									MeshGeometry.ReferencingMeshInstanceUids.Add(MeshInstance.MeshInstanceUid);
									MeshInstance.ReferencingMeshGeometryUids.Add(MeshUid);
									MeshInstance.bReferenceSkinnedMesh |= MeshGeometry.MeshNode->IsSkinnedMesh();
									MeshInstance.bReferenceMorphTarget |= MeshGeometry.MeshNode->IsMorphTarget();
									MeshInstance.bHasMorphTargets |= MeshGeometry.MeshNode->GetMorphTargetDependeciesCount() > 0;
								}
							}
						}
					}
				}
			}
		}
	);

	// Do a second pass to discover sockets
	if (bHasSockets)
	{
		BaseNodeContainer->IterateNodes(
			[&PipelineMeshesUtilities, &BaseNodeContainer](const FString& NodeUid, const UInterchangeBaseNode* Node)
			{
				if (const UInterchangeSceneNode* SceneNode = Cast<UInterchangeSceneNode>(Node))
				{
					if (IsSceneNodeASocket(SceneNode))
					{
						FString MeshUid;
						if (!SceneNode->GetCustomAssetInstanceUid(MeshUid))
						{
							const UInterchangeSceneNode* ParentMeshSceneNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(SceneNode->GetParentUid()));
							while (ParentMeshSceneNode)
							{
								if (ParentMeshSceneNode->GetCustomAssetInstanceUid(MeshUid))
								{
									break;
								}

								ParentMeshSceneNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(ParentMeshSceneNode->GetParentUid()));
							}
						}

						if (!MeshUid.IsEmpty())
						{
							FInterchangeMeshGeometry& MeshGeometry = PipelineMeshesUtilities->MeshGeometriesPerMeshUid.FindChecked(MeshUid);
							MeshGeometry.AttachedSocketUids.Add(SceneNode->GetUniqueID());
						}
					}
				}
			}
		);
	}


	//Fill the SkeletonRootUidPerMeshUid data
	for (const TPair<FString, FInterchangeMeshGeometry>& MeshGeometryUidAndMeshGeometry : PipelineMeshesUtilities->MeshGeometriesPerMeshUid)
	{
		const FInterchangeMeshGeometry& MeshGeometry = MeshGeometryUidAndMeshGeometry.Value;
		if (!ensure(MeshGeometry.MeshNode))
		{
			continue;
		}
		if (!MeshGeometry.MeshNode->IsSkinnedMesh() || PipelineMeshesUtilities->SkeletonRootUidPerMeshUid.Contains(MeshGeometry.MeshUid))
		{
			continue;
		}
		const UInterchangeMeshNode* SkinnedMeshNode = MeshGeometry.MeshNode;
		if (!SkinnedMeshNode || SkinnedMeshNode->GetSkeletonDependeciesCount() == 0)
		{
			continue;
		}
		//Find the root joint for this MeshGeometry
		FString JointNodeUid;
		SkinnedMeshNode->GetSkeletonDependency(0, JointNodeUid);
		while (!JointNodeUid.Equals(UInterchangeBaseNode::InvalidNodeUid()) && !SkeletonRootNodeUids.Contains(JointNodeUid))
		{
			JointNodeUid = BaseNodeContainer->GetNode(JointNodeUid)->GetParentUid();
		}
		//Add the MeshGeometry to the map per joint uid
		if (SkeletonRootNodeUids.Contains(JointNodeUid))
		{
			PipelineMeshesUtilities->SkeletonRootUidPerMeshUid.Add(MeshGeometry.MeshUid, JointNodeUid);
		}
	}

	return PipelineMeshesUtilities;
}

void UInterchangePipelineMeshesUtilities::GetAllMeshInstanceUids(TArray<FString>& MeshInstanceUids) const
{
	MeshInstancesPerMeshInstanceUid.GetKeys(MeshInstanceUids);
}

void UInterchangePipelineMeshesUtilities::IterateAllMeshInstance(TFunctionRef<void(const FInterchangeMeshInstance&)> IterationLambda) const
{
	for (const TPair<FString, FInterchangeMeshInstance>& MeshInstanceUidAndMeshInstance : MeshInstancesPerMeshInstanceUid)
	{
		const FInterchangeMeshInstance& MeshInstance = MeshInstanceUidAndMeshInstance.Value;
		IterationLambda(MeshInstance);
	}
}

void UInterchangePipelineMeshesUtilities::GetAllSkinnedMeshInstance(TArray<FString>& MeshInstanceUids) const
{
	MeshInstanceUids.Empty(MeshInstancesPerMeshInstanceUid.Num());
	for (const TPair<FString, FInterchangeMeshInstance>& MeshInstanceUidAndMeshInstance : MeshInstancesPerMeshInstanceUid)
	{
		const FInterchangeMeshInstance& MeshInstance = MeshInstanceUidAndMeshInstance.Value;
		if (CurrentDataContext.IsSkeletalMeshInstance(MeshInstance, BaseNodeContainer))
		{
			MeshInstanceUids.Add(MeshInstance.MeshInstanceUid);
		}
	}
}

void UInterchangePipelineMeshesUtilities::IterateAllSkinnedMeshInstance(TFunctionRef<void(const FInterchangeMeshInstance&)> IterationLambda) const
{
	for (const TPair<FString, FInterchangeMeshInstance>& MeshInstanceUidAndMeshInstance : MeshInstancesPerMeshInstanceUid)
	{
		const FInterchangeMeshInstance& MeshInstance = MeshInstanceUidAndMeshInstance.Value;
		if (CurrentDataContext.IsSkeletalMeshInstance(MeshInstance, BaseNodeContainer))
		{
			IterationLambda(MeshInstance);
		}
	}
}

void UInterchangePipelineMeshesUtilities::GetAllStaticMeshInstance(TArray<FString>& MeshInstanceUids) const
{
	MeshInstanceUids.Empty(MeshInstancesPerMeshInstanceUid.Num());
	for (const TPair<FString, FInterchangeMeshInstance>& MeshInstanceUidAndMeshInstance : MeshInstancesPerMeshInstanceUid)
	{
		const FInterchangeMeshInstance& MeshInstance = MeshInstanceUidAndMeshInstance.Value;
		if (CurrentDataContext.IsStaticMeshInstance(MeshInstance, BaseNodeContainer))
		{
			MeshInstanceUids.Add(MeshInstance.MeshInstanceUid);
		}
	}
}

void UInterchangePipelineMeshesUtilities::IterateAllStaticMeshInstance(TFunctionRef<void(const FInterchangeMeshInstance&)> IterationLambda) const
{
	for (const TPair<FString, FInterchangeMeshInstance>& MeshInstanceUidAndMeshInstance : MeshInstancesPerMeshInstanceUid)
	{
		const FInterchangeMeshInstance& MeshInstance = MeshInstanceUidAndMeshInstance.Value;
		if (CurrentDataContext.IsStaticMeshInstance(MeshInstance, BaseNodeContainer))
		{
			IterationLambda(MeshInstance);
		}
	}
}

void UInterchangePipelineMeshesUtilities::GetAllMeshGeometry(TArray<FString>& MeshGeometryUids) const
{
	MeshGeometriesPerMeshUid.GetKeys(MeshGeometryUids);
}

void UInterchangePipelineMeshesUtilities::IterateAllMeshGeometry(TFunctionRef<void(const FInterchangeMeshGeometry&)> IterationLambda) const
{
	for (const TPair<FString, FInterchangeMeshGeometry>& MeshGeometryUidAndMeshGeometry : MeshGeometriesPerMeshUid)
	{
		const FInterchangeMeshGeometry& MeshGeometry = MeshGeometryUidAndMeshGeometry.Value;
		IterationLambda(MeshGeometry);
	}
}

void UInterchangePipelineMeshesUtilities::GetAllSkinnedMeshGeometry(TArray<FString>& MeshGeometryUids) const
{
	MeshGeometryUids.Empty(MeshGeometriesPerMeshUid.Num());
	for (const TPair<FString, FInterchangeMeshGeometry>& MeshGeometryUidAndMeshGeometry : MeshGeometriesPerMeshUid)
	{
		const FInterchangeMeshGeometry& MeshGeometry = MeshGeometryUidAndMeshGeometry.Value;
		if(CurrentDataContext.IsSkeletalMeshGeometry(MeshGeometry))
		{
			MeshGeometryUids.Add(MeshGeometry.MeshUid);
		}
	}
}

void UInterchangePipelineMeshesUtilities::IterateAllSkinnedMeshGeometry(TFunctionRef<void(const FInterchangeMeshGeometry&)> IterationLambda) const
{
	for (const TPair<FString, FInterchangeMeshGeometry>& MeshGeometryUidAndMeshGeometry : MeshGeometriesPerMeshUid)
	{
		const FInterchangeMeshGeometry& MeshGeometry = MeshGeometryUidAndMeshGeometry.Value;
		if (CurrentDataContext.IsSkeletalMeshGeometry(MeshGeometry))
		{
			IterationLambda(MeshGeometry);
		}
	}
}

void UInterchangePipelineMeshesUtilities::GetAllStaticMeshGeometry(TArray<FString>& MeshGeometryUids) const
{
	MeshGeometryUids.Empty(MeshGeometriesPerMeshUid.Num());
	for (const TPair<FString, FInterchangeMeshGeometry>& MeshGeometryUidAndMeshGeometry : MeshGeometriesPerMeshUid)
	{
		const FInterchangeMeshGeometry& MeshGeometry = MeshGeometryUidAndMeshGeometry.Value;
		if (CurrentDataContext.IsStaticMeshGeometry(MeshGeometry))
		{
			MeshGeometryUids.Add(MeshGeometry.MeshUid);
		}
	}
}

void UInterchangePipelineMeshesUtilities::IterateAllStaticMeshGeometry(TFunctionRef<void(const FInterchangeMeshGeometry&)> IterationLambda) const
{
	for (const TPair<FString, FInterchangeMeshGeometry>& MeshGeometryUidAndMeshGeometry : MeshGeometriesPerMeshUid)
	{
		const FInterchangeMeshGeometry& MeshGeometry = MeshGeometryUidAndMeshGeometry.Value;
		if (CurrentDataContext.IsStaticMeshGeometry(MeshGeometry))
		{
			IterationLambda(MeshGeometry);
		}
	}
}

void UInterchangePipelineMeshesUtilities::GetAllMeshGeometryNotInstanced(TArray<FString>& MeshGeometryUids) const
{
	MeshGeometryUids.Empty(MeshGeometriesPerMeshUid.Num());
	for (const TPair<FString, FInterchangeMeshGeometry>& MeshGeometryUidAndMeshGeometry : MeshGeometriesPerMeshUid)
	{
		const FInterchangeMeshGeometry& MeshGeometry = MeshGeometryUidAndMeshGeometry.Value;
		if (MeshGeometry.ReferencingMeshInstanceUids.Num() == 0)
		{
			MeshGeometryUids.Add(MeshGeometry.MeshUid);
		}
	}
}

void UInterchangePipelineMeshesUtilities::IterateAllMeshGeometryNotIntanced(TFunctionRef<void(const FInterchangeMeshGeometry&)> IterationLambda) const
{
	for (const TPair<FString, FInterchangeMeshGeometry>& MeshGeometryUidAndMeshGeometry : MeshGeometriesPerMeshUid)
	{
		const FInterchangeMeshGeometry& MeshGeometry = MeshGeometryUidAndMeshGeometry.Value;
		if (MeshGeometry.ReferencingMeshInstanceUids.Num() == 0)
		{
			IterationLambda(MeshGeometry);
		}
	}
}

bool UInterchangePipelineMeshesUtilities::IsValidMeshInstanceUid(const FString& MeshInstanceUid) const
{
	return MeshInstancesPerMeshInstanceUid.Contains(MeshInstanceUid);
}

const FInterchangeMeshInstance& UInterchangePipelineMeshesUtilities::GetMeshInstanceByUid(const FString& MeshInstanceUid) const
{
	return MeshInstancesPerMeshInstanceUid.FindChecked(MeshInstanceUid);
}

bool UInterchangePipelineMeshesUtilities::IsValidMeshGeometryUid(const FString& MeshGeometryUid) const
{
	return MeshGeometriesPerMeshUid.Contains(MeshGeometryUid);
}

const FInterchangeMeshGeometry& UInterchangePipelineMeshesUtilities::GetMeshGeometryByUid(const FString& MeshGeometryUid) const
{
	return MeshGeometriesPerMeshUid.FindChecked(MeshGeometryUid);
}

void UInterchangePipelineMeshesUtilities::GetAllMeshInstanceUidsUsingMeshGeometryUid(const FString& MeshGeometryUid, TArray<FString>& MeshInstanceUids) const
{
	const FInterchangeMeshGeometry& MeshGeometry = MeshGeometriesPerMeshUid.FindChecked(MeshGeometryUid);
	MeshInstanceUids = MeshGeometry.ReferencingMeshInstanceUids;
}

void UInterchangePipelineMeshesUtilities::IterateAllMeshInstanceUsingMeshGeometry(const FString& MeshGeometryUid, TFunctionRef<void(const FInterchangeMeshInstance&)> IterationLambda) const
{
	const FInterchangeMeshGeometry& MeshGeometry = MeshGeometriesPerMeshUid.FindChecked(MeshGeometryUid);
	for (const FString& MeshInstanceUid : MeshGeometry.ReferencingMeshInstanceUids)
	{
		const FInterchangeMeshInstance& MeshInstance = MeshInstancesPerMeshInstanceUid.FindChecked(MeshInstanceUid);
		IterationLambda(MeshInstance);
	}
}

void UInterchangePipelineMeshesUtilities::GetCombinedSkinnedMeshInstances(TMap<FString, TArray<FString>>& OutMeshInstanceUidsPerSkeletonRootUid) const
{
	check(BaseNodeContainer);

	for (const TPair<FString, FInterchangeMeshInstance>& MeshInstanceUidAndMeshInstance : MeshInstancesPerMeshInstanceUid)
	{
		const FInterchangeMeshInstance& MeshInstance = MeshInstanceUidAndMeshInstance.Value;

		bool bIsNestedIntoSkeleton = false;
		if (!CurrentDataContext.IsSkeletalMeshInstance(MeshInstance, BaseNodeContainer, bIsNestedIntoSkeleton))
		{
			continue;
		}
		bool bIsStaticMesh = !MeshInstance.bReferenceSkinnedMesh;
		//Find the root skeleton for this MeshInstance
		FString SkeletonRootUid; // = CurrentDataContext.FindSkeletalMeshSkeletonRootFromMeshInstance(MeshInstance, BaseNodeContainer);
		
		for (const FString& MeshGeometryUid : MeshInstance.ReferencingMeshGeometryUids)
		{
			if (const FString* SkeletonRootUidPtr = SkeletonRootUidPerMeshUid.Find(MeshGeometryUid))
			{
				if (SkeletonRootUid.IsEmpty())
				{
					SkeletonRootUid = *SkeletonRootUidPtr;
				}
				else if (!SkeletonRootUid.Equals(*SkeletonRootUidPtr))
				{
					//Log an error, this FInterchangeMeshInstance use more then one skeleton root node, we will not add this instance to the combined
					SkeletonRootUid.Empty();
					break;
				}
			}
			else if (bIsStaticMesh)
			{
				//Create a joint from the instance node (the scene node pointing on the mesh).
				SkeletonRootUid = MeshInstance.MeshInstanceUid;
				if (bIsNestedIntoSkeleton || !MeshInstance.bHasMorphTargets)
				{
					//Find the deepest joint node
					if (const UInterchangeSceneNode* SceneNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(MeshInstance.MeshInstanceUid)))
					{
						FString ParentUid = SceneNode->GetParentUid();
						FString LastSceneNodeUid = SkeletonRootUid;
						while (!ParentUid.Equals(UInterchangeBaseNode::InvalidNodeUid()))
						{
							if (const UInterchangeSceneNode* ParentNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(ParentUid)))
							{
								if(ParentNode->IsSpecializedTypeContains(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString()))
								{
									SkeletonRootUid = ParentUid;
								}
								LastSceneNodeUid = ParentUid;
								ParentUid = ParentNode->GetParentUid();
							}
						}
						//If we did not find any joint because we have non nested mesh, get the deepest scene node
						if (!bIsNestedIntoSkeleton && SkeletonRootUid.Equals(MeshInstance.MeshInstanceUid))
						{
							SkeletonRootUid = LastSceneNodeUid;
						}
					}
				}
			}
			else
			{
				//every skinned geometry should have a skeleton root node ???
			}
		}
		
		if (SkeletonRootUid.IsEmpty())
		{
			//Skip this MeshInstance
			continue;
		}
		TArray<FString>& MeshInstanceUids = OutMeshInstanceUidsPerSkeletonRootUid.FindOrAdd(SkeletonRootUid);
		MeshInstanceUids.Add(MeshInstanceUidAndMeshInstance.Key);
	}
}

FString UInterchangePipelineMeshesUtilities::GetMeshInstanceSkeletonRootUid(const FString& MeshInstanceUid) const
{
	FString SkeletonRootUid;
	if (IsValidMeshInstanceUid(MeshInstanceUid))
	{
		SkeletonRootUid = GetMeshInstanceSkeletonRootUid(GetMeshInstanceByUid(MeshInstanceUid));
	}
	return SkeletonRootUid;
}

FString UInterchangePipelineMeshesUtilities::GetMeshInstanceSkeletonRootUid(const FInterchangeMeshInstance& MeshInstance) const
{
	FString SkeletonRootUid;
	if (MeshInstance.SceneNodePerLodIndex.Num() == 0)
	{
		return SkeletonRootUid;
	}
	const int32 BaseLodIndex = 0;
	if (MeshInstance.SceneNodePerLodIndex[BaseLodIndex].SceneNodes.Num() > 0)
	{
		const UInterchangeSceneNode* SceneNode = MeshInstance.SceneNodePerLodIndex[BaseLodIndex].SceneNodes[0];
		FString MeshNodeUid;
		if (SceneNode->GetCustomAssetInstanceUid(MeshNodeUid))
		{
			if (SkeletonRootUidPerMeshUid.Contains(MeshNodeUid))
			{
				SkeletonRootUid = SkeletonRootUidPerMeshUid.FindChecked(MeshNodeUid);
			}
		}
	}
	return SkeletonRootUid;
}

FString UInterchangePipelineMeshesUtilities::GetMeshGeometrySkeletonRootUid(const FString& MeshGeometryUid) const
{
	FString SkeletonRootUid;
	if (IsValidMeshGeometryUid(MeshGeometryUid))
	{
		const FInterchangeMeshGeometry& MeshGeometry = GetMeshGeometryByUid(MeshGeometryUid);
		SkeletonRootUid = GetMeshGeometrySkeletonRootUid(MeshGeometry);
	}
	return SkeletonRootUid;
}

FString UInterchangePipelineMeshesUtilities::GetMeshGeometrySkeletonRootUid(const FInterchangeMeshGeometry& MeshGeometry) const
{
	FString SkeletonRootUid;
	if (SkeletonRootUidPerMeshUid.Contains(MeshGeometry.MeshUid))
	{
		SkeletonRootUid = SkeletonRootUidPerMeshUid.FindChecked(MeshGeometry.MeshUid);
	}
	return SkeletonRootUid;
}
