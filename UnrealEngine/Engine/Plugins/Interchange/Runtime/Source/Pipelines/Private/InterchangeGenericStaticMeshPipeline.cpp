// Copyright Epic Games, Inc. All Rights Reserved. 
#include "InterchangeGenericMeshPipeline.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/Async.h"
#include "CoreMinimal.h"
#include "InterchangeMaterialFactoryNode.h"
#include "InterchangeMeshNode.h"
#include "InterchangePipelineLog.h"
#include "InterchangePipelineMeshesUtilities.h"
#include "InterchangeSceneNode.h"
#include "InterchangeStaticMeshFactoryNode.h"
#include "InterchangeStaticMeshLodDataNode.h"
#include "InterchangeSourceData.h"
#include "Misc/Paths.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Nodes/InterchangeUserDefinedAttribute.h"
#include "Tasks/Task.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"


enum class EMeshCollisionType
{
	None,
	Box,
	Sphere,
	Capsule,
	Convex
};


static FStringView GetMeshNameFromUid(const FString& NodeUid)
{
	FStringView MeshName = NodeUid;
	int32 FinalDot = INDEX_NONE;
	if (MeshName.FindLastChar(TEXT('.'), FinalDot))
	{
		MeshName.RightChopInline(FinalDot + 1);
	}

	return MeshName;
}


static TTuple<EMeshCollisionType, FStringView> GetCollisionMeshType(const FString& NodeUid, const TArray<FString>& AllNodeUids)
{
	FStringView MeshName = GetMeshNameFromUid(NodeUid);
	EMeshCollisionType CollisionType = EMeshCollisionType::None;

	// Determine if the mesh name is a potential collision mesh

	if (MeshName.StartsWith(TEXT("UBX_")))
	{
		CollisionType = EMeshCollisionType::Box;
	}
	else if (MeshName.StartsWith(TEXT("UCX_")) || MeshName.StartsWith(TEXT("MCDCX_")))
	{
		CollisionType = EMeshCollisionType::Convex;
	}
	else if (MeshName.StartsWith(TEXT("USP_")))
	{
		CollisionType = EMeshCollisionType::Sphere;
	}
	else if (MeshName.StartsWith(TEXT("UCP_")))
	{
		CollisionType = EMeshCollisionType::Capsule;
	}
	else
	{
		return { EMeshCollisionType::None, FStringView() };
	}

	// We have a mesh name with a collision type suffix.
	// However it should only be treated as a collision mesh if its body name corresponds to one of the other meshes.
	// If we get here, we know there is at least one underscore, so we never expect either of these character searches to fail.

	int32 FirstUnderscore = INDEX_NONE;
	verify(MeshName.FindChar(TEXT('_'), FirstUnderscore));

	int32 LastUnderscore = INDEX_NONE;
	verify(MeshName.FindLastChar(TEXT('_'), LastUnderscore));

	auto MatchPredicate = [](FStringView Body)
	{
		// Generate a predicate to be used by the below Finds.
		return [Body](const FString& ToCompare) { return Body == GetMeshNameFromUid(ToCompare); };
	};

	// If we find a mesh named the same as the collision mesh (following the collision prefix), we have a match
	// e.g. this will match 'UBX_House' with a mesh called 'House'

	const FString* CorrespondingMeshUid = AllNodeUids.FindByPredicate(MatchPredicate(MeshName.RightChop(FirstUnderscore + 1)));
	if (CorrespondingMeshUid)
	{
		return { CollisionType, *CorrespondingMeshUid };
	}

	// Otherwise strip the final underscore suffix from the collision mesh name and look again
	// e.g. this will match 'UBX_House_01' with a mesh called 'House'

	if (FirstUnderscore != LastUnderscore)
	{
		CorrespondingMeshUid = AllNodeUids.FindByPredicate(MatchPredicate(MeshName.Mid(FirstUnderscore + 1, LastUnderscore - FirstUnderscore - 1)));
		if (CorrespondingMeshUid)
		{
			return { CollisionType, *CorrespondingMeshUid };
		}
	}

	// Mesh had a collision type prefix, but no corresponding mesh, so don't treat it as a collision mesh

	return { EMeshCollisionType::None, FStringView() };
}


static bool IsCollisionMeshUid(const FString& MeshUid, const TArray<FString>& MeshUids)
{
	return GetCollisionMeshType(MeshUid, MeshUids).Get<0>() != EMeshCollisionType::None;
}


static void BuildMeshToCollisionMeshMap(const TArray<FString>& MeshUids, TMap<FString, TArray<FString>>& MeshToCollisionMeshMap)
{
	for (const FString& MeshUid : MeshUids)
	{
		TTuple<EMeshCollisionType, FStringView> CollisionType = GetCollisionMeshType(MeshUid, MeshUids);
		if (CollisionType.Get<0>() != EMeshCollisionType::None)
		{
			MeshToCollisionMeshMap.FindOrAdd(FString(CollisionType.Get<1>())).Emplace(MeshUid);
		}
	}
}


void UInterchangeGenericMeshPipeline::ExecutePreImportPipelineStaticMesh()
{
	check(CommonMeshesProperties.IsValid());

#if WITH_EDITOR
	//Make sure the generic pipeline will cover all staticmesh build settings when we import
	{
		TArray<const UClass*> Classes;
		Classes.Add(UInterchangeGenericCommonMeshesProperties::StaticClass());
		Classes.Add(UInterchangeGenericMeshPipeline::StaticClass());
		if (!ensure(DoClassesIncludeAllEditableStructProperties(Classes, FMeshBuildSettings::StaticStruct())))
		{
			UE_LOG(LogInterchangePipeline, Log, TEXT("UInterchangeGenericMeshPipeline: The generic pipeline does not cover all static mesh build options."));
		}
	}
#endif

	if (bImportStaticMeshes && (CommonMeshesProperties->ForceAllMeshAsType == EInterchangeForceMeshType::IFMT_None || CommonMeshesProperties->ForceAllMeshAsType == EInterchangeForceMeshType::IFMT_StaticMesh))
	{
		const bool bConvertSkeletalMeshToStaticMesh = (CommonMeshesProperties->ForceAllMeshAsType == EInterchangeForceMeshType::IFMT_StaticMesh);
		if (bCombineStaticMeshes)
		{
			// Combine all the static meshes

			bool bFoundMeshes = false;
			{
				// If baking transforms, get all the static mesh instance nodes, and group them by LOD
				TArray<FString> MeshUids;
				PipelineMeshesUtilities->GetAllStaticMeshInstance(MeshUids, bConvertSkeletalMeshToStaticMesh);

				TMap<int32, TArray<FString>> MeshUidsPerLodIndex;

				for (const FString& MeshUid : MeshUids)
				{
					const FInterchangeMeshInstance& MeshInstance = PipelineMeshesUtilities->GetMeshInstanceByUid(MeshUid);
					for (const auto& LodIndexAndSceneNodeContainer : MeshInstance.SceneNodePerLodIndex)
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

				// If we got some instances, create a static mesh factory node
				if (MeshUidsPerLodIndex.Num() > 0)
				{
					UInterchangeStaticMeshFactoryNode* StaticMeshFactoryNode = CreateStaticMeshFactoryNode(MeshUidsPerLodIndex);
					StaticMeshFactoryNodes.Add(StaticMeshFactoryNode);
					bFoundMeshes = true;
				}
			}

			if (!bFoundMeshes)
			{
				// If we haven't yet managed to build a factory node, look at static mesh geometry directly.
				TArray<FString> MeshUids;
				PipelineMeshesUtilities->GetAllStaticMeshGeometry(MeshUids, bConvertSkeletalMeshToStaticMesh);

				TMap<int32, TArray<FString>> MeshUidsPerLodIndex;

				for (const FString& MeshUid : MeshUids)
				{
					// MeshGeometry cannot have Lod since LODs are defined in the scene node
					const FInterchangeMeshGeometry& MeshGeometry = PipelineMeshesUtilities->GetMeshGeometryByUid(MeshUid);
					const int32 LodIndex = 0;
					TArray<FString>& TranslatedNodes = MeshUidsPerLodIndex.FindOrAdd(LodIndex);
					TranslatedNodes.Add(MeshGeometry.MeshUid);
				}

				if (MeshUidsPerLodIndex.Num() > 0)
				{
					UInterchangeStaticMeshFactoryNode* StaticMeshFactoryNode = CreateStaticMeshFactoryNode(MeshUidsPerLodIndex);
					StaticMeshFactoryNodes.Add(StaticMeshFactoryNode);
				}
			}

		}
		else
		{
			// Do not combine static meshes

			bool bFoundMeshes = false;
			{
				TArray<FString> MeshUids;
				PipelineMeshesUtilities->GetAllStaticMeshInstance(MeshUids, bConvertSkeletalMeshToStaticMesh);

				// Work out which meshes are collision meshes which correspond to another mesh
				TMap<FString, TArray<FString>> MeshToCollisionMeshMap;
				if (bImportCollisionAccordingToMeshName)
				{
					BuildMeshToCollisionMeshMap(MeshUids, MeshToCollisionMeshMap);
				}

				// Now iterate through each mesh UID, creating a new factory for each one
				for (const FString& MeshUid : MeshUids)
				{
					if (bImportCollisionAccordingToMeshName && IsCollisionMeshUid(MeshUid, MeshUids))
					{
						// If this is a collision mesh, don't add a factory; it will be added as part of another factory
						continue;
					}

					TMap<int32, TArray<FString>> MeshUidsPerLodIndex;

					const FInterchangeMeshInstance& MeshInstance = PipelineMeshesUtilities->GetMeshInstanceByUid(MeshUid);
					for (const auto& LodIndexAndSceneNodeContainer : MeshInstance.SceneNodePerLodIndex)
					{
						const int32 LodIndex = LodIndexAndSceneNodeContainer.Key;
						const FInterchangeLodSceneNodeContainer& SceneNodeContainer = LodIndexAndSceneNodeContainer.Value;

						TArray<FString>& TranslatedNodes = MeshUidsPerLodIndex.FindOrAdd(LodIndex);
						for (const UInterchangeSceneNode* SceneNode : SceneNodeContainer.SceneNodes)
						{
							TranslatedNodes.Add(SceneNode->GetUniqueID());
						}
					}

					if (MeshUidsPerLodIndex.Num() > 0)
					{
						if (bImportCollisionAccordingToMeshName)
						{
							if (const TArray<FString>* CorrespondingCollisionMeshes = MeshToCollisionMeshMap.Find(MeshUid))
							{
								MeshUidsPerLodIndex.FindOrAdd(0).Append(*CorrespondingCollisionMeshes);
							}
						}

						UInterchangeStaticMeshFactoryNode* StaticMeshFactoryNode = CreateStaticMeshFactoryNode(MeshUidsPerLodIndex);
						StaticMeshFactoryNodes.Add(StaticMeshFactoryNode);
						bFoundMeshes = true;
					}
				}
			}

			if (!bFoundMeshes)
			{
				TArray<FString> MeshUids;
				PipelineMeshesUtilities->GetAllStaticMeshGeometry(MeshUids, bConvertSkeletalMeshToStaticMesh);

				// Work out which meshes are collision meshes which correspond to another mesh
				TMap<FString, TArray<FString>> MeshToCollisionMeshMap;
				if (bImportCollisionAccordingToMeshName)
				{
					BuildMeshToCollisionMeshMap(MeshUids, MeshToCollisionMeshMap);
				}

				for (const FString& MeshUid : MeshUids)
				{
					if (bImportCollisionAccordingToMeshName && IsCollisionMeshUid(MeshUid, MeshUids))
					{
						// If this is a collision mesh, don't add a factory; it will be added as part of another factory
						continue;
					}

					TMap<int32, TArray<FString>> MeshUidsPerLodIndex;

					const FInterchangeMeshGeometry& MeshGeometry = PipelineMeshesUtilities->GetMeshGeometryByUid(MeshUid);
					const int32 LodIndex = 0;
					TArray<FString>& TranslatedNodes = MeshUidsPerLodIndex.FindOrAdd(LodIndex);
					TranslatedNodes.Add(MeshGeometry.MeshUid);

					if (MeshUidsPerLodIndex.Num() > 0)
					{
						if (bImportCollisionAccordingToMeshName)
						{
							if (const TArray<FString>* CorrespondingCollisionMeshes = MeshToCollisionMeshMap.Find(MeshUid))
							{
								MeshUidsPerLodIndex.FindOrAdd(0).Append(*CorrespondingCollisionMeshes);
							}
						}

						UInterchangeStaticMeshFactoryNode* StaticMeshFactoryNode = CreateStaticMeshFactoryNode(MeshUidsPerLodIndex);
						StaticMeshFactoryNodes.Add(StaticMeshFactoryNode);
					}
				}
			}
		}
	}
}

bool UInterchangeGenericMeshPipeline::MakeMeshFactoryNodeUidAndDisplayLabel(const TMap<int32, TArray<FString>>& MeshUidsPerLodIndex, int32 LodIndex, FString& NewNodeUid, FString& DisplayLabel)
{
	int32 SceneNodeCount = 0;

	if (!ensure(LodIndex >= 0 && MeshUidsPerLodIndex.Num() > LodIndex))
	{
		return false;
	}

	for (const TPair<int32, TArray<FString>>& LodIndexAndMeshUids : MeshUidsPerLodIndex)
	{
		if (LodIndex == LodIndexAndMeshUids.Key)
		{
			const TArray<FString>& Uids = LodIndexAndMeshUids.Value;
			if (Uids.Num() > 0)
			{
				const FString& Uid = Uids[0];
				const UInterchangeBaseNode* Node = BaseNodeContainer->GetNode(Uid);

				if (const UInterchangeMeshNode* MeshNode = Cast<const UInterchangeMeshNode>(Node))
				{
					DisplayLabel = Node->GetDisplayLabel();
					NewNodeUid = Uid;
					return true;
				}

				if (const UInterchangeSceneNode* SceneNode = Cast<const UInterchangeSceneNode>(Node))
				{
					FString RefMeshUid;
					if (SceneNode->GetCustomAssetInstanceUid(RefMeshUid))
					{
						const UInterchangeBaseNode* MeshNode = BaseNodeContainer->GetNode(RefMeshUid);
						if (MeshNode)
						{
							DisplayLabel = MeshNode->GetDisplayLabel();
							if (Uids.Num() == 1)
							{
								// If we are instancing one scene node, we want to use it to name the mesh
								DisplayLabel = SceneNode->GetDisplayLabel();
							}

							NewNodeUid = RefMeshUid;
							return true;
						}
					}
				}
			}

			// We found the lod but there is no valid Mesh node to return the Uid
			break;
		}
	}

	return false;
}

UInterchangeStaticMeshFactoryNode* UInterchangeGenericMeshPipeline::CreateStaticMeshFactoryNode(const TMap<int32, TArray<FString>>& MeshUidsPerLodIndex)
{
	check(CommonMeshesProperties.IsValid());
	if (MeshUidsPerLodIndex.Num() == 0)
	{
		return nullptr;
	}

	// Create the static mesh factory node, name it according to the first mesh node compositing the meshes
	FString StaticMeshUid_MeshNamePart;
	FString DisplayLabel;
	const int32 BaseLodIndex = 0;
	if (!MakeMeshFactoryNodeUidAndDisplayLabel(MeshUidsPerLodIndex, BaseLodIndex, StaticMeshUid_MeshNamePart, DisplayLabel))
	{
		// Log an error
		return nullptr;
	}

	const FString StaticMeshUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(StaticMeshUid_MeshNamePart);
	UInterchangeStaticMeshFactoryNode* StaticMeshFactoryNode = NewObject<UInterchangeStaticMeshFactoryNode>(BaseNodeContainer, NAME_None);
	if (!ensure(StaticMeshFactoryNode))
	{
		return nullptr;
	}

	StaticMeshFactoryNode->InitializeStaticMeshNode(StaticMeshUid, DisplayLabel, UStaticMesh::StaticClass()->GetName());
	BaseNodeContainer->AddNode(StaticMeshFactoryNode);

	AddLodDataToStaticMesh(StaticMeshFactoryNode, MeshUidsPerLodIndex);

	switch (CommonMeshesProperties->VertexColorImportOption)
	{
		case EInterchangeVertexColorImportOption::IVCIO_Replace:
		{
			StaticMeshFactoryNode->SetCustomVertexColorReplace(true);
		}
		break;
		case EInterchangeVertexColorImportOption::IVCIO_Ignore:
		{
			StaticMeshFactoryNode->SetCustomVertexColorIgnore(true);
		}
		break;
		case EInterchangeVertexColorImportOption::IVCIO_Override:
		{
			StaticMeshFactoryNode->SetCustomVertexColorOverride(CommonMeshesProperties->VertexOverrideColor);
		}
		break;
	}

	//Common meshes build options
	StaticMeshFactoryNode->SetCustomRecomputeNormals(CommonMeshesProperties->bRecomputeNormals);
	StaticMeshFactoryNode->SetCustomRecomputeTangents(CommonMeshesProperties->bRecomputeTangents);
	StaticMeshFactoryNode->SetCustomUseMikkTSpace(CommonMeshesProperties->bUseMikkTSpace);
	StaticMeshFactoryNode->SetCustomComputeWeightedNormals(CommonMeshesProperties->bComputeWeightedNormals);
	StaticMeshFactoryNode->SetCustomUseHighPrecisionTangentBasis(CommonMeshesProperties->bUseHighPrecisionTangentBasis);
	StaticMeshFactoryNode->SetCustomUseFullPrecisionUVs(CommonMeshesProperties->bUseFullPrecisionUVs);
	StaticMeshFactoryNode->SetCustomUseBackwardsCompatibleF16TruncUVs(CommonMeshesProperties->bUseBackwardsCompatibleF16TruncUVs);
	StaticMeshFactoryNode->SetCustomRemoveDegenerates(CommonMeshesProperties->bRemoveDegenerates);
	//Static meshes build options
	StaticMeshFactoryNode->SetCustomBuildReversedIndexBuffer(bBuildReversedIndexBuffer);
	StaticMeshFactoryNode->SetCustomGenerateLightmapUVs(bGenerateLightmapUVs);
	StaticMeshFactoryNode->SetCustomGenerateDistanceFieldAsIfTwoSided(bGenerateDistanceFieldAsIfTwoSided);
	StaticMeshFactoryNode->SetCustomSupportFaceRemap(bSupportFaceRemap);
	StaticMeshFactoryNode->SetCustomMinLightmapResolution(MinLightmapResolution);
	StaticMeshFactoryNode->SetCustomSrcLightmapIndex(SrcLightmapIndex);
	StaticMeshFactoryNode->SetCustomDstLightmapIndex(DstLightmapIndex);
	StaticMeshFactoryNode->SetCustomBuildScale3D(BuildScale3D);
	StaticMeshFactoryNode->SetCustomDistanceFieldResolutionScale(DistanceFieldResolutionScale);
	StaticMeshFactoryNode->SetCustomDistanceFieldReplacementMesh(DistanceFieldReplacementMesh.Get());
	StaticMeshFactoryNode->SetCustomMaxLumenMeshCards(MaxLumenMeshCards);
	StaticMeshFactoryNode->SetCustomBuildNanite(bBuildNanite);

	return StaticMeshFactoryNode;
}


UInterchangeStaticMeshLodDataNode* UInterchangeGenericMeshPipeline::CreateStaticMeshLodDataNode(const FString& NodeName, const FString& NodeUniqueID)
{
	FString DisplayLabel(NodeName);
	FString NodeUID(NodeUniqueID);
	UInterchangeStaticMeshLodDataNode* StaticMeshLodDataNode = NewObject<UInterchangeStaticMeshLodDataNode>(BaseNodeContainer, NAME_None);
	if (!ensure(StaticMeshLodDataNode))
	{
		// @TODO: log error
		return nullptr;
	}

	StaticMeshLodDataNode->InitializeNode(NodeUID, DisplayLabel, EInterchangeNodeContainerType::FactoryData);
	StaticMeshLodDataNode->SetOneConvexHullPerUCX(bOneConvexHullPerUCX);
	BaseNodeContainer->AddNode(StaticMeshLodDataNode);
	return StaticMeshLodDataNode;
}


void UInterchangeGenericMeshPipeline::AddLodDataToStaticMesh(UInterchangeStaticMeshFactoryNode* StaticMeshFactoryNode, const TMap<int32, TArray<FString>>& NodeUidsPerLodIndex)
{
	check(CommonMeshesProperties.IsValid());
	const FString StaticMeshFactoryUid = StaticMeshFactoryNode->GetUniqueID();

	for (const TPair<int32, TArray<FString>>& LodIndexAndNodeUids : NodeUidsPerLodIndex)
	{
		const int32 LodIndex = LodIndexAndNodeUids.Key;
		if (!CommonMeshesProperties->bImportLods && LodIndex > 0)
		{
			// If the pipeline should not import lods, skip any lod over base lod
			continue;
		}

		const TArray<FString>& NodeUids = LodIndexAndNodeUids.Value;

		// Create a lod data node with all the meshes for this LOD
		const FString StaticMeshLodDataName = TEXT("LodData") + FString::FromInt(LodIndex);
		const FString LODDataPrefix = TEXT("\\LodData") + (LodIndex > 0 ? FString::FromInt(LodIndex) : TEXT(""));
		const FString StaticMeshLodDataUniqueID = LODDataPrefix + StaticMeshFactoryUid;

		// Create LodData node if it doesn't already exist
		UInterchangeStaticMeshLodDataNode* LodDataNode = Cast<UInterchangeStaticMeshLodDataNode>(BaseNodeContainer->GetFactoryNode(StaticMeshLodDataUniqueID));
		if (!LodDataNode)
		{
			// Add the data for the LOD (all the mesh node fbx path, so we can find them when we will create the payload data)
			LodDataNode = CreateStaticMeshLodDataNode(StaticMeshLodDataName, StaticMeshLodDataUniqueID);
			BaseNodeContainer->SetNodeParentUid(StaticMeshLodDataUniqueID, StaticMeshFactoryUid);
			StaticMeshFactoryNode->AddLodDataUniqueId(StaticMeshLodDataUniqueID);
		}
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
					UInterchangeUserDefinedAttributesAPI::DuplicateAllUserDefinedAttribute(MeshDependencyNode, StaticMeshFactoryNode, bAddSourceNodeName);
					StaticMeshFactoryNode->AddTargetNodeUid(MeshDependency);
					StaticMeshFactoryNode->AddSocketUids(PipelineMeshesUtilities->GetMeshGeometryByUid(MeshDependency).AttachedSocketUids);
					MeshDependencyNode->AddTargetNodeUid(StaticMeshFactoryNode->GetUniqueID());

					MeshDependencyNode->GetSlotMaterialDependencies(SlotMaterialDependencies);
				}
				else
				{
					SceneNode->GetSlotMaterialDependencies(SlotMaterialDependencies);
				}

				UInterchangeUserDefinedAttributesAPI::DuplicateAllUserDefinedAttribute(SceneNode, StaticMeshFactoryNode, bAddSourceNodeName);
			}
			else if (const UInterchangeMeshNode* MeshNode = Cast<UInterchangeMeshNode>(BaseNodeContainer->GetNode(NodeUid)))
			{
				UInterchangeUserDefinedAttributesAPI::DuplicateAllUserDefinedAttribute(MeshNode, StaticMeshFactoryNode, bAddSourceNodeName);
				StaticMeshFactoryNode->AddTargetNodeUid(NodeUid);
				StaticMeshFactoryNode->AddSocketUids(PipelineMeshesUtilities->GetMeshGeometryByUid(NodeUid).AttachedSocketUids);
				MeshNode->AddTargetNodeUid(StaticMeshFactoryNode->GetUniqueID());
				
				MeshNode->GetSlotMaterialDependencies(SlotMaterialDependencies);
			}

			UE::Interchange::MeshesUtilities::ApplySlotMaterialDependencies(*StaticMeshFactoryNode, SlotMaterialDependencies, *BaseNodeContainer);

			if (bImportCollisionAccordingToMeshName)
			{
				TTuple<EMeshCollisionType, FStringView> MeshType = GetCollisionMeshType(NodeUid, NodeUids);
				switch (MeshType.Get<0>())
				{
				case EMeshCollisionType::None:
					LodDataNode->AddMeshUid(NodeUid);
					break;

				case EMeshCollisionType::Box:
					LodDataNode->AddBoxCollisionMeshUid(NodeUid);
					break;

				case EMeshCollisionType::Sphere:
					LodDataNode->AddSphereCollisionMeshUid(NodeUid);
					break;

				case EMeshCollisionType::Capsule:
					LodDataNode->AddCapsuleCollisionMeshUid(NodeUid);
					break;

				case EMeshCollisionType::Convex:
					LodDataNode->AddConvexCollisionMeshUid(NodeUid);
					break;
				}
			}
			else
			{
				LodDataNode->AddMeshUid(NodeUid);
			}
		}
	}
}
