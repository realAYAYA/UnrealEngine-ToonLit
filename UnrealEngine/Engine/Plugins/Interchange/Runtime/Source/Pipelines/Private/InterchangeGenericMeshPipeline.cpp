// Copyright Epic Games, Inc. All Rights Reserved. 
#include "InterchangeGenericMeshPipeline.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "InterchangeAnimationTrackSetNode.h"
#include "InterchangeGenericAssetsPipeline.h"
#include "InterchangeMeshNode.h"
#include "InterchangePipelineLog.h"
#include "InterchangePipelineMeshesUtilities.h"
#include "InterchangeSceneNode.h"
#include "InterchangeSkeletalMeshFactoryNode.h"
#include "InterchangeStaticMeshFactoryNode.h"
#include "InterchangeSourceData.h"
#include "Misc/Paths.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "PhysicsEngine/PhysicsAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeGenericMeshPipeline)

void UInterchangeGenericMeshPipeline::AdjustSettingsForContext(EInterchangePipelineContext ImportType, TObjectPtr<UObject> ReimportAsset)
{
	Super::AdjustSettingsForContext(ImportType, ReimportAsset);

#if WITH_EDITOR

	check(CommonSkeletalMeshesAndAnimationsProperties.IsValid());
	if (ImportType == EInterchangePipelineContext::None)
	{
		//We do not change the setting if we are in editing context
		return;
	}

	const bool bIsReimport = IsReimportContext();

	//Avoid creating physics asset when importing a LOD or the alternate skinning
	if (ImportType == EInterchangePipelineContext::AssetCustomLODImport
		|| ImportType == EInterchangePipelineContext::AssetCustomLODReimport
		|| ImportType == EInterchangePipelineContext::AssetAlternateSkinningImport
		|| ImportType == EInterchangePipelineContext::AssetAlternateSkinningReimport)
	{
		bCreatePhysicsAsset = false;
		PhysicsAsset = nullptr;
		
		if (ImportType == EInterchangePipelineContext::AssetAlternateSkinningImport
			|| ImportType == EInterchangePipelineContext::AssetAlternateSkinningReimport)
		{
			CommonMeshesProperties->ForceAllMeshAsType = EInterchangeForceMeshType::IFMT_SkeletalMesh;
			CommonMeshesProperties->bAutoDetectMeshType = false;
			CommonMeshesProperties->bBakeMeshes = true;
			CommonMeshesProperties->bImportLods = false;
			CommonMeshesProperties->bKeepSectionsSeparate = false;
			CommonMeshesProperties->VertexColorImportOption = EInterchangeVertexColorImportOption::IVCIO_Ignore;
			bImportSkeletalMeshes = true;
			bImportStaticMeshes = false;
			bBuildNanite = false;
			bImportMorphTargets = false;
			bImportVertexAttributes = false;
			bUpdateSkeletonReferencePose = false;
			SkeletalMeshImportContentType = EInterchangeSkeletalMeshContentType::All;
			CommonSkeletalMeshesAndAnimationsProperties->Skeleton = nullptr;
			CommonSkeletalMeshesAndAnimationsProperties->bImportOnlyAnimations = false;
		}
	}
	const FString CommonMeshesCategory = TEXT("Common Meshes");
	const FString StaticMeshesCategory = TEXT("Static Meshes");
	const FString SkeletalMeshesCategory = TEXT("Skeletal Meshes");
	const FString CommonSkeletalMeshesAndAnimationCategory = TEXT("Common Skeletal Meshes and Animations");

	TArray<FString> HideCategories;
	TArray<FString> HideSubCategories;
	if (ImportType == EInterchangePipelineContext::AssetReimport)
	{
		HideSubCategories.Add(TEXT("Build"));
		if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(ReimportAsset))
		{
			//Set the skeleton to the current asset skeleton
			CommonSkeletalMeshesAndAnimationsProperties->Skeleton = SkeletalMesh->GetSkeleton();
			PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
			if (PhysicsAsset.IsValid())
			{
				bCreatePhysicsAsset = false;
			}
			bImportStaticMeshes = false;
			HideCategories.Add(StaticMeshesCategory);
			if(SkeletalMeshImportContentType == EInterchangeSkeletalMeshContentType::Geometry)
			{
				CommonMeshesProperties->ForceAllMeshAsType = EInterchangeForceMeshType::IFMT_SkeletalMesh;
			}
		}
		else if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(ReimportAsset))
		{
			HideCategories.Add(SkeletalMeshesCategory);
			HideCategories.Add(CommonSkeletalMeshesAndAnimationCategory);
		}
		else if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(ReimportAsset))
		{
			HideCategories.Add(StaticMeshesCategory);
			HideCategories.Add(SkeletalMeshesCategory);
			HideCategories.Add(CommonMeshesCategory);
		}
		else if (ReimportAsset)
		{
			HideCategories.Add(StaticMeshesCategory);
			HideCategories.Add(SkeletalMeshesCategory);
			HideCategories.Add(CommonMeshesCategory);
			HideCategories.Add(CommonSkeletalMeshesAndAnimationCategory);
		}
	}

	if (UInterchangePipelineBase* OuterMostPipeline = GetMostPipelineOuter())
	{
		constexpr bool bDoTransientSubPipeline = true;
		if (UInterchangeGenericAssetsPipeline* ParentPipeline = Cast<UInterchangeGenericAssetsPipeline>(OuterMostPipeline))
		{
			if (ParentPipeline->ReimportStrategy == EReimportStrategyFlags::ApplyNoProperties)
			{
				for (const FString& HideSubCategoryName : HideSubCategories)
				{
					HidePropertiesOfSubCategory(OuterMostPipeline, this, HideSubCategoryName, bDoTransientSubPipeline);
				}
			}
		}

		for (const FString& HideCategoryName : HideCategories)
		{
			HidePropertiesOfCategory(OuterMostPipeline, this, HideCategoryName, bDoTransientSubPipeline);
		}
	}
#endif //WITH_EDITOR
}

#if WITH_EDITOR

bool UInterchangeGenericMeshPipeline::IsPropertyChangeNeedRefresh(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UInterchangeGenericMeshPipeline, SkeletalMeshImportContentType))
	{
		return true;
	}
	return Super::IsPropertyChangeNeedRefresh(PropertyChangedEvent);
}

#endif //WITH_EDITOR

void UInterchangeGenericMeshPipeline::PreDialogCleanup(const FName PipelineStackName)
{
	PhysicsAsset = nullptr;
}

#if WITH_EDITOR

bool UInterchangeGenericMeshPipeline::GetPropertyPossibleValues(const FName PropertyPath, TArray<FString>& PossibleValues)
{
	FString PropertyPathString = PropertyPath.ToString();
	int32 PropertyNameIndex = INDEX_NONE;
	if (PropertyPathString.FindLastChar(':', PropertyNameIndex))
	{
		PropertyPathString = PropertyPathString.RightChop(PropertyNameIndex+1);
	}
	if (PropertyPathString.Equals(GET_MEMBER_NAME_STRING_CHECKED(UInterchangeGenericMeshPipeline, LodGroup)))
	{
		TArray<FName> LODGroupNames;
		UStaticMesh::GetLODGroups(LODGroupNames);
		for (int32 GroupIndex = 0; GroupIndex < LODGroupNames.Num(); ++GroupIndex)
		{
			PossibleValues.Add(LODGroupNames[GroupIndex].GetPlainNameString());
		}
		return true;
	}
	//If we did not find any property call the super implementation
	return Super::GetPropertyPossibleValues(PropertyPath, PossibleValues);
}

#endif

UInterchangePipelineMeshesUtilities* UInterchangeGenericMeshPipeline::CreateMeshPipelineUtilities(UInterchangeBaseNodeContainer* InBaseNodeContainer
	, const UInterchangeGenericMeshPipeline* Pipeline
	, const bool bAutoDetectType)
{
	UInterchangePipelineMeshesUtilities* CreatedPipelineMeshesUtilities = UInterchangePipelineMeshesUtilities::CreateInterchangePipelineMeshesUtilities(InBaseNodeContainer);

	bool bAutoDetectConvertStaticMeshToSkeletalMesh = false;
	if (bAutoDetectType && Pipeline->CommonMeshesProperties->ForceAllMeshAsType == EInterchangeForceMeshType::IFMT_None)
	{
		TArray<FString> StaticMeshNodeUids;
		bool bContainSkeletalMesh = false;
		InBaseNodeContainer->IterateNodesOfType<UInterchangeMeshNode>([&bContainSkeletalMesh, &StaticMeshNodeUids](const FString& NodeUid, UInterchangeMeshNode* MeshNode)
			{
				if (!MeshNode->IsMorphTarget())
				{
					MeshNode->IsSkinnedMesh() ? bContainSkeletalMesh = true : StaticMeshNodeUids.Add(NodeUid);
				}
			});
		
		bool bContainAnimationNode = false;
		if (!bContainSkeletalMesh && StaticMeshNodeUids.Num() > 0)
		{
			TMap<const UInterchangeSceneNode*, bool> CacheProcessSceneNodes;
			InBaseNodeContainer->BreakableIterateNodesOfType<UInterchangeTransformAnimationTrackNode>([&InBaseNodeContainer, &bContainAnimationNode, &StaticMeshNodeUids, &CacheProcessSceneNodes](const FString& NodeUid, UInterchangeTransformAnimationTrackNode* AnimationNode)
				{
					FString SceneNodeUid;
					if (AnimationNode->GetCustomActorDependencyUid(SceneNodeUid))
					{
						if (const UInterchangeSceneNode* SceneNode = Cast<UInterchangeSceneNode>(InBaseNodeContainer->GetNode(SceneNodeUid)))
						{
							if (IsImpactingAnyMeshesRecursive(SceneNode, InBaseNodeContainer, StaticMeshNodeUids, CacheProcessSceneNodes))
							{
								bContainAnimationNode = true;
							}
						}
					}
					return bContainAnimationNode;
				});
		}

		//Auto detect some static mesh transform animations, we need to force the skeletal mesh type and recompute
		bAutoDetectConvertStaticMeshToSkeletalMesh = bContainAnimationNode;
	}

	//Set the context option to use when querying the pipeline mesh utilities
	FInterchangePipelineMeshesUtilitiesContext DataContext;
	DataContext.bConvertStaticMeshToSkeletalMesh = bAutoDetectConvertStaticMeshToSkeletalMesh || (Pipeline->CommonMeshesProperties->ForceAllMeshAsType == EInterchangeForceMeshType::IFMT_SkeletalMesh);
	DataContext.bConvertSkeletalMeshToStaticMesh = (Pipeline->CommonMeshesProperties->ForceAllMeshAsType == EInterchangeForceMeshType::IFMT_StaticMesh);
	DataContext.bConvertStaticsWithMorphTargetsToSkeletals = Pipeline->CommonSkeletalMeshesAndAnimationsProperties->bConvertStaticsWithMorphTargetsToSkeletals;
	DataContext.bImportMeshesInBoneHierarchy = Pipeline->CommonSkeletalMeshesAndAnimationsProperties->bImportMeshesInBoneHierarchy;
	DataContext.bQueryGeometryOnlyIfNoInstance = Pipeline->CommonMeshesProperties->bBakeMeshes;
	CreatedPipelineMeshesUtilities->SetContext(DataContext);
	return CreatedPipelineMeshesUtilities;
}

void UInterchangeGenericMeshPipeline::ExecutePipeline(UInterchangeBaseNodeContainer* InBaseNodeContainer, const TArray<UInterchangeSourceData*>& InSourceDatas, const FString& ContentBasePath)
{
	if (!InBaseNodeContainer)
	{
		UE_LOG(LogInterchangePipeline, Warning, TEXT("UInterchangeGenericMeshPipeline: Cannot execute pre-import pipeline because InBaseNodeContrainer is null."));
		return;
	}
	
	BaseNodeContainer = InBaseNodeContainer;
	SourceDatas.Empty(InSourceDatas.Num());
	for (const UInterchangeSourceData* SourceData : InSourceDatas)
	{
		SourceDatas.Add(SourceData);
	}
	PipelineMeshesUtilities = CreateMeshPipelineUtilities(BaseNodeContainer, this, CommonMeshesProperties->bAutoDetectMeshType);

	//Create skeletalmesh factory nodes
	ExecutePreImportPipelineSkeletalMesh();

	//Create staticmesh factory nodes
	ExecutePreImportPipelineStaticMesh();
}

void UInterchangeGenericMeshPipeline::ExecutePostImportPipeline(const UInterchangeBaseNodeContainer* InBaseNodeContainer, const FString& FactoryNodeKey, UObject* CreatedAsset, bool bIsAReimport)
{
	//We do not use the provided base container since ExecutePreImportPipeline cache it
	//We just make sure the same one is pass in parameter
	if (!InBaseNodeContainer || !ensure(BaseNodeContainer == InBaseNodeContainer) || !CreatedAsset)
	{
		return;
	}

	const UInterchangeFactoryBaseNode* FactoryNode = BaseNodeContainer->GetFactoryNode(FactoryNodeKey);
	if (!FactoryNode)
	{
		return;
	}

	//Set the last content type import
	LastSkeletalMeshImportContentType = SkeletalMeshImportContentType;

	PostImportSkeletalMesh(CreatedAsset, FactoryNode);

	//Finish the physics asset import, it need the skeletal mesh render data to create the physics collision geometry
	PostImportPhysicsAssetImport(CreatedAsset, FactoryNode);
}

void UInterchangeGenericMeshPipeline::SetReimportSourceIndex(UClass* ReimportObjectClass, const int32 SourceFileIndex)
{
	if (ReimportObjectClass == USkeletalMesh::StaticClass())
	{
		switch (SourceFileIndex)
		{
			case 0:
			{
				//Geo and skinning
				SkeletalMeshImportContentType = EInterchangeSkeletalMeshContentType::All;
			}
			break;

			case 1:
			{
				//Geo only
				SkeletalMeshImportContentType = EInterchangeSkeletalMeshContentType::Geometry;
			}
			break;

			case 2:
			{
				//Skinning only
				SkeletalMeshImportContentType = EInterchangeSkeletalMeshContentType::SkinningWeights;
			}
			break;

			default:
			{
				//In case SourceFileIndex == INDEX_NONE //No specified options, we use the last imported content type
				SkeletalMeshImportContentType = LastSkeletalMeshImportContentType;
			}
		};
	}
}

#if WITH_EDITOR
bool UInterchangeGenericMeshPipeline::DoClassesIncludeAllEditableStructProperties(const TArray<const UClass*>& Classes, const UStruct* Struct)
{
	check(IsInGameThread());

	bool bResult = true;
	const FName CategoryKey("Category");
	for (const FProperty* Property = Struct->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		//skip (transient, deprecated, const) property
		if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated | CPF_EditConst))
		{
			continue;
		}
		//skip property that is not editable
		if (!Property->HasAnyPropertyFlags(CPF_Edit))
		{
			continue;
		}
		const FObjectProperty* SubObject = CastField<FObjectProperty>(Property);
		if (SubObject)
		{
			continue;
		}
		else if (const FString* PropertyCategoryString = Property->FindMetaData(CategoryKey))
		{
			FName PropertyName = Property->GetFName();
			bool bFindProperty = false;
			for (const UClass* Class : Classes)
			{
				if (Class->FindPropertyByName(PropertyName) != nullptr)
				{
					bFindProperty = true;
					break;
				}
			}
			//Ensure to notify
			if (!bFindProperty)
			{
				UE_LOG(LogInterchangePipeline, Log, TEXT("The Interchange mesh pipeline does not include build property %s."), *PropertyName.ToString());
				bResult = false;
			}
		}
	}
	return bResult;
}
#endif

bool UInterchangeGenericMeshPipeline::IsImpactingAnyMeshesRecursive(const UInterchangeSceneNode* SceneNode
	, const UInterchangeBaseNodeContainer* InBaseNodeContainer
	, const TArray<FString>& StaticMeshNodeUids
	, TMap<const UInterchangeSceneNode*, bool>& CacheProcessSceneNodes)
{
	bool& bIsImpactingCache = CacheProcessSceneNodes.FindOrAdd(SceneNode, false);
	if (bIsImpactingCache)
	{
		return bIsImpactingCache;
	}
	FString AssetUid;
	if (SceneNode->GetCustomAssetInstanceUid(AssetUid))
	{
		if (StaticMeshNodeUids.Contains(AssetUid))
		{
			bIsImpactingCache = true;
			return true;
		}
	}
	TArray<FString> Children = InBaseNodeContainer->GetNodeChildrenUids(SceneNode->GetUniqueID());
	for (const FString& ChildUid : Children)
	{
		if (const UInterchangeSceneNode* ChildSceneNode = Cast<UInterchangeSceneNode>(InBaseNodeContainer->GetNode(ChildUid)))
		{
			if (IsImpactingAnyMeshesRecursive(ChildSceneNode, InBaseNodeContainer, StaticMeshNodeUids, CacheProcessSceneNodes))
			{
				return true;
			}
		}
	}
	return false;
}