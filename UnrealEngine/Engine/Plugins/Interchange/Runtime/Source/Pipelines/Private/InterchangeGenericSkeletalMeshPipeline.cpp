// Copyright Epic Games, Inc. All Rights Reserved. 
#include "InterchangeGenericMeshPipeline.h"

#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/Async.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "InterchangeGenericAssetsPipeline.h"
#include "InterchangeMaterialFactoryNode.h"
#include "InterchangeMeshNode.h"
#include "InterchangePhysicsAssetFactoryNode.h"
#include "InterchangePipelineLog.h"
#include "InterchangePipelineMeshesUtilities.h"
#include "InterchangeSceneNode.h"
#include "InterchangeSkeletalMeshFactoryNode.h"
#include "InterchangeSkeletalMeshLodDataNode.h"
#include "InterchangeSkeletonFactoryNode.h"
#include "InterchangeSkeletonHelper.h"
#include "InterchangeSourceData.h"
#include "Misc/Paths.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Nodes/InterchangeUserDefinedAttribute.h"
#if WITH_EDITOR
#include "PhysicsAssetUtils.h"
#endif //WITH_EDITOR
#include "PhysicsEngine/PhysicsAsset.h"
#include "ReferenceSkeleton.h"
#include "Tasks/Task.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

namespace UE::Interchange::SkeletalMeshGenericPipeline
{
	bool RecursiveFindChildUid(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FString& ParentUid, const FString& SearchUid)
	{
		if (ParentUid == SearchUid)
		{
			return true;
		}
		const int32 ChildCount = BaseNodeContainer->GetNodeChildrenCount(ParentUid);
		TArray<FString> Childrens = BaseNodeContainer->GetNodeChildrenUids(ParentUid);
		for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
		{
			if (RecursiveFindChildUid(BaseNodeContainer, Childrens[ChildIndex], SearchUid))
			{
				return true;
			}
		}
		return false;
	}

	void RemoveNestedMeshNodes(const UInterchangeBaseNodeContainer* BaseNodeContainer, const UInterchangeSkeletonFactoryNode* SkeletonFactoryNode, TArray<FString>& NodeUids)
	{
		if (!SkeletonFactoryNode)
		{
			return;
		}
		FString SkeletonRootJointUid;
		SkeletonFactoryNode->GetCustomRootJointUid(SkeletonRootJointUid);
		for (int32 NodeIndex = NodeUids.Num()-1; NodeIndex >= 0; NodeIndex--)
		{
			const FString& NodeUid = NodeUids[NodeIndex];
			if (RecursiveFindChildUid(BaseNodeContainer, SkeletonRootJointUid, NodeUid))
			{
				NodeUids.RemoveAt(NodeIndex);
			}
		}
	}
}

void UInterchangeGenericMeshPipeline::ExecutePreImportPipelineSkeletalMesh()
{
	LLM_SCOPE_BYNAME(TEXT("Interchange"));
	check(CommonMeshesProperties.IsValid());
	if (!bImportSkeletalMeshes)
	{
		//Nothing to import
		return;
	}

	if (CommonMeshesProperties->ForceAllMeshAsType != EInterchangeForceMeshType::IFMT_None && CommonMeshesProperties->ForceAllMeshAsType != EInterchangeForceMeshType::IFMT_SkeletalMesh)
	{
		//Nothing to import
		return;
	}

#if WITH_EDITOR
	//Make sure the generic pipeline we cover all skeletalmesh build settings by asserting when we import
	Async(EAsyncExecution::TaskGraphMainThread, []()
		{
			static bool bVerifyBuildProperties = false;
			if (!bVerifyBuildProperties)
			{
				bVerifyBuildProperties = true;
				TArray<const UClass*> Classes;
				Classes.Add(UInterchangeGenericCommonMeshesProperties::StaticClass());
				Classes.Add(UInterchangeGenericMeshPipeline::StaticClass());
				if (!DoClassesIncludeAllEditableStructProperties(Classes, FSkeletalMeshBuildSettings::StaticStruct()))
				{
					UE_LOG(LogInterchangePipeline, Log, TEXT("UInterchangeGenericMeshPipeline: The generic pipeline does not cover all skeletal mesh build options."));
				}
			}
		});
#endif

	TMap<FString, TArray<FString>> SkeletalMeshFactoryDependencyOrderPerSkeletonRootNodeUid;

	auto SetSkeletalMeshDependencies = [&SkeletalMeshFactoryDependencyOrderPerSkeletonRootNodeUid](const FString& JointNodeUid, UInterchangeSkeletalMeshFactoryNode* SkeletalMeshFactoryNode)
	{
		TArray<FString>& SkeletalMeshFactoryDependencyOrder = SkeletalMeshFactoryDependencyOrderPerSkeletonRootNodeUid.FindOrAdd(JointNodeUid);
		//Updating the skeleton is not multi thread safe, so we add dependency between skeletalmesh altering the same skeleton
		//TODO make the skeletalMesh ReferenceSkeleton thread safe to allow multiple parallel skeletalmesh factory on the same skeleton asset.
		int32 DependencyIndex = SkeletalMeshFactoryDependencyOrder.AddUnique(SkeletalMeshFactoryNode->GetUniqueID());
		if (DependencyIndex > 0)
		{
			const FString SkeletalMeshFactoryNodeDependencyUid = SkeletalMeshFactoryDependencyOrder[DependencyIndex - 1];
			SkeletalMeshFactoryNode->AddFactoryDependencyUid(SkeletalMeshFactoryNodeDependencyUid);
		}
	};

	if (bCombineSkeletalMeshes)
	{
		//////////////////////////////////////////////////////////////////////////
		//Combined everything we can
		TMap<FString, TArray<FString>> MeshUidsPerSkeletonRootUid;
		auto CreatePerSkeletonRootUidCombinedSkinnedMesh = [this, &MeshUidsPerSkeletonRootUid, &SetSkeletalMeshDependencies](const bool bUseInstanceMesh)
		{
#if !WITH_EDITOR || !WITH_EDITORONLY_DATA
				if (MeshUidsPerSkeletonRootUid.Num() > 0)
				{
					UE_LOG(LogInterchangePipeline, Warning, TEXT("Cannot import skeletalMesh asset in runtime, this is an editor only feature."));
				}
				return false;
#else
				if (MeshUidsPerSkeletonRootUid.Num() > 0)
				{
					if (FApp::IsGame())
					{
						//PIE
						UE_LOG(LogInterchangePipeline, Warning, TEXT("Cannot import skeletalMesh asset in runtime, this is an editor only feature."));
						return false;
					}
					else
					{
						bool bFoundInstances = false;
						for (const TPair<FString, TArray<FString>>& SkeletonRootUidAndMeshUids : MeshUidsPerSkeletonRootUid)
						{
							const FString& SkeletonRootUid = SkeletonRootUidAndMeshUids.Key;
							//Every iteration is a skeletalmesh asset that combine all MeshInstances sharing the same skeleton root node
							UInterchangeSkeletonFactoryNode* SkeletonFactoryNode = CommonSkeletalMeshesAndAnimationsProperties->CreateSkeletonFactoryNode(BaseNodeContainer, SkeletonRootUid);
							//The MeshUids can represent a SceneNode pointing on a MeshNode or directly a MeshNode;
							TMap<int32, TArray<FString>> MeshUidsPerLodIndex;
							const TArray<FString>& MeshUids = SkeletonRootUidAndMeshUids.Value;
							for (const FString& MeshUid : MeshUids)
							{
								if (bUseInstanceMesh)
								{
									const FInterchangeMeshInstance& MeshInstance = PipelineMeshesUtilities->GetMeshInstanceByUid(MeshUid);
									for (const TPair<int32, FInterchangeLodSceneNodeContainer>& LodIndexAndSceneNodeContainer : MeshInstance.SceneNodePerLodIndex)
									{
										const int32 LodIndex = LodIndexAndSceneNodeContainer.Key;
										const FInterchangeLodSceneNodeContainer& SceneNodeContainer = LodIndexAndSceneNodeContainer.Value;
										TArray<FString>& TranslatedNodes = MeshUidsPerLodIndex.FindOrAdd(LodIndex);
										for (const UInterchangeSceneNode* SceneNode : SceneNodeContainer.SceneNodes)
										{
											TranslatedNodes.Add(SceneNode->GetUniqueID());
										}
									}
								}
								else
								{
									//MeshGeometry cannot have Lod since LODs are define in the scene node
									const FInterchangeMeshGeometry& MeshGeometry = PipelineMeshesUtilities->GetMeshGeometryByUid(MeshUid);
									const int32 LodIndex = 0;
									TArray<FString>& TranslatedNodes = MeshUidsPerLodIndex.FindOrAdd(LodIndex);
									TranslatedNodes.Add(MeshGeometry.MeshUid);
								}
							}

							if (MeshUidsPerLodIndex.Num() > 0)
							{
								UInterchangeSkeletalMeshFactoryNode* SkeletalMeshFactoryNode = CreateSkeletalMeshFactoryNode(SkeletonRootUid, MeshUidsPerLodIndex);
								if (SkeletalMeshFactoryNode != nullptr)
								{
									SetSkeletalMeshDependencies(SkeletonRootUid, SkeletalMeshFactoryNode);
									SkeletonFactoryNodes.Add(SkeletonFactoryNode);
									SkeletalMeshFactoryNodes.Add(SkeletalMeshFactoryNode);
									bFoundInstances = true;
								}
							}
						}
						return bFoundInstances;
					}
				}
				return false;
#endif
		};


		PipelineMeshesUtilities->GetCombinedSkinnedMeshInstances(MeshUidsPerSkeletonRootUid);
		bool bUseMeshInstance = true;
		CreatePerSkeletonRootUidCombinedSkinnedMesh(bUseMeshInstance);
	}
	else
	{
		//////////////////////////////////////////////////////////////////////////
		//Do not combined meshes
		TArray<FString> MeshUids;
		auto CreatePerSkeletonRootUidSkinnedMesh = [this, &MeshUids, &SetSkeletalMeshDependencies](const bool bUseInstanceMesh)
		{
#if !WITH_EDITOR || !WITH_EDITORONLY_DATA
			if (MeshUids.Num() > 0)
			{
				UE_LOG(LogInterchangePipeline, Warning, TEXT("Cannot import skeletalMesh asset in runtime, this is an editor only feature."));
			}
			return false;
#else
			if (MeshUids.Num() > 0)
			{
				if (FApp::IsGame())
				{
					//PIE
					UE_LOG(LogInterchangePipeline, Warning, TEXT("Cannot import skeletalMesh asset in runtime, this is an editor only feature."));
					return false;
				}
				else
				{
					bool bFoundInstances = false;
					for (const FString& MeshUid : MeshUids)
					{
						//Every iteration is a skeletalmesh asset that combine all MeshInstances sharing the same skeleton root node
						//The MeshUids can represent a SceneNode pointing on a MeshNode or directly a MeshNode;
						TMap<int32, TArray<FString>> MeshUidsPerLodIndex;
						FString SkeletonRootUid;
						if (!(bUseInstanceMesh ? PipelineMeshesUtilities->IsValidMeshInstanceUid(MeshUid) : PipelineMeshesUtilities->IsValidMeshGeometryUid(MeshUid)))
						{
							continue;
						}
						SkeletonRootUid = (bUseInstanceMesh ? PipelineMeshesUtilities->GetMeshInstanceSkeletonRootUid(MeshUid) : PipelineMeshesUtilities->GetMeshGeometrySkeletonRootUid(MeshUid));
						if (SkeletonRootUid.IsEmpty())
						{
							if (CommonSkeletalMeshesAndAnimationsProperties->bConvertStaticsWithMorphTargetsToSkeletals)
							{
								SkeletonRootUid = MeshUid;
							}
							else
							{
								//Log an error
								continue;
							}
						}
						UInterchangeSkeletonFactoryNode* SkeletonFactoryNode = CommonSkeletalMeshesAndAnimationsProperties->CreateSkeletonFactoryNode(BaseNodeContainer, SkeletonRootUid);
						if (bUseInstanceMesh)
						{
							const FInterchangeMeshInstance& MeshInstance = PipelineMeshesUtilities->GetMeshInstanceByUid(MeshUid);
							for (const TPair<int32, FInterchangeLodSceneNodeContainer>& LodIndexAndSceneNodeContainer : MeshInstance.SceneNodePerLodIndex)
							{
								const int32 LodIndex = LodIndexAndSceneNodeContainer.Key;
								const FInterchangeLodSceneNodeContainer& SceneNodeContainer = LodIndexAndSceneNodeContainer.Value;
								TArray<FString>& TranslatedNodes = MeshUidsPerLodIndex.FindOrAdd(LodIndex);
								for (const UInterchangeSceneNode* SceneNode : SceneNodeContainer.SceneNodes)
								{
									TranslatedNodes.Add(SceneNode->GetUniqueID());
								}
							}
						}
						else
						{
							const FInterchangeMeshGeometry& MeshGeometry = PipelineMeshesUtilities->GetMeshGeometryByUid(MeshUid);
							const int32 LodIndex = 0;
							TArray<FString>& TranslatedNodes = MeshUidsPerLodIndex.FindOrAdd(LodIndex);
							TranslatedNodes.Add(MeshGeometry.MeshUid);
						}
						if (MeshUidsPerLodIndex.Num() > 0)
						{
							UInterchangeSkeletalMeshFactoryNode* SkeletalMeshFactoryNode = CreateSkeletalMeshFactoryNode(SkeletonRootUid, MeshUidsPerLodIndex);
							if (SkeletalMeshFactoryNode != nullptr)
							{
								SetSkeletalMeshDependencies(SkeletonRootUid, SkeletalMeshFactoryNode);
								SkeletonFactoryNodes.Add(SkeletonFactoryNode);
								SkeletalMeshFactoryNodes.Add(SkeletalMeshFactoryNode);
								bFoundInstances = true;
							}

						}
					}
					return bFoundInstances;
				}
			}
			return false;
#endif
		};
		
		PipelineMeshesUtilities->GetAllSkinnedMeshInstance(MeshUids);
		bool bUseMeshInstance = true;
		CreatePerSkeletonRootUidSkinnedMesh(bUseMeshInstance);
	}
}

UInterchangeSkeletalMeshFactoryNode* UInterchangeGenericMeshPipeline::CreateSkeletalMeshFactoryNode(const FString& RootJointUid, const TMap<int32, TArray<FString>>& MeshUidsPerLodIndex)
{
	check(CommonMeshesProperties.IsValid());
	check(CommonSkeletalMeshesAndAnimationsProperties.IsValid());
	//Get the skeleton factory node
	const UInterchangeBaseNode* RootJointNode = BaseNodeContainer->GetNode(RootJointUid);
	if (!RootJointNode)
	{
		return nullptr;
	}
	const FString SkeletonUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(RootJointNode->GetUniqueID());
	UInterchangeSkeletonFactoryNode* SkeletonFactoryNode = Cast<UInterchangeSkeletonFactoryNode>(BaseNodeContainer->GetFactoryNode(SkeletonUid));
	if (!ensure(SkeletonFactoryNode))
	{
		//Log an error
		return nullptr;
	}
	
	if (MeshUidsPerLodIndex.Num() == 0)
	{
		return nullptr;
	}
	
	auto GetFirstNodeInfo = [this, &MeshUidsPerLodIndex](const int32 Index, FString& OutFirstMeshNodeUid, int32& OutSceneNodeCount)->const UInterchangeBaseNode*
	{
		OutSceneNodeCount = 0;
		if (!ensure(Index >= 0 && MeshUidsPerLodIndex.Num() > Index))
		{
			//Log an error
			return nullptr;
		}
		for (const TPair<int32, TArray<FString>>& LodIndexAndMeshUids : MeshUidsPerLodIndex)
		{
			if (Index == LodIndexAndMeshUids.Key)
			{
				const TArray<FString>& MeshUids = LodIndexAndMeshUids.Value;
				if (MeshUids.Num() > 0)
				{
					const FString& MeshUid = MeshUids[0];
					const UInterchangeMeshNode* MeshNode = Cast<const UInterchangeMeshNode>(BaseNodeContainer->GetNode(MeshUid));
					if (MeshNode)
					{
						OutFirstMeshNodeUid = MeshUid;
						return MeshNode;
					}
					const UInterchangeSceneNode* SceneNode = Cast<const UInterchangeSceneNode>(BaseNodeContainer->GetNode(MeshUid));
					if (SceneNode)
					{
						FString MeshNodeUid;
						if (SceneNode->GetCustomAssetInstanceUid(MeshNodeUid))
						{
							OutSceneNodeCount = MeshUids.Num();
							OutFirstMeshNodeUid = MeshNodeUid;
							return SceneNode;
						}
					}
				}
				//We found the lod but there is no valid Mesh node to return the Uid
				break;
			}
		}
		return nullptr;
	};

	FString FirstMeshNodeUid;
	const int32 BaseLodIndex = 0;
	int32 SceneNodeCount = 0;
	const UInterchangeBaseNode* InterchangeBaseNode = GetFirstNodeInfo(BaseLodIndex, FirstMeshNodeUid, SceneNodeCount);
	if (!InterchangeBaseNode)
	{
		//Log an error
		return nullptr;
	}
	const UInterchangeSceneNode* FirstSceneNode = Cast<UInterchangeSceneNode>(InterchangeBaseNode);
	const UInterchangeMeshNode* FirstMeshNode = Cast<UInterchangeMeshNode>(BaseNodeContainer->GetNode(FirstMeshNodeUid));

	//Create the skeletal mesh factory node, name it according to the first mesh node compositing the meshes
	FString DisplayLabel = FirstMeshNode->GetDisplayLabel();
	FString SkeletalMeshUid_MeshNamePart = FirstMeshNodeUid;
	if(FirstSceneNode)
	{
		//If we are instancing one scene node, we want to use it to name the mesh
		if (SceneNodeCount == 1)
		{
			DisplayLabel = FirstSceneNode->GetDisplayLabel();
		}
		//Use the first scene node uid this skeletalmesh reference, add backslash since this uid is not asset typed (\\Mesh\\) like FirstMeshNodeUid
		SkeletalMeshUid_MeshNamePart = TEXT("\\") + FirstSceneNode->GetUniqueID();
	}
	const FString SkeletalMeshUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(SkeletalMeshUid_MeshNamePart + SkeletonUid);
	UInterchangeSkeletalMeshFactoryNode* SkeletalMeshFactoryNode = NewObject<UInterchangeSkeletalMeshFactoryNode>(BaseNodeContainer, NAME_None);
	if (!ensure(SkeletalMeshFactoryNode))
	{
		return nullptr;
	}
	SkeletalMeshFactoryNode->InitializeSkeletalMeshNode(SkeletalMeshUid, DisplayLabel, USkeletalMesh::StaticClass()->GetName());
	SkeletalMeshFactoryNode->AddFactoryDependencyUid(SkeletonUid);
	BaseNodeContainer->AddNode(SkeletalMeshFactoryNode);

	if (CommonMeshesProperties->bKeepSectionsSeparate)
	{
		SkeletalMeshFactoryNode->SetCustomKeepSectionsSeparate(CommonMeshesProperties->bKeepSectionsSeparate);
	}

	SkeletonFactoryNode->SetCustomSkeletalMeshFactoryNodeUid(SkeletalMeshFactoryNode->GetUniqueID());

	AddLodDataToSkeletalMesh(SkeletonFactoryNode, SkeletalMeshFactoryNode, MeshUidsPerLodIndex);
	SkeletalMeshFactoryNode->SetCustomImportMorphTarget(bImportMorphTargets);
	SkeletalMeshFactoryNode->SetCustomImportVertexAttributes(bImportVertexAttributes);

	SkeletalMeshFactoryNode->SetCustomImportContentType(SkeletalMeshImportContentType);

	//If we have a specified skeleton
	if (CommonSkeletalMeshesAndAnimationsProperties->Skeleton.IsValid())
	{
		bool bSkeletonCompatible = false;

		//TODO: support skeleton helper in runtime
#if WITH_EDITOR
		bSkeletonCompatible = UE::Interchange::Private::FSkeletonHelper::IsCompatibleSkeleton(CommonSkeletalMeshesAndAnimationsProperties->Skeleton.Get(), RootJointNode->GetUniqueID(), BaseNodeContainer, CommonSkeletalMeshesAndAnimationsProperties->bConvertStaticsWithMorphTargetsToSkeletals || CommonMeshesProperties->ForceAllMeshAsType == EInterchangeForceMeshType::IFMT_SkeletalMesh);
#endif
		if (bSkeletonCompatible)
		{
			FSoftObjectPath SkeletonSoftObjectPath(CommonSkeletalMeshesAndAnimationsProperties->Skeleton.Get());
			SkeletalMeshFactoryNode->SetCustomSkeletonSoftObjectPath(SkeletonSoftObjectPath);
		}
		else
		{
			UInterchangeResultDisplay_Generic* Message = AddMessage<UInterchangeResultDisplay_Generic>();
			Message->Text = FText::Format(NSLOCTEXT("UInterchangeGenericMeshPipeline", "IncompatibleSkeleton", "Incompatible skeleton {0} when importing skeletalmesh {1}."),
				FText::FromString(CommonSkeletalMeshesAndAnimationsProperties->Skeleton->GetName()),
				FText::FromString(DisplayLabel));
		}
	}

	//Physic asset dependency, if we must create or use a specialize physic asset let create
	//a PhysicsAsset factory node, so the asset will exist when we will setup the skeletalmesh
	if (bCreatePhysicsAsset)
	{
		UInterchangePhysicsAssetFactoryNode* PhysicsAssetFactoryNode = NewObject<UInterchangePhysicsAssetFactoryNode>(BaseNodeContainer, NAME_None);
		if (ensure(SkeletalMeshFactoryNode))
		{
			const FString PhysicsAssetUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(SkeletalMeshUid_MeshNamePart + SkeletonUid + TEXT("_PhysicsAsset"));
			const FString PhysicsAssetDisplayLabel = DisplayLabel + TEXT("_PhysicsAsset");
			PhysicsAssetFactoryNode->InitializePhysicsAssetNode(PhysicsAssetUid, PhysicsAssetDisplayLabel, UPhysicsAsset::StaticClass()->GetName());
			PhysicsAssetFactoryNode->SetCustomSkeletalMeshUid(SkeletalMeshUid);
			BaseNodeContainer->AddNode(PhysicsAssetFactoryNode);
		}
	}
	SkeletalMeshFactoryNode->SetCustomCreatePhysicsAsset(bCreatePhysicsAsset);
	if (!bCreatePhysicsAsset && PhysicsAsset.IsValid())
	{
		FSoftObjectPath PhysicSoftObjectPath(PhysicsAsset.Get());
		SkeletalMeshFactoryNode->SetCustomPhysicAssetSoftObjectPath(PhysicSoftObjectPath);
	}

	const bool bTrueValue = true;
	switch (CommonMeshesProperties->VertexColorImportOption)
	{
		case EInterchangeVertexColorImportOption::IVCIO_Replace:
		{
			SkeletalMeshFactoryNode->SetCustomVertexColorReplace(bTrueValue);
		}
		break;
		case EInterchangeVertexColorImportOption::IVCIO_Ignore:
		{
			SkeletalMeshFactoryNode->SetCustomVertexColorIgnore(bTrueValue);
		}
		break;
		case EInterchangeVertexColorImportOption::IVCIO_Override:
		{
			SkeletalMeshFactoryNode->SetCustomVertexColorOverride(CommonMeshesProperties->VertexOverrideColor);
		}
		break;
	}

	//Avoid importing skeletalmesh if we want to only import animation
	if (CommonSkeletalMeshesAndAnimationsProperties->bImportOnlyAnimations)
	{
		SkeletonFactoryNode->SetEnabled(false);
		SkeletalMeshFactoryNode->SetEnabled(false);
	}

	//Common meshes build options
	SkeletalMeshFactoryNode->SetCustomRecomputeNormals(CommonMeshesProperties->bRecomputeNormals);
	SkeletalMeshFactoryNode->SetCustomRecomputeTangents(CommonMeshesProperties->bRecomputeTangents);
	SkeletalMeshFactoryNode->SetCustomUseMikkTSpace(CommonMeshesProperties->bUseMikkTSpace);
	SkeletalMeshFactoryNode->SetCustomComputeWeightedNormals(CommonMeshesProperties->bComputeWeightedNormals);
	SkeletalMeshFactoryNode->SetCustomUseHighPrecisionTangentBasis(CommonMeshesProperties->bUseHighPrecisionTangentBasis);
	SkeletalMeshFactoryNode->SetCustomUseFullPrecisionUVs(CommonMeshesProperties->bUseFullPrecisionUVs);
	SkeletalMeshFactoryNode->SetCustomUseBackwardsCompatibleF16TruncUVs(CommonMeshesProperties->bUseBackwardsCompatibleF16TruncUVs);
	SkeletalMeshFactoryNode->SetCustomRemoveDegenerates(CommonMeshesProperties->bRemoveDegenerates);
	//Skeletal meshes build options
	SkeletalMeshFactoryNode->SetCustomUseHighPrecisionSkinWeights(bUseHighPrecisionSkinWeights);
	SkeletalMeshFactoryNode->SetCustomThresholdPosition(ThresholdPosition);
	SkeletalMeshFactoryNode->SetCustomThresholdTangentNormal(ThresholdTangentNormal);
	SkeletalMeshFactoryNode->SetCustomThresholdUV(ThresholdUV);
	SkeletalMeshFactoryNode->SetCustomMorphThresholdPosition(MorphThresholdPosition);
	SkeletalMeshFactoryNode->SetCustomBoneInfluenceLimit(BoneInfluenceLimit);

	return SkeletalMeshFactoryNode;
}

UInterchangeSkeletalMeshLodDataNode* UInterchangeGenericMeshPipeline::CreateSkeletalMeshLodDataNode(const FString& NodeName, const FString& NodeUniqueID)
{
	FString DisplayLabel(NodeName);
	FString NodeUID(NodeUniqueID);
	UInterchangeSkeletalMeshLodDataNode* SkeletalMeshLodDataNode = NewObject<UInterchangeSkeletalMeshLodDataNode>(BaseNodeContainer, NAME_None);
	if (!ensure(SkeletalMeshLodDataNode))
	{
		//TODO log error
		return nullptr;
	}
	// Creating a UMaterialInterface
	SkeletalMeshLodDataNode->InitializeNode(NodeUID, DisplayLabel, EInterchangeNodeContainerType::FactoryData);
	BaseNodeContainer->AddNode(SkeletalMeshLodDataNode);
	return SkeletalMeshLodDataNode;
}

void UInterchangeGenericMeshPipeline::AddLodDataToSkeletalMesh(const UInterchangeSkeletonFactoryNode* SkeletonFactoryNode, UInterchangeSkeletalMeshFactoryNode* SkeletalMeshFactoryNode, const TMap<int32, TArray<FString>>& NodeUidsPerLodIndex)
{
	check(CommonMeshesProperties.IsValid());
	check(CommonSkeletalMeshesAndAnimationsProperties.IsValid());

	const FString SkeletalMeshUid = SkeletalMeshFactoryNode->GetUniqueID();
	const FString SkeletonUid = SkeletonFactoryNode->GetUniqueID();
	const int32 LodCount = NodeUidsPerLodIndex.Num();
	for (int32 LodIndex = 0; LodIndex < LodCount; ++LodIndex)
	{
		if (!CommonMeshesProperties->bImportLods && LodIndex > 0)
		{
			//If the pipeline should not import lods, skip any lod over base lod
			continue;
		}

		//Copy the nodes unique id because we need to remove nested mesh if the option is to not import them
		TArray<FString> NodeUids = NodeUidsPerLodIndex.FindChecked(LodIndex);
		if (!CommonSkeletalMeshesAndAnimationsProperties->bImportMeshesInBoneHierarchy)
		{
			UE::Interchange::SkeletalMeshGenericPipeline::RemoveNestedMeshNodes(BaseNodeContainer, SkeletonFactoryNode, NodeUids);
		}

		//Create a lod data node with all the meshes for this LOD
		const FString SkeletalMeshLodDataName = TEXT("LodData") + FString::FromInt(LodIndex);
		const FString LODDataPrefix = TEXT("\\LodData") + (LodIndex > 0 ? FString::FromInt(LodIndex) : TEXT(""));
		const FString SkeletalMeshLodDataUniqueID = LODDataPrefix + SkeletalMeshUid + SkeletonUid;
		//The LodData already exist
		UInterchangeSkeletalMeshLodDataNode* LodDataNode = Cast<UInterchangeSkeletalMeshLodDataNode>(BaseNodeContainer->GetFactoryNode(SkeletalMeshLodDataUniqueID));
		if (!LodDataNode)
		{
			//Add the data for the LOD (skeleton Unique ID and all the mesh node fbx path, so we can find them when we will create the payload data)
			LodDataNode = CreateSkeletalMeshLodDataNode(SkeletalMeshLodDataName, SkeletalMeshLodDataUniqueID);
			BaseNodeContainer->SetNodeParentUid(SkeletalMeshLodDataUniqueID, SkeletalMeshUid);
			LodDataNode->SetCustomSkeletonUid(SkeletonUid);
			SkeletalMeshFactoryNode->AddLodDataUniqueId(SkeletalMeshLodDataUniqueID);
		}
		TMap<FString, FString> ExistingLodSlotMaterialDependencies;
		constexpr bool bAddSourceNodeName = true;
		for (const FString& NodeUid : NodeUids)
		{
			TMap<FString, FString> SlotMaterialDependencies;
			if (const UInterchangeSceneNode* SceneNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(NodeUid)))
			{
				FString MeshDependency;
				SceneNode->GetCustomAssetInstanceUid(MeshDependency);
				if (BaseNodeContainer->IsNodeUidValid(MeshDependency))
				{
					const UInterchangeMeshNode* MeshDependencyNode = Cast<UInterchangeMeshNode>(BaseNodeContainer->GetNode(MeshDependency));
					UInterchangeUserDefinedAttributesAPI::DuplicateAllUserDefinedAttribute(MeshDependencyNode, SkeletalMeshFactoryNode, bAddSourceNodeName);
					SkeletalMeshFactoryNode->AddTargetNodeUid(MeshDependency);
					MeshDependencyNode->AddTargetNodeUid(SkeletalMeshFactoryNode->GetUniqueID());
					
					MeshDependencyNode->GetSlotMaterialDependencies(SlotMaterialDependencies);
				}
				else
				{
					SceneNode->GetSlotMaterialDependencies(SlotMaterialDependencies);
				}
				UInterchangeUserDefinedAttributesAPI::DuplicateAllUserDefinedAttribute(SceneNode, SkeletalMeshFactoryNode, bAddSourceNodeName);
			}
			else if (const UInterchangeMeshNode* MeshNode = Cast<UInterchangeMeshNode>(BaseNodeContainer->GetNode(NodeUid)))
			{
				UInterchangeUserDefinedAttributesAPI::DuplicateAllUserDefinedAttribute(MeshNode, SkeletalMeshFactoryNode, bAddSourceNodeName);
				SkeletalMeshFactoryNode->AddTargetNodeUid(NodeUid);
				MeshNode->AddTargetNodeUid(SkeletalMeshFactoryNode->GetUniqueID());

				MeshNode->GetSlotMaterialDependencies(SlotMaterialDependencies);
			}

			UE::Interchange::MeshesUtilities::ApplySlotMaterialDependencies(*SkeletalMeshFactoryNode, SlotMaterialDependencies, *BaseNodeContainer, &ExistingLodSlotMaterialDependencies);
			LodDataNode->AddMeshUid(NodeUid);
		}
		UE::Interchange::MeshesUtilities::ReorderSlotMaterialDependencies(*SkeletalMeshFactoryNode, *BaseNodeContainer);
	}
}

void UInterchangeGenericMeshPipeline::PostImportSkeletalMesh(UObject* CreatedAsset, const UInterchangeFactoryBaseNode* FactoryNode)
{
	check(CommonSkeletalMeshesAndAnimationsProperties.IsValid());

	if (!BaseNodeContainer)
	{
		return;
	}

	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(CreatedAsset);
	if (!SkeletalMesh)
	{
		return;
	}

	//If we import only the geometry we do not want to update the skeleton reference pose.
	const bool bImportGeometryOnlyContent = SkeletalMeshImportContentType == EInterchangeSkeletalMeshContentType::Geometry;
	if (!bImportGeometryOnlyContent && bUpdateSkeletonReferencePose && CommonSkeletalMeshesAndAnimationsProperties->Skeleton.IsValid() && SkeletalMesh->GetSkeleton() == CommonSkeletalMeshesAndAnimationsProperties->Skeleton.Get())
	{
		SkeletalMesh->GetSkeleton()->UpdateReferencePoseFromMesh(SkeletalMesh);
		//TODO: notify editor the skeleton has change
	}
}

void UInterchangeGenericMeshPipeline::PostImportPhysicsAssetImport(UObject* CreatedAsset, const UInterchangeFactoryBaseNode* FactoryNode)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UInterchangeGenericMeshPipeline::PostImportPhysicsAssetImport);
	LLM_SCOPE_BYNAME(TEXT("Interchange"));
#if WITH_EDITOR
	if (!bCreatePhysicsAsset || !BaseNodeContainer)
	{
		return;
	}

	UPhysicsAsset* CreatedPhysicsAsset = Cast<UPhysicsAsset>(CreatedAsset);
	if (!CreatedPhysicsAsset)
	{
		return;
	}
	if (const UInterchangePhysicsAssetFactoryNode* PhysicsAssetFactoryNode = Cast<const UInterchangePhysicsAssetFactoryNode>(FactoryNode))
	{
		FString SkeletalMeshFactoryNodeUid;
		if (PhysicsAssetFactoryNode->GetCustomSkeletalMeshUid(SkeletalMeshFactoryNodeUid))
		{
			if (const UInterchangeSkeletalMeshFactoryNode* SkeletalMeshFactoryNode = Cast<const UInterchangeSkeletalMeshFactoryNode>(BaseNodeContainer->GetFactoryNode(SkeletalMeshFactoryNodeUid)))
			{
				FSoftObjectPath ReferenceObject;
				SkeletalMeshFactoryNode->GetCustomReferenceObject(ReferenceObject);
				if (ReferenceObject.IsValid())
				{
					if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(ReferenceObject.TryLoad()))
					{
						auto CreateFromSkeletalMeshLambda = [CreatedPhysicsAsset, SkeletalMesh]()
						{
							TRACE_CPUPROFILER_EVENT_SCOPE(UInterchangeGenericMeshPipeline::PostImportPhysicsAssetImport::CreateFromSkeletalMeshLambda);
							FPhysAssetCreateParams NewBodyData;
							FText CreationErrorMessage;
							constexpr bool bSetToMesh = true;
							constexpr bool bShowProgress = false;
							if (!FPhysicsAssetUtils::CreateFromSkeletalMesh(CreatedPhysicsAsset, SkeletalMesh, NewBodyData, CreationErrorMessage, bSetToMesh, bShowProgress))
							{
								//TODO: Log an error
							}
						};

						if (!IsInGameThread() && SkeletalMesh->IsCompiling())
						{
							//If the skeletalmesh is compiling we have to stall on the main thread
							Async(EAsyncExecution::TaskGraphMainThread, [CreateFromSkeletalMeshLambda]()
							{
								CreateFromSkeletalMeshLambda();
							});
						}
						else
						{
							CreateFromSkeletalMeshLambda();
						}
					}
				}
			}
		}
	}
#endif //WITH_EDITOR
}

void UInterchangeGenericMeshPipeline::ImplementUseSourceNameForAssetOptionSkeletalMesh(const int32 MeshesImportedNodeCount, const bool bUseSourceNameForAsset, const FString& AssetName)
{
	check(CommonSkeletalMeshesAndAnimationsProperties.IsValid());

	const UClass* SkeletalMeshFactoryNodeClass = UInterchangeSkeletalMeshFactoryNode::StaticClass();
	TArray<FString> SkeletalMeshNodeUids;
	BaseNodeContainer->GetNodes(SkeletalMeshFactoryNodeClass, SkeletalMeshNodeUids);
	if (SkeletalMeshNodeUids.Num() == 0)
	{
		return;
	}
	//If we import only one asset, and bUseSourceNameForAsset is true, we want to rename the asset using the file name.
	const bool bShouldChangeAssetName = ((bUseSourceNameForAsset || !AssetName.IsEmpty()) && MeshesImportedNodeCount == 1);
	const FString SkeletalMeshUid = SkeletalMeshNodeUids[0];
	UInterchangeSkeletalMeshFactoryNode* SkeletalMeshNode = Cast<UInterchangeSkeletalMeshFactoryNode>(BaseNodeContainer->GetFactoryNode(SkeletalMeshUid));
	if (!SkeletalMeshNode)
	{
		return;
	}

	FString DisplayLabelName = SkeletalMeshNode->GetDisplayLabel();
		
	if (bShouldChangeAssetName)
	{
		DisplayLabelName = AssetName.IsEmpty() ? FPaths::GetBaseFilename(SourceDatas[0]->GetFilename()) : AssetName;
		SkeletalMeshNode->SetDisplayLabel(DisplayLabelName);
	}

	//Also set the skeleton factory node name
	TArray<FString> LodDataUids;
	SkeletalMeshNode->GetLodDataUniqueIds(LodDataUids);
	if (LodDataUids.Num() > 0)
	{
		//Get the skeleton from the base LOD, skeleton is shared with all LODs
		if (const UInterchangeSkeletalMeshLodDataNode* SkeletalMeshLodDataNode = Cast<const UInterchangeSkeletalMeshLodDataNode>(BaseNodeContainer->GetFactoryNode(LodDataUids[0])))
		{
			//If the user did not specify any skeleton
			if (!CommonSkeletalMeshesAndAnimationsProperties->Skeleton.IsValid())
			{
				FString SkeletalMeshSkeletonUid;
				SkeletalMeshLodDataNode->GetCustomSkeletonUid(SkeletalMeshSkeletonUid);
				UInterchangeSkeletonFactoryNode* SkeletonFactoryNode = Cast<UInterchangeSkeletonFactoryNode>(BaseNodeContainer->GetFactoryNode(SkeletalMeshSkeletonUid));
				if (SkeletonFactoryNode)
				{
					const FString SkeletonName = DisplayLabelName + TEXT("_Skeleton");
					SkeletonFactoryNode->SetDisplayLabel(SkeletonName);
				}
			}
		}
	}
	const UClass* PhysicsAssetFactoryNodeClass = UInterchangePhysicsAssetFactoryNode::StaticClass();
	TArray<FString> PhysicsAssetNodeUids;
	BaseNodeContainer->GetNodes(PhysicsAssetFactoryNodeClass, PhysicsAssetNodeUids);
	for (const FString& PhysicsAssetNodeUid : PhysicsAssetNodeUids)
	{
		UInterchangePhysicsAssetFactoryNode* PhysicsAssetFactoryNode = Cast<UInterchangePhysicsAssetFactoryNode>(BaseNodeContainer->GetFactoryNode(PhysicsAssetNodeUid));
		if (!ensure(PhysicsAssetFactoryNode))
		{
			continue;
		}
		FString PhysicsAssetSkeletalMeshUid;
		if (PhysicsAssetFactoryNode->GetCustomSkeletalMeshUid(PhysicsAssetSkeletalMeshUid) && PhysicsAssetSkeletalMeshUid.Equals(SkeletalMeshUid))
		{
			//Rename this asset
			const FString PhysicsAssetName = DisplayLabelName + TEXT("_PhysicsAsset");
			PhysicsAssetFactoryNode->SetDisplayLabel(PhysicsAssetName);
		}
	}
}