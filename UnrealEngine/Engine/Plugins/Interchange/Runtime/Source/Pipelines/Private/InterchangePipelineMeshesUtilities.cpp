// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangePipelineMeshesUtilities.h"

#include "CoreMinimal.h"
#include "Nodes/InterchangeBaseNode.h"
#include "InterchangeMeshNode.h"
#include "InterchangeSceneNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangePipelineMeshesUtilities)


static bool IsSceneNodeASocket(const UInterchangeSceneNode* SceneNode)
{
	// Generic pipeline determines where a scene node should be considered as a socket from its naming convention.
	// This is no longer decided by the translator.
	FString NodeDisplayName = SceneNode->GetDisplayLabel();
	return NodeDisplayName.StartsWith(TEXT("SOCKET_"));
}


UInterchangePipelineMeshesUtilities* UInterchangePipelineMeshesUtilities::CreateInterchangePipelineMeshesUtilities(UInterchangeBaseNodeContainer* BaseNodeContainer)
{
	check(BaseNodeContainer);
	UInterchangePipelineMeshesUtilities* PipelineMeshesUtilities = NewObject<UInterchangePipelineMeshesUtilities>(GetTransientPackage(), NAME_None);
	
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

	//Find all translated scene node we need for this pipeline
	bool bHasSockets = false;
	BaseNodeContainer->IterateNodes(
		[&PipelineMeshesUtilities, &BaseNodeContainer, &SkeletonRootNodeUids, &bHasSockets](const FString& NodeUid, const UInterchangeBaseNode* Node)
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
						const UInterchangeSceneNode* ParentMeshSceneNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(SceneNode->GetParentUid()));
						while (ParentMeshSceneNode)
						{
							if (ParentMeshSceneNode->GetCustomAssetInstanceUid(MeshUid))
							{
								break;
							}

							ParentMeshSceneNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(SceneNode->GetParentUid()));
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

void UInterchangePipelineMeshesUtilities::GetAllSkinnedMeshInstance(TArray<FString>& MeshInstanceUids, const bool bConvertStaticMeshToSkeletalMesh) const
{
	MeshInstanceUids.Empty(MeshInstancesPerMeshInstanceUid.Num());
	for (const TPair<FString, FInterchangeMeshInstance>& MeshInstanceUidAndMeshInstance : MeshInstancesPerMeshInstanceUid)
	{
		const FInterchangeMeshInstance& MeshInstance = MeshInstanceUidAndMeshInstance.Value;
		if((bConvertStaticMeshToSkeletalMesh || MeshInstance.bReferenceSkinnedMesh) && !MeshInstance.bReferenceMorphTarget)
		{
			MeshInstanceUids.Add(MeshInstance.MeshInstanceUid);
		}
	}
}

void UInterchangePipelineMeshesUtilities::IterateAllSkinnedMeshInstance(TFunctionRef<void(const FInterchangeMeshInstance&)> IterationLambda, const bool bConvertStaticMeshToSkeletalMesh) const
{
	for (const TPair<FString, FInterchangeMeshInstance>& MeshInstanceUidAndMeshInstance : MeshInstancesPerMeshInstanceUid)
	{
		const FInterchangeMeshInstance& MeshInstance = MeshInstanceUidAndMeshInstance.Value;
		if ((bConvertStaticMeshToSkeletalMesh || MeshInstance.bReferenceSkinnedMesh) && !MeshInstance.bReferenceMorphTarget)
		{
			IterationLambda(MeshInstance);
		}
	}
}

void UInterchangePipelineMeshesUtilities::GetAllStaticMeshInstance(TArray<FString>& MeshInstanceUids, const bool bConvertSkeletalMeshToStaticMesh) const
{
	MeshInstanceUids.Empty(MeshInstancesPerMeshInstanceUid.Num());
	for (const TPair<FString, FInterchangeMeshInstance>& MeshInstanceUidAndMeshInstance : MeshInstancesPerMeshInstanceUid)
	{
		const FInterchangeMeshInstance& MeshInstance = MeshInstanceUidAndMeshInstance.Value;
		if ((bConvertSkeletalMeshToStaticMesh || !MeshInstance.bReferenceSkinnedMesh) && !MeshInstance.bReferenceMorphTarget)
		{
			MeshInstanceUids.Add(MeshInstance.MeshInstanceUid);
		}
	}
}

void UInterchangePipelineMeshesUtilities::IterateAllStaticMeshInstance(TFunctionRef<void(const FInterchangeMeshInstance&)> IterationLambda, const bool bConvertSkeletalMeshToStaticMesh) const
{
	for (const TPair<FString, FInterchangeMeshInstance>& MeshInstanceUidAndMeshInstance : MeshInstancesPerMeshInstanceUid)
	{
		const FInterchangeMeshInstance& MeshInstance = MeshInstanceUidAndMeshInstance.Value;
		if ((bConvertSkeletalMeshToStaticMesh || !MeshInstance.bReferenceSkinnedMesh) && !MeshInstance.bReferenceMorphTarget)
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
		if (MeshGeometry.MeshNode->IsSkinnedMesh())
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
		if (MeshGeometry.MeshNode->IsSkinnedMesh())
		{
			IterationLambda(MeshGeometry);
		}
	}
}

void UInterchangePipelineMeshesUtilities::GetAllStaticMeshGeometry(TArray<FString>& MeshGeometryUids, const bool bConvertSkeletalMeshToStaticMesh) const
{
	MeshGeometryUids.Empty(MeshGeometriesPerMeshUid.Num());
	for (const TPair<FString, FInterchangeMeshGeometry>& MeshGeometryUidAndMeshGeometry : MeshGeometriesPerMeshUid)
	{
		const FInterchangeMeshGeometry& MeshGeometry = MeshGeometryUidAndMeshGeometry.Value;
		if ((bConvertSkeletalMeshToStaticMesh || !MeshGeometry.MeshNode->IsSkinnedMesh()) && !MeshGeometry.MeshNode->IsMorphTarget())
		{
			MeshGeometryUids.Add(MeshGeometry.MeshUid);
		}
	}
}

void UInterchangePipelineMeshesUtilities::IterateAllStaticMeshGeometry(TFunctionRef<void(const FInterchangeMeshGeometry&)> IterationLambda, const bool bConvertSkeletalMeshToStaticMesh) const
{
	for (const TPair<FString, FInterchangeMeshGeometry>& MeshGeometryUidAndMeshGeometry : MeshGeometriesPerMeshUid)
	{
		const FInterchangeMeshGeometry& MeshGeometry = MeshGeometryUidAndMeshGeometry.Value;
		if ((bConvertSkeletalMeshToStaticMesh || !MeshGeometry.MeshNode->IsSkinnedMesh()) && !MeshGeometry.MeshNode->IsMorphTarget())
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

void UInterchangePipelineMeshesUtilities::GetCombinedSkinnedMeshInstances(UInterchangeBaseNodeContainer* BaseNodeContainer, TMap<FString, TArray<FString>>& OutMeshInstanceUidsPerSkeletonRootUid, const bool bConvertStaticMeshToSkeletalMesh) const
{
	check(BaseNodeContainer);

	for (const TPair<FString, FInterchangeMeshInstance>& MeshInstanceUidAndMeshInstance : MeshInstancesPerMeshInstanceUid)
	{
		const FInterchangeMeshInstance& MeshInstance = MeshInstanceUidAndMeshInstance.Value;
		if (!bConvertStaticMeshToSkeletalMesh && !MeshInstance.bReferenceSkinnedMesh)
		{
			continue;
		}
		bool bIsStaticMesh = bConvertStaticMeshToSkeletalMesh && !MeshInstance.bReferenceSkinnedMesh;
		//Find the root skeleton for this MeshInstance
		FString SkeletonRootUid;
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
				//Since we are dealing with rigid mesh and combine we will use the outermost valid parent
				SkeletonRootUid = MeshInstance.MeshInstanceUid;
				if (const UInterchangeSceneNode* SceneNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(MeshInstance.MeshInstanceUid)))
				{
					FString ParentUid = SceneNode->GetParentUid();
					while (!ParentUid.Equals(UInterchangeBaseNode::InvalidNodeUid()))
					{
						if (const UInterchangeSceneNode* ParentNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(ParentUid)))
						{
							SkeletonRootUid = ParentUid;
							ParentUid = ParentNode->GetParentUid();
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

void UInterchangePipelineMeshesUtilities::GetCombinedSkinnedMeshGeometries(TMap<FString, TArray<FString>>& OutMeshGeometryUidsPerSkeletonRootUid) const
{
	for (const TPair<FString, FInterchangeMeshGeometry>& MeshGeometryUidAndMeshGeometry : MeshGeometriesPerMeshUid)
	{
		const FInterchangeMeshGeometry& MeshGeometry = MeshGeometryUidAndMeshGeometry.Value;
		if (!MeshGeometry.MeshNode || !MeshGeometry.MeshNode->IsSkinnedMesh())
		{
			continue;
		}
		//Find the root skeleton for this MeshInstance
		FString SkeletonRootUid;
		if (const FString* SkeletonRootUidPtr = SkeletonRootUidPerMeshUid.Find(MeshGeometryUidAndMeshGeometry.Key))
		{
			if (SkeletonRootUid.IsEmpty())
			{
				SkeletonRootUid = *SkeletonRootUidPtr;
			}
		}
		else
		{
			//every skinned geometry should have a skeleton root node ???
		}

		if (SkeletonRootUid.IsEmpty())
		{
			//Skip this MeshGeometry
			continue;
		}
		TArray<FString>& MeshGeometryUids = OutMeshGeometryUidsPerSkeletonRootUid.FindOrAdd(SkeletonRootUid);
		MeshGeometryUids.Add(MeshGeometryUidAndMeshGeometry.Key);
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
