// Copyright Epic Games, Inc. All Rights Reserved. 

#include "InterchangeGenericScenesPipeline.h"

#include "InterchangeActorFactoryNode.h"
#include "InterchangeAssetImportData.h"
#include "InterchangeCameraNode.h"
#include "InterchangeCameraFactoryNode.h"
#include "InterchangeCommonPipelineDataFactoryNode.h"
#include "InterchangeSceneImportAsset.h"
#include "InterchangeSceneImportAssetFactoryNode.h"
#include "InterchangeSkeletalMeshLodDataNode.h"
#include "InterchangeLightNode.h"
#include "InterchangeLightFactoryNode.h"
#include "InterchangeMeshActorFactoryNode.h"
#include "InterchangeMeshNode.h"
#include "InterchangeDecalActorFactoryNode.h"
#include "InterchangeDecalNode.h"
#include "InterchangePipelineLog.h"
#include "InterchangePipelineMeshesUtilities.h"
#include "InterchangeSceneNode.h"
#include "InterchangeSceneVariantSetsFactoryNode.h"
#include "InterchangeVariantSetNode.h"
#include "Nodes/InterchangeUserDefinedAttribute.h"
#include "InterchangeSkeletonFactoryNode.h"
#include "InterchangeSkeletalMeshFactoryNode.h"

#include "Animation/SkeletalMeshActor.h"
#include "CineCameraActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/RectLight.h"
#include "Engine/SpotLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"

#if WITH_EDITOR
#include "ObjectTools.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeGenericScenesPipeline)

#define COPY_FROM_TRANSLATED_TO_FACTORY(TranslatedNode, FactoryNode, AttributeName, AttributeType) \
if(AttributeType AttributeName; TranslatedNode->GetCustom##AttributeName(AttributeName)) \
{ \
	FactoryNode->SetCustom##AttributeName(AttributeName); \
}

namespace UE::Interchange::Private
{
	TArray<FString> GetAllActiveJoints(UInterchangeBaseNodeContainer* BaseNodeContainer)
	{
		TArray<FString> AllActiveJoints;
		BaseNodeContainer->IterateNodesOfType<UInterchangeSkeletonFactoryNode>([&BaseNodeContainer, &AllActiveJoints](const FString& NodeUid, UInterchangeSkeletonFactoryNode* Node)
			{
				FString RootNodeUid;
				if (Node->GetCustomRootJointUid(RootNodeUid))
				{
					AllActiveJoints.Add(RootNodeUid);
					BaseNodeContainer->IterateNodeChildren(RootNodeUid, [&AllActiveJoints](const UInterchangeBaseNode* Node)
						{
							if (const UInterchangeSceneNode* SceneNode = Cast<UInterchangeSceneNode>(Node))
							{
								TArray<FString> SpecializeTypes;
								SceneNode->GetSpecializedTypes(SpecializeTypes);
								if (SpecializeTypes.Contains(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString()))
								{
									AllActiveJoints.Add(Node->GetUniqueID());
								}
							}
						});
				}
			});
		return AllActiveJoints;
	}

	//Either a (TransformSpecialized || !JointSpecialized || RootJoint) can be a parent (only those get FactoryNodes) :
	FString FindFactoryParentSceneNodeUid(UInterchangeBaseNodeContainer* BaseNodeContainer, TArray<FString>& ActiveSkeletonUids, const UInterchangeSceneNode* SceneNode)
	{
		FString ParentUid = SceneNode->GetParentUid();
		if (const UInterchangeSceneNode* ParentSceneNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(ParentUid)))
		{
			if (!ActiveSkeletonUids.Contains(ParentUid) || ParentSceneNode->IsSpecializedTypeContains(UE::Interchange::FSceneNodeStaticData::GetTransformSpecializeTypeString()))
			{
				return ParentUid;
			}
			else
			{
				bool bParentIsJoint = ParentSceneNode->IsSpecializedTypeContains(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString());
				if (bParentIsJoint)
				{
					//Check if its a rootjoint:
					//	aka check parent's parent if its !joint:
					FString ParentsParentUid = ParentSceneNode->GetParentUid();
					if (const UInterchangeSceneNode* ParentsParentSceneNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(ParentsParentUid)))
					{
						bool bParentsParentIsJoint = ParentSceneNode->IsSpecializedTypeContains(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString());
						if (bParentsParentIsJoint)
						{
							return FindFactoryParentSceneNodeUid(BaseNodeContainer, ActiveSkeletonUids, ParentSceneNode);
						}
						else
						{
							return ParentUid;
						}
					}
				}
				else
				{
					return ParentUid;
				}
			}
		}

		return UInterchangeBaseNode::InvalidNodeUid();
	}

	void UpdateReimportStrategyFlags(UInterchangeBaseNodeContainer& NodeContainer, UInterchangeFactoryBaseNode& FactoryNode, EReimportStrategyFlags ReimportPropertyStrategy)
	{
		FactoryNode.SetReimportStrategyFlags(ReimportPropertyStrategy);

		TArray<FString> ActorDependencies;
		FactoryNode.GetFactoryDependencies(ActorDependencies);
		for (const FString& FactoryNodeID : ActorDependencies)
		{
			if (UInterchangeFactoryBaseNode* DependencyFactoryNode = NodeContainer.GetFactoryNode(FactoryNodeID))
			{
				UpdateReimportStrategyFlags(NodeContainer, *DependencyFactoryNode, ReimportPropertyStrategy);
			}
		}
	}

#if WITH_EDITOR
	void DeleteAssets(const TArray<UObject*>& AssetsToDelete)
	{
		if (AssetsToDelete.IsEmpty())
		{
			return;
		}

		TArray<UObject*> ObjectsToForceDelete;
		ObjectsToForceDelete.Reserve(AssetsToDelete.Num());

		for (UObject* Asset : AssetsToDelete)
		{
			if (Asset)
			{
				ObjectsToForceDelete.Add(Asset);
			}
		}

		if (ObjectsToForceDelete.IsEmpty())
		{
			return;
		}

		constexpr bool bShowConfirmation = true;
		constexpr ObjectTools::EAllowCancelDuringDelete AllowCancelDuringDelete = ObjectTools::EAllowCancelDuringDelete::CancelNotAllowed;
		ObjectTools::DeleteObjects(ObjectsToForceDelete, bShowConfirmation, AllowCancelDuringDelete);
	}
#else
	void DeleteAssets(const TArray<UObject*>& AssetsToDelete)
	{
		if (AssetsToDelete.IsEmpty())
		{
			return;
		}

		bool bForceGarbageCollection = false;
		for (UObject* Asset : AssetsToDelete)
		{
			if (Asset)
			{
				Asset->Rename(nullptr, GetTransientPackage(), REN_NonTransactional | REN_DontCreateRedirectors);

				if (Asset->IsRooted())
				{
					Asset->RemoveFromRoot();
				}

				Asset->ClearFlags(RF_Public | RF_Standalone);
				Asset->MarkAsGarbage();

				bForceGarbageCollection = true;
			}
		}

		if (bForceGarbageCollection)
		{
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}
	}
#endif // WITH_EDITOR

	void DeleteActors(const TArray<AActor*>& ActorsToDelete)
	{
		if (ActorsToDelete.IsEmpty())
		{
			return;
		}

		for (AActor* Actor : ActorsToDelete)
		{
			if (!Actor)
			{
				continue;
			}

			if (UWorld* OwningWorld = Actor->GetWorld())
			{
				OwningWorld->EditorDestroyActor(Actor, true);
				// Since deletion can be delayed, rename to avoid future name collision
				// Call UObject::Rename directly on actor to avoid AActor::Rename which unnecessarily sunregister and re-register components
				Actor->UObject::Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
			}
		}
	}
}

void UInterchangeGenericLevelPipeline::AdjustSettingsForContext(EInterchangePipelineContext ImportType, TObjectPtr<UObject> ReimportAsset)
{
	Super::AdjustSettingsForContext(ImportType, ReimportAsset);

	bIsReimportContext = ReimportAsset != nullptr;
}

void UInterchangeGenericLevelPipeline::ExecutePipeline(UInterchangeBaseNodeContainer* InBaseNodeContainer, const TArray<UInterchangeSourceData*>& InSourceDatas, const FString& ContentBasePath)
{
	if (!InBaseNodeContainer)
	{
		UE_LOG(LogInterchangePipeline, Warning, TEXT("UInterchangeGenericAssetsPipeline: Cannot execute pre-import pipeline because InBaseNodeContrainer is null"));
		return;
	}

	BaseNodeContainer = InBaseNodeContainer;

	// Make sure all factory nodes created for assets have the chosen policy strategy
	InBaseNodeContainer->IterateNodesOfType<UInterchangeFactoryBaseNode>([this](const FString& NodeUid, UInterchangeFactoryBaseNode* FactoryNode)
		{
			if (this->bForceReimportDeletedAssets)
			{
				FactoryNode->SetForceNodeReimport();
			}
		});

	FTransform GlobalOffsetTransform = FTransform::Identity;
	if (UInterchangeCommonPipelineDataFactoryNode* CommonPipelineDataFactoryNode = UInterchangeCommonPipelineDataFactoryNode::GetUniqueInstance(BaseNodeContainer))
	{
		CommonPipelineDataFactoryNode->GetCustomGlobalOffsetTransform(GlobalOffsetTransform);
	}

	TArray<UInterchangeSceneNode*> SceneNodes;

	//Find all translated node we need for this pipeline
	BaseNodeContainer->IterateNodes([&SceneNodes](const FString& NodeUid, UInterchangeBaseNode* Node)
	{
		switch(Node->GetNodeContainerType())
		{
		case EInterchangeNodeContainerType::TranslatedScene:
		{
			if (UInterchangeSceneNode* SceneNode = Cast<UInterchangeSceneNode>(Node))
			{
				SceneNodes.Add(SceneNode);
			}
		}
		break;
		}
	});

#if WITH_EDITORONLY_DATA
	// Add the SceneImportData factory node
	{
		ensure(!SceneImportFactoryNode);

		const FString FilePath = FPaths::ConvertRelativePathToFull(InSourceDatas[0]->GetFilename());
		const FString DisplayLabel = TEXT("SceneImport_") + FPaths::GetBaseFilename(FilePath);
		const FString NodeUid = TEXT("SceneImport_") + FilePath;
		const FString FactoryNodeUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(NodeUid);
		ensure(!BaseNodeContainer->IsNodeUidValid(FactoryNodeUid));
		SceneImportFactoryNode = NewObject<UInterchangeSceneImportAssetFactoryNode>(BaseNodeContainer, NAME_None);

		SceneImportFactoryNode->InitializeNode(FactoryNodeUid, DisplayLabel, EInterchangeNodeContainerType::FactoryData);

		// Add dependency to all the factory nodes created so far
		BaseNodeContainer->IterateNodesOfType<UInterchangeFactoryBaseNode>(
			[this](const FString& NodeUid, UInterchangeFactoryBaseNode* Node)
			{
				// Make sure the UInterchangeSceneImportAsset is the last asset created
				this->SceneImportFactoryNode->AddFactoryDependencyUid(NodeUid);
			}
		);

		BaseNodeContainer->AddNode(SceneImportFactoryNode);
	}
#endif

	/* Find all scene node that are active joint. Non active joint should be convert to actor if they are in a static mesh hierarchy */
	TArray<FString> ActiveSkeletonUids = UE::Interchange::Private::GetAllActiveJoints(BaseNodeContainer);

	for (const UInterchangeSceneNode* SceneNode : SceneNodes)
	{
		if (SceneNode)
		{
			if (SceneNode->GetSpecializedTypeCount() > 0)
			{
				TArray<FString> SpecializeTypes;
				SceneNode->GetSpecializedTypes(SpecializeTypes);
				if (!SpecializeTypes.Contains(UE::Interchange::FSceneNodeStaticData::GetTransformSpecializeTypeString()))
				{
					bool bSkipNode = true;
					if (SpecializeTypes.Contains(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString()))
					{
						if(!ActiveSkeletonUids.Contains(SceneNode->GetUniqueID()))
						{ 
							bSkipNode = false;
						}
						else
						{
							//check if its the root joint (we want to create an actor for the root joint)
							FString CurrentNodesParentUid = SceneNode->GetParentUid();
							const UInterchangeBaseNode* ParentNode = BaseNodeContainer->GetNode(CurrentNodesParentUid);
							if (const UInterchangeSceneNode* ParentSceneNode = Cast<UInterchangeSceneNode>(ParentNode))
							{
								if (!ParentSceneNode->IsSpecializedTypeContains(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString()))
								{
									bSkipNode = false;
								}
							}
						}
					}

					if (bSkipNode)
					{
						//Skip any scene node that have specialized types but not the "Transform" type.
						continue;
					}
				}
			}
			ExecuteSceneNodePreImport(GlobalOffsetTransform, SceneNode);
		}
	}

	//Find all translated scene variant sets
	TArray<UInterchangeSceneVariantSetsNode*> SceneVariantSetNodes;

	InBaseNodeContainer->IterateNodesOfType<UInterchangeSceneVariantSetsNode>([&SceneVariantSetNodes](const FString& NodeUid, UInterchangeSceneVariantSetsNode* Node)
		{
			SceneVariantSetNodes.Add(Node);
		});

	for (const UInterchangeSceneVariantSetsNode* SceneVariantSetNode : SceneVariantSetNodes)
	{
		if (SceneVariantSetNode)
		{
			ExecuteSceneVariantSetNodePreImport(*SceneVariantSetNode);
		}
	}
}

void UInterchangeGenericLevelPipeline::ExecuteSceneNodePreImport(const FTransform& GlobalOffsetTransform, const UInterchangeSceneNode* SceneNode)
{
	using namespace UE::Interchange;

	if (!BaseNodeContainer || !SceneNode)
	{
		return;
	}

	const UInterchangeBaseNode* TranslatedAssetNode = nullptr;
	bool bRootJointNode = false;
	FString SkeletalMeshFactoryNodeUid;

	FString SkeletonFactoryNodeUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(SceneNode->GetUniqueID());
	const UInterchangeSkeletonFactoryNode* SkeletonFactoryNode = Cast<UInterchangeSkeletonFactoryNode>(BaseNodeContainer->GetFactoryNode(SkeletonFactoryNodeUid));
	if (SkeletonFactoryNode)
	{
		if (SkeletonFactoryNode->GetCustomSkeletalMeshFactoryNodeUid(SkeletalMeshFactoryNodeUid))
		{
			if (const UInterchangeFactoryBaseNode* SkeletalMeshFactoryNode = BaseNodeContainer->GetFactoryNode(SkeletalMeshFactoryNodeUid))
			{
				TArray<FString> NodeUids;
				SkeletalMeshFactoryNode->GetTargetNodeUids(NodeUids);

				if (NodeUids.Num() > 0)
				{
					TranslatedAssetNode = BaseNodeContainer->GetNode(NodeUids[0]);
					bRootJointNode = true;
				}
			}
		}
	}
	
	if (!bRootJointNode)
	{
		FString AssetInstanceUid;
		if (SceneNode->GetCustomAssetInstanceUid(AssetInstanceUid))
		{
			TranslatedAssetNode = BaseNodeContainer->GetNode(AssetInstanceUid);
		}
		if (const UInterchangeMeshNode* MeshNode = Cast<UInterchangeMeshNode>(TranslatedAssetNode))
		{
			bool bIsSkinnedMesh = MeshNode->IsSkinnedMesh();
			if(!bIsSkinnedMesh)
			{
				//In case we have a rigid mesh (static mesh having morph target...), we should find a skeletalmesh factory node holding this mesh node
				BaseNodeContainer->BreakableIterateNodesOfType<UInterchangeSkeletalMeshFactoryNode>([this, &SceneNode, &bIsSkinnedMesh](const FString& NodeUid, UInterchangeSkeletalMeshFactoryNode* SkeletalMeshNode)
				{
					int32 LodCount = SkeletalMeshNode->GetLodDataCount();
					TArray<FString> LodDataUniqueIds;
					SkeletalMeshNode->GetLodDataUniqueIds(LodDataUniqueIds);
					for (int32 LodIndex = 0; LodIndex < LodCount; ++LodIndex)
					{
						FString LodUniqueId = LodDataUniqueIds[LodIndex];
						const UInterchangeSkeletalMeshLodDataNode* LodDataNode = Cast<UInterchangeSkeletalMeshLodDataNode>(BaseNodeContainer->GetNode(LodUniqueId));
						if (LodDataNode)
						{
							TArray<FString> MeshUids;
							LodDataNode->GetMeshUids(MeshUids);
							if (MeshUids.Contains(SceneNode->GetUniqueID()))
							{
								bIsSkinnedMesh = true;
								return bIsSkinnedMesh;
							}
						}
					}
					return false;
				});	
			}
			//Skinned mesh are added when the bRootJointNode is true.
			//In this case we dont want to add an empty staticmesh actor.
			if (bIsSkinnedMesh)
			{
				return;
			}
		}
	}

	UInterchangeActorFactoryNode* ActorFactoryNode = CreateActorFactoryNode(SceneNode, TranslatedAssetNode);

	if (!ensure(ActorFactoryNode))
	{
		return;
	}

	FString NodeUid = SceneNode->GetUniqueID() + (bRootJointNode ? TEXT("_SkeletonNode") : TEXT(""));
	FString FactoryNodeUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(NodeUid);

	ActorFactoryNode->InitializeNode(FactoryNodeUid, SceneNode->GetDisplayLabel(), EInterchangeNodeContainerType::FactoryData);
	const FString ActorFactoryNodeUid = BaseNodeContainer->AddNode(ActorFactoryNode);

	// The translator is responsible to provide a unique name
	ActorFactoryNode->SetAssetName(SceneNode->GetAssetName());

	if (!SceneNode->GetParentUid().IsEmpty())
	{
		/* Find all scene node that are active joint. Non active joint should be convert to actor if they are in a static mesh hierarchy */
		TArray<FString> ActiveSkeletonUids = UE::Interchange::Private::GetAllActiveJoints(BaseNodeContainer);
		FString ParentNodeUid = UE::Interchange::Private::FindFactoryParentSceneNodeUid(BaseNodeContainer, ActiveSkeletonUids, SceneNode);
		if (ParentNodeUid != UInterchangeBaseNode::InvalidNodeUid())
		{
			FString ParentFactoryNodeUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(ParentNodeUid);
			if (BaseNodeContainer->SetNodeParentUid(ActorFactoryNodeUid, ParentFactoryNodeUid))
			{
				ActorFactoryNode->AddFactoryDependencyUid(ParentFactoryNodeUid);
			}
		}
	}

	if (bRootJointNode)
	{
		ActorFactoryNode->AddTargetNodeUid(SkeletalMeshFactoryNodeUid);
	}
	else
	{
		ActorFactoryNode->AddTargetNodeUid(SceneNode->GetUniqueID());
		SceneNode->AddTargetNodeUid(ActorFactoryNode->GetUniqueID());
	}

	//TODO move this code to the factory, a stack over pipeline can change the global offset transform which will affect this value.
	//We prioritize Local (Relative) Transforms due to issues introduced by 0 scales with Global Transforms.
	//In case the LocalTransform is not available we fallback onto GlobalTransforms
	FTransform LocalTransform;
	if (SceneNode->GetCustomLocalTransform(LocalTransform))
	{
		if (bRootJointNode)
		{
			LocalTransform = FTransform::Identity;
			//LocalTransform of RootjointNode is already baked into the Skeletal and animation.
			//due to that we acquire the Parent SceneNode and get its GlobalTransform:
			if (!SceneNode->GetParentUid().IsEmpty())
			{
				if (const UInterchangeSceneNode* ParentSceneNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(SceneNode->GetParentUid())))
				{
					ParentSceneNode->GetCustomLocalTransform(LocalTransform);
				}
			}
		}

		if (SceneNode->GetParentUid().IsEmpty())
		{
			LocalTransform = LocalTransform * GlobalOffsetTransform;
		}

		ActorFactoryNode->SetCustomLocalTransform(LocalTransform);
	}
	else
	{
		FTransform GlobalTransform;
		if (SceneNode->GetCustomGlobalTransform(BaseNodeContainer, GlobalOffsetTransform, GlobalTransform))
		{
			if (bRootJointNode)
			{
				GlobalTransform = FTransform::Identity;
				//LocalTransform of RootjointNode is already baked into the Skeletal and animation.
				//due to that we acquire the Parent SceneNode and get its GlobalTransform:
				if (!SceneNode->GetParentUid().IsEmpty())
				{
					if (const UInterchangeSceneNode* ParentSceneNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(SceneNode->GetParentUid())))
					{
						ParentSceneNode->GetCustomGlobalTransform(BaseNodeContainer, GlobalOffsetTransform, GlobalTransform);
					}
				}
			}
			ActorFactoryNode->SetCustomGlobalTransform(GlobalTransform);
		}
	}

	ActorFactoryNode->SetCustomMobility(EComponentMobility::Static);

	if (TranslatedAssetNode)
	{
		SetUpFactoryNode(ActorFactoryNode, SceneNode, TranslatedAssetNode);
	}

	//Make sure all actor factory nodes and dependencies have the specified strategy
	Private::UpdateReimportStrategyFlags(*BaseNodeContainer, *ActorFactoryNode, ReimportPropertyStrategy);
	if (bForceReimportDeletedActors)
	{
		ActorFactoryNode->SetForceNodeReimport();
	}

#if WITH_EDITORONLY_DATA
	// Add dependency to newly created factory node
	SceneImportFactoryNode->AddFactoryDependencyUid(ActorFactoryNode->GetUniqueID());
#endif
}

UInterchangeActorFactoryNode* UInterchangeGenericLevelPipeline::CreateActorFactoryNode(const UInterchangeSceneNode* SceneNode, const UInterchangeBaseNode* TranslatedAssetNode) const
{
	if (!ensure(BaseNodeContainer))
	{
		return nullptr;
	}

	if(TranslatedAssetNode)
	{
		if(TranslatedAssetNode->IsA<UInterchangePhysicalCameraNode>())
		{
			return NewObject<UInterchangePhysicalCameraFactoryNode>(BaseNodeContainer, NAME_None);
		}
		if (TranslatedAssetNode->IsA<UInterchangeStandardCameraNode>())
		{
			if (bUsePhysicalInsteadOfStandardPerspectiveCamera)
			{
				//in case it has perspective projection we want to use PhysicalCamera (CineCamera) instead:
				if (const UInterchangeStandardCameraNode* CameraNode = Cast<UInterchangeStandardCameraNode>(TranslatedAssetNode))
				{
					EInterchangeCameraProjectionType ProjectionType = EInterchangeCameraProjectionType::Perspective;
					if (CameraNode->GetCustomProjectionMode(ProjectionType) && ProjectionType == EInterchangeCameraProjectionType::Perspective)
					{
						return NewObject<UInterchangePhysicalCameraFactoryNode>(BaseNodeContainer, NAME_None);
					}
				}
			}

			return NewObject<UInterchangeStandardCameraFactoryNode>(BaseNodeContainer, NAME_None);
		}
		else if(TranslatedAssetNode->IsA<UInterchangeMeshNode>())
		{
			return NewObject<UInterchangeMeshActorFactoryNode>(BaseNodeContainer, NAME_None);
		}
		else if(TranslatedAssetNode->IsA<UInterchangeSpotLightNode>())
		{
			return NewObject<UInterchangeSpotLightFactoryNode>(BaseNodeContainer, NAME_None);
		}
		else if(TranslatedAssetNode->IsA<UInterchangePointLightNode>())
		{
			return NewObject<UInterchangePointLightFactoryNode>(BaseNodeContainer, NAME_None);
		}
		else if(TranslatedAssetNode->IsA<UInterchangeRectLightNode>())
		{
			return NewObject<UInterchangeRectLightFactoryNode>(BaseNodeContainer, NAME_None);
		}
		else if(TranslatedAssetNode->IsA<UInterchangeDirectionalLightNode>())
		{
			return NewObject<UInterchangeDirectionalLightFactoryNode>(BaseNodeContainer, NAME_None);
		}
		else if (TranslatedAssetNode && TranslatedAssetNode->IsA<UInterchangeDecalNode>())
		{
			return NewObject<UInterchangeDecalActorFactoryNode>(BaseNodeContainer, NAME_None);
		}
	}

	return NewObject<UInterchangeActorFactoryNode>(BaseNodeContainer, NAME_None);
}

void UInterchangeGenericLevelPipeline::SetUpFactoryNode(UInterchangeActorFactoryNode* ActorFactoryNode, const UInterchangeSceneNode* SceneNode, const UInterchangeBaseNode* TranslatedAssetNode) const
{
	if (!ensure(BaseNodeContainer && ActorFactoryNode && SceneNode && TranslatedAssetNode))
	{
		return;
	}

	if (const UInterchangeMeshNode* MeshNode = Cast<UInterchangeMeshNode>(TranslatedAssetNode))
	{
		TArray<FString> TargetNodeUids;
		ActorFactoryNode->GetTargetNodeUids(TargetNodeUids);
		bool bSkeletal = false;
		if (TargetNodeUids.Num() > 0)
		{
			if (const UInterchangeSkeletalMeshFactoryNode* SkeletalMeshFactoryNode = Cast<UInterchangeSkeletalMeshFactoryNode>(BaseNodeContainer->GetFactoryNode(TargetNodeUids[0])))
			{
				bSkeletal = true;
			}
		}

		if (bSkeletal)
		{
			ActorFactoryNode->SetCustomActorClassName(ASkeletalMeshActor::StaticClass()->GetPathName());
			ActorFactoryNode->SetCustomMobility(EComponentMobility::Movable);
		}
		else
		{
			ActorFactoryNode->SetCustomActorClassName(AStaticMeshActor::StaticClass()->GetPathName());
		}

		if (UInterchangeMeshActorFactoryNode* MeshActorFactoryNode = Cast<UInterchangeMeshActorFactoryNode>(ActorFactoryNode))
		{
			TMap<FString, FString> SlotMaterialDependencies;
			SceneNode->GetSlotMaterialDependencies(SlotMaterialDependencies);

			UE::Interchange::MeshesUtilities::ApplySlotMaterialDependencies(*MeshActorFactoryNode, SlotMaterialDependencies, *BaseNodeContainer, nullptr);

			FString AnimationAssetUidToPlay;
			if (SceneNode->GetCustomAnimationAssetUidToPlay(AnimationAssetUidToPlay))
			{
				MeshActorFactoryNode->SetCustomAnimationAssetUidToPlay(AnimationAssetUidToPlay);
			}

			MeshActorFactoryNode->AddFactoryDependencyUid(UInterchangeFactoryBaseNode::BuildFactoryNodeUid(MeshNode->GetUniqueID()));

			FTransform GeometricTransform;
			if (SceneNode->GetCustomGeometricTransform(GeometricTransform))
			{
				MeshActorFactoryNode->SetCustomGeometricTransform(GeometricTransform);
			}
		}
	}
	else if (const UInterchangeBaseLightNode* BaseLightNode = Cast<UInterchangeBaseLightNode>(TranslatedAssetNode))
	{
		if (UInterchangeBaseLightFactoryNode* BaseLightFactoryNode = Cast<UInterchangeBaseLightFactoryNode>(ActorFactoryNode))
		{
			if (FLinearColor LightColor; BaseLightNode->GetCustomLightColor(LightColor))
			{
				BaseLightFactoryNode->SetCustomLightColor(LightColor.ToFColor(true));
			}

			if (float Intensity; BaseLightNode->GetCustomIntensity(Intensity))
			{
				BaseLightFactoryNode->SetCustomIntensity(Intensity);
			}

			if(bool bUseTemperature; BaseLightNode->GetCustomUseTemperature(bUseTemperature))
			{
				BaseLightFactoryNode->SetCustomUseTemperature(bUseTemperature);

				if(float Temperature; BaseLightNode->GetCustomTemperature(Temperature))
				{
					BaseLightFactoryNode->SetCustomTemperature(Temperature);
				}
			}

			using FLightUnits = std::underlying_type_t<ELightUnits>;
			using FInterchangeLightUnits = std::underlying_type_t<EInterchangeLightUnits>;
			using FCommonLightUnits = std::common_type_t<FLightUnits, FInterchangeLightUnits>;

			static_assert(FCommonLightUnits(EInterchangeLightUnits::Unitless) == FCommonLightUnits(ELightUnits::Unitless), "EInterchangeLightUnits::Unitless differs from ELightUnits::Unitless");
			static_assert(FCommonLightUnits(EInterchangeLightUnits::Lumens) == FCommonLightUnits(ELightUnits::Lumens), "EInterchangeLightUnits::Lumens differs from ELightUnits::Lumens");
			static_assert(FCommonLightUnits(EInterchangeLightUnits::Candelas) == FCommonLightUnits(ELightUnits::Candelas), "EInterchangeLightUnits::Candelas differs from ELightUnits::Candelas");
			static_assert(FCommonLightUnits(EInterchangeLightUnits::EV) == FCommonLightUnits(ELightUnits::EV), "EInterchangeLightUnits::EV differs from ELightUnits::EV");

			if (const UInterchangeLightNode* LightNode = Cast<UInterchangeLightNode>(BaseLightNode))
			{
				if (UInterchangeLightFactoryNode* LightFactoryNode = Cast<UInterchangeLightFactoryNode>(BaseLightFactoryNode))
				{
					if (FString IESTextureUid; LightNode->GetCustomIESTexture(IESTextureUid))
					{
						if (BaseNodeContainer->GetNode(IESTextureUid))
						{
							LightFactoryNode->SetCustomIESTexture(IESTextureUid);
							LightFactoryNode->AddFactoryDependencyUid(UInterchangeFactoryBaseNode::BuildFactoryNodeUid(IESTextureUid));

							COPY_FROM_TRANSLATED_TO_FACTORY(LightNode, LightFactoryNode, UseIESBrightness, bool)
							COPY_FROM_TRANSLATED_TO_FACTORY(LightNode, LightFactoryNode, IESBrightnessScale, float)
							COPY_FROM_TRANSLATED_TO_FACTORY(LightNode, LightFactoryNode, Rotation, FRotator)
						}
					}

					if (EInterchangeLightUnits IntensityUnits; LightNode->GetCustomIntensityUnits(IntensityUnits))
					{
						LightFactoryNode->SetCustomIntensityUnits(ELightUnits(IntensityUnits));
					}

					COPY_FROM_TRANSLATED_TO_FACTORY(LightNode, LightFactoryNode, AttenuationRadius, float)

					// RectLight
					if(const UInterchangeRectLightNode* RectLightNode = Cast<UInterchangeRectLightNode>(LightNode))
					{
						if(UInterchangeRectLightFactoryNode* RectLightFactoryNode = Cast<UInterchangeRectLightFactoryNode>(LightFactoryNode))
						{
							COPY_FROM_TRANSLATED_TO_FACTORY(RectLightNode, RectLightFactoryNode, SourceWidth, float)
							COPY_FROM_TRANSLATED_TO_FACTORY(RectLightNode, RectLightFactoryNode, SourceHeight, float)
						}
					}

					// Point Light
					if (const UInterchangePointLightNode* PointLightNode = Cast<UInterchangePointLightNode>(LightNode))
					{
						if (UInterchangePointLightFactoryNode* PointLightFactoryNode = Cast<UInterchangePointLightFactoryNode>(LightFactoryNode))
						{
							if (bool bUseInverseSquaredFalloff; PointLightNode->GetCustomUseInverseSquaredFalloff(bUseInverseSquaredFalloff))
							{
								PointLightFactoryNode->SetCustomUseInverseSquaredFalloff(bUseInverseSquaredFalloff);

								COPY_FROM_TRANSLATED_TO_FACTORY(PointLightNode, PointLightFactoryNode, LightFalloffExponent, float)
							}


							// Spot Light
							if (const UInterchangeSpotLightNode* SpotLightNode = Cast<UInterchangeSpotLightNode>(PointLightNode))
							{
								if (UInterchangeSpotLightFactoryNode* SpotLightFactoryNode = Cast<UInterchangeSpotLightFactoryNode>(PointLightFactoryNode))
								{
									COPY_FROM_TRANSLATED_TO_FACTORY(SpotLightNode, SpotLightFactoryNode, InnerConeAngle, float)
									COPY_FROM_TRANSLATED_TO_FACTORY(SpotLightNode, SpotLightFactoryNode, OuterConeAngle, float)
								}
							}
						}
					}
				}
			}
		}

		//Test for spot before point since a spot light is a point light
		if (BaseLightNode->IsA<UInterchangeSpotLightNode>())
		{
			ActorFactoryNode->SetCustomActorClassName(ASpotLight::StaticClass()->GetPathName());
		}
		else if (BaseLightNode->IsA<UInterchangePointLightNode>())
		{
			ActorFactoryNode->SetCustomActorClassName(APointLight::StaticClass()->GetPathName());
		}
		else if (BaseLightNode->IsA<UInterchangeRectLightNode>())
		{
			ActorFactoryNode->SetCustomActorClassName(ARectLight::StaticClass()->GetPathName());
		}
		else if (BaseLightNode->IsA<UInterchangeDirectionalLightNode>())
		{
			ActorFactoryNode->SetCustomActorClassName(ADirectionalLight::StaticClass()->GetPathName());
		}
		else
		{
			ActorFactoryNode->SetCustomActorClassName(APointLight::StaticClass()->GetPathName());
		}
	}
	else if (const UInterchangePhysicalCameraNode* PhysicalCameraNode = Cast<UInterchangePhysicalCameraNode>(TranslatedAssetNode))
	{
		ActorFactoryNode->SetCustomActorClassName(ACineCameraActor::StaticClass()->GetPathName());
		ActorFactoryNode->SetCustomMobility(EComponentMobility::Movable);

		if (UInterchangePhysicalCameraFactoryNode* PhysicalCameraFactoryNode = Cast<UInterchangePhysicalCameraFactoryNode>(ActorFactoryNode))
		{
			float FocalLength;
			if (PhysicalCameraNode->GetCustomFocalLength(FocalLength))
			{
				PhysicalCameraFactoryNode->SetCustomFocalLength(FocalLength);
			}

			float SensorHeight;
			if (PhysicalCameraNode->GetCustomSensorHeight(SensorHeight))
			{
				PhysicalCameraFactoryNode->SetCustomSensorHeight(SensorHeight);
			}

			float SensorWidth;
			if (PhysicalCameraNode->GetCustomSensorWidth(SensorWidth))
			{
				PhysicalCameraFactoryNode->SetCustomSensorWidth(SensorWidth);
			}

			bool bEnableDepthOfField;
			if (PhysicalCameraNode->GetCustomEnableDepthOfField(bEnableDepthOfField))
			{
				PhysicalCameraFactoryNode->SetCustomFocusMethod(bEnableDepthOfField ? ECameraFocusMethod::Manual : ECameraFocusMethod::DoNotOverride);
			}
			
		}
	}
	else if (const UInterchangeStandardCameraNode* CameraNode = Cast<UInterchangeStandardCameraNode>(TranslatedAssetNode))
	{
		EInterchangeCameraProjectionType ProjectionType = EInterchangeCameraProjectionType::Perspective;
		if (CameraNode->GetCustomProjectionMode(ProjectionType) && bUsePhysicalInsteadOfStandardPerspectiveCamera && ProjectionType == EInterchangeCameraProjectionType::Perspective)
		{
			float AspectRatio = 1.0f;
			CameraNode->GetCustomAspectRatio(AspectRatio);

			const float SensorWidth = 36.f;  // mm
			float SensorHeight = SensorWidth / AspectRatio;

			float FieldOfView = 90;
			CameraNode->GetCustomFieldOfView(FieldOfView); //Degrees

			float FocalLength = (SensorHeight) / (2.0 * tan(FMath::DegreesToRadians(FieldOfView) / 2.0));

			ActorFactoryNode->SetCustomActorClassName(ACineCameraActor::StaticClass()->GetPathName());
			ActorFactoryNode->SetCustomMobility(EComponentMobility::Movable);

			if (UInterchangePhysicalCameraFactoryNode* PhysicalCameraFactoryNode = Cast<UInterchangePhysicalCameraFactoryNode>(ActorFactoryNode))
			{
				PhysicalCameraFactoryNode->SetCustomFocalLength(FocalLength);
				PhysicalCameraFactoryNode->SetCustomSensorHeight(SensorHeight);
				PhysicalCameraFactoryNode->SetCustomSensorWidth(SensorWidth);
				PhysicalCameraFactoryNode->SetCustomFocusMethod(ECameraFocusMethod::DoNotOverride);
			}
		}
		else
		{
			ActorFactoryNode->SetCustomActorClassName(ACameraActor::StaticClass()->GetPathName());
			ActorFactoryNode->SetCustomMobility(EComponentMobility::Movable);

			if (UInterchangeStandardCameraFactoryNode* CameraFactoryNode = Cast<UInterchangeStandardCameraFactoryNode>(ActorFactoryNode))
			{
				if (CameraNode->GetCustomProjectionMode(ProjectionType))
				{
					CameraFactoryNode->SetCustomProjectionMode((ECameraProjectionMode::Type)ProjectionType);
				}

				float OrthoWidth;
				if (CameraNode->GetCustomWidth(OrthoWidth))
				{
					CameraFactoryNode->SetCustomWidth(OrthoWidth);
				}

				float OrthoNearClipPlane;
				if (CameraNode->GetCustomNearClipPlane(OrthoNearClipPlane))
				{
					CameraFactoryNode->SetCustomNearClipPlane(OrthoNearClipPlane);
				}

				float OrthoFarClipPlane;
				if (CameraNode->GetCustomFarClipPlane(OrthoFarClipPlane))
				{
					CameraFactoryNode->SetCustomFarClipPlane(OrthoFarClipPlane);
				}

				float AspectRatio;
				if (CameraNode->GetCustomAspectRatio(AspectRatio))
				{
					CameraFactoryNode->SetCustomAspectRatio(AspectRatio);
				}

				float FieldOfView;
				if (CameraNode->GetCustomFieldOfView(FieldOfView))
				{
					CameraFactoryNode->SetCustomFieldOfView(FieldOfView);
				}
			}
		}
	}
	else if (const UInterchangeDecalNode* DecalNode = Cast<UInterchangeDecalNode>(TranslatedAssetNode))
	{
		UInterchangeDecalActorFactoryNode* DecalActorFactory = Cast<UInterchangeDecalActorFactoryNode>(ActorFactoryNode);
		ensure(DecalActorFactory);

		if (FVector DecalSize; DecalNode->GetCustomDecalSize(DecalSize))
		{
			DecalActorFactory->SetCustomDecalSize(DecalSize);
		}

		if (int32 SortOrder; DecalNode->GetCustomSortOrder(SortOrder))
		{
			DecalActorFactory->SetCustomSortOrder(SortOrder);
		}
		
		bool bHasMaterialPathName = false;
		FString DecalMaterialPathName;
		if (DecalNode->GetCustomDecalMaterialPathName(DecalMaterialPathName))
		{
			DecalActorFactory->SetCustomDecalMaterialPathName(DecalMaterialPathName);
			bHasMaterialPathName = true;
		}

		// If the path is not a valid object path then it is an Interchange Node UID (Decal Material Node to be specific).
		if (bHasMaterialPathName && !FPackageName::IsValidObjectPath(DecalMaterialPathName))
		{
			const FString MaterialFactoryUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(DecalMaterialPathName);
			DecalActorFactory->SetCustomDecalMaterialPathName(MaterialFactoryUid);
			DecalActorFactory->AddFactoryDependencyUid(MaterialFactoryUid);
		}
	}
}

void UInterchangeGenericLevelPipeline::ExecuteSceneVariantSetNodePreImport(const UInterchangeSceneVariantSetsNode& SceneVariantSetNode)
{
	if (!ensure(BaseNodeContainer))
	{
		return;
	}

	// We may eventually want to optionally import variants
	static bool bEnableSceneVariantSet = true;

	const FString FactoryNodeUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(SceneVariantSetNode.GetUniqueID());

	UInterchangeSceneVariantSetsFactoryNode* FactoryNode = NewObject<UInterchangeSceneVariantSetsFactoryNode>(BaseNodeContainer, NAME_None);

	FactoryNode->InitializeNode(FactoryNodeUid, SceneVariantSetNode.GetDisplayLabel(), EInterchangeNodeContainerType::FactoryData);
	FactoryNode->SetEnabled(bEnableSceneVariantSet);

	TArray<FString> VariantSetUids;
	SceneVariantSetNode.GetCustomVariantSetUids(VariantSetUids);

	for (const FString& VariantSetUid : VariantSetUids)
	{
		FactoryNode->AddCustomVariantSetUid(VariantSetUid);

		// Update factory's dependencies
		if (const UInterchangeVariantSetNode* TrackNode = Cast<UInterchangeVariantSetNode>(BaseNodeContainer->GetNode(VariantSetUid)))
		{
			TArray<FString> DependencyNodeUids;
			TrackNode->GetCustomDependencyUids(DependencyNodeUids);

			for (const FString& DependencyNodeUid : DependencyNodeUids)
			{
				
				const FString DependencyFactoryNodeUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(DependencyNodeUid);
				FactoryNode->AddFactoryDependencyUid(DependencyFactoryNodeUid);

				if (UInterchangeFactoryBaseNode* DependencyFactoryNode = BaseNodeContainer->GetFactoryNode(DependencyFactoryNodeUid))
				{
					if (bEnableSceneVariantSet && !DependencyFactoryNode->IsEnabled())
					{
						DependencyFactoryNode->SetEnabled(true);
					}
				}
			}
		}
	}

	UInterchangeUserDefinedAttributesAPI::DuplicateAllUserDefinedAttribute(&SceneVariantSetNode, FactoryNode, false);

	FactoryNode->AddTargetNodeUid(SceneVariantSetNode.GetUniqueID());
	SceneVariantSetNode.AddTargetNodeUid(FactoryNode->GetUniqueID());

	BaseNodeContainer->AddNode(FactoryNode);
}

void UInterchangeGenericLevelPipeline::ExecutePostImportPipeline(const UInterchangeBaseNodeContainer* InBaseNodeContainer, const FString& NodeKey, UObject* CreatedAsset, bool bIsAReimport)
{
	Super::ExecutePostImportPipeline(InBaseNodeContainer, NodeKey, CreatedAsset, bIsAReimport);

#if WITH_EDITORONLY_DATA
	using namespace UE::Interchange;

	//We do not use the provided base container since ExecutePreImportPipeline cache it
	//We just make sure the same one is pass in parameter
	if (!InBaseNodeContainer || !ensure(BaseNodeContainer == InBaseNodeContainer) || !CreatedAsset || !ensure(IsInGameThread()))
	{
		return;
	}

	if (UInterchangeSceneImportAsset* SceneImportAsset = Cast<UInterchangeSceneImportAsset>(CreatedAsset))
	{
		if (!bIsReimportContext || !bDeleteMissingActors)
		{
			SceneImportAsset->UpdateSceneObjects();
			return;
		}

		const UInterchangeSceneImportAssetFactoryNode* FactoryNode = Cast<UInterchangeSceneImportAssetFactoryNode>(BaseNodeContainer->GetFactoryNode(NodeKey));
		if (!ensure(FactoryNode))
		{
			SceneImportAsset->UpdateSceneObjects();
			return;
		}

		// Cache list of objects previously imported in case of a re-import
		TArray<FSoftObjectPath> PrevSoftObjectPaths;
		SceneImportAsset->GetSceneSoftObjectPaths(PrevSoftObjectPaths);

		SceneImportAsset->UpdateSceneObjects();

		// Nothing to take care of
		if (PrevSoftObjectPaths.IsEmpty())
		{
			return;
		}

		TArray<FSoftObjectPath> NewSoftObjectPaths;
		SceneImportAsset->GetSceneSoftObjectPaths(NewSoftObjectPaths);

		TSet<FSoftObjectPath> SoftObjectPathSet(NewSoftObjectPaths);
		TArray<AActor*> ActorsToDelete;
		TArray<UObject*> AssetsToForceDelete;

		ActorsToDelete.Reserve(PrevSoftObjectPaths.Num());
		AssetsToForceDelete.Reserve(PrevSoftObjectPaths.Num());

		for (const FSoftObjectPath& ObjectPath : PrevSoftObjectPaths)
		{
			if (!SoftObjectPathSet.Contains(ObjectPath))
			{
				if (UObject* Object = ObjectPath.TryLoad())
				{
					if (IsValid(Object))
					{
						if (Object->GetClass()->IsChildOf<AActor>())
						{
							ActorsToDelete.Add(Cast<AActor>(Object));
						}
						else
						{
							AssetsToForceDelete.Add(Object);
						}
					}
				}
			}
		}

		Private::DeleteActors(ActorsToDelete);

		if (!AssetsToForceDelete.IsEmpty())
		{
			Private::DeleteAssets(AssetsToForceDelete);
		}

		// Update newly imported objects with a soft reference to the UInterchangeSceneImportAsset
		for (const FSoftObjectPath& ObjectPath : NewSoftObjectPaths)
		{
			UObject* Object = ObjectPath.TryLoad();
			if (IsValid(Object) && Object != CreatedAsset)
			{
				if (UInterchangeAssetImportData* AssetImportData = UInterchangeAssetImportData::GetFromObject(Object))
				{
					AssetImportData->SceneImportAsset = CreatedAsset;
				}
			}
		}
	}
#endif
}

