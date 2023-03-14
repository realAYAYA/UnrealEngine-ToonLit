// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeDatasmithPipeline.h"

#include "InterchangeDatasmithAreaLightNode.h"
#include "InterchangeDatasmithAreaLightFactoryNode.h"
#include "InterchangeDatasmithLevelPipeline.h"
#include "InterchangeDatasmithMaterialPipeline.h"
#include "InterchangeDatasmithSceneNode.h"
#include "InterchangeDatasmithSceneFactoryNode.h"
#include "InterchangeDatasmithStaticMeshPipeline.h"
#include "InterchangeDatasmithTexturePipeline.h"
#include "InterchangeDatasmithUtils.h"

#include "InterchangeAnimationTrackSetFactoryNode.h"
#include "InterchangeGenericAnimationPipeline.h"
#include "InterchangeGenericScenesPipeline.h"
#include "InterchangeMaterialFactoryNode.h"
#include "InterchangeSceneVariantSetsFactoryNode.h"
#include "InterchangeStaticMeshFactoryNode.h"
#include "InterchangeTexture2DNode.h"
#include "InterchangeTextureFactoryNode.h"
#include "InterchangeTexture2DFactoryNode.h"

#include "ExternalSource.h"
#include "DatasmithAreaLightActor.h"
#include "DatasmithScene.h"
#include "DatasmithSceneXmlWriter.h"

#if WITH_EDITOR
#include "DatasmithImporter.h"
#include "DatasmithImportContext.h"
#include "DatasmithStaticMeshImporter.h"
#include "Misc/ScopedSlowTask.h"
#include "Utility/DatasmithImporterUtils.h"
#endif //WITH_EDITOR

#define LOCTEXT_NAMESPACE "InterchangeDatasmithPipeline"

UInterchangeDatasmithPipeline::UInterchangeDatasmithPipeline()
{
	CommonSkeletalMeshesAndAnimationsProperties = CreateDefaultSubobject<UInterchangeGenericCommonSkeletalMeshesAndAnimationsProperties>("CommonSkeletalMeshesAndAnimationsProperties");
	CommonSkeletalMeshesAndAnimationsProperties->bImportMeshesInBoneHierarchy = false;
	
	CommonMeshesProperties = CreateDefaultSubobject<UInterchangeGenericCommonMeshesProperties>("CommonMeshesProperties");
	CommonMeshesProperties->bBakeMeshes = false;

	TexturePipeline = CreateDefaultSubobject<UInterchangeDatasmithTexturePipeline>("DatasmithTexturePipeline");
	MaterialPipeline = CreateDefaultSubobject<UInterchangeDatasmithMaterialPipeline>("DatasmithMaterialPipeline");
	MeshPipeline = CreateDefaultSubobject<UInterchangeDatasmithStaticMeshPipeline>("DatasmithMeshPipeline");
	LevelPipeline = CreateDefaultSubobject<UInterchangeDatasmithLevelPipeline>("DatasmithLevelPipeline");
	AnimationPipeline = CreateDefaultSubobject<UInterchangeGenericAnimationPipeline>("AnimationPipeline");

	MeshPipeline->CommonMeshesProperties = CommonMeshesProperties;
	MeshPipeline->CommonSkeletalMeshesAndAnimationsProperties = CommonSkeletalMeshesAndAnimationsProperties;
}

void UInterchangeDatasmithPipeline::ExecutePreImportPipeline(UInterchangeBaseNodeContainer* InBaseNodeContainer, const TArray<UInterchangeSourceData*>& SourceDatas)
{
	using namespace UE::DatasmithInterchange;

	BaseNodeContainer = InBaseNodeContainer;

	TArray<UInterchangeDatasmithSceneNode*> DatasmithSceneNodes = NodeUtils::GetNodes<UInterchangeDatasmithSceneNode>(BaseNodeContainer);
	if (!ensure(DatasmithSceneNodes.Num() == 1))
	{
		return;
	}

	auto ExecutePreImportPipelineFunc = [this,SourceDatas](UInterchangePipelineBase* Pipeline)
	{
		if (Pipeline)
		{
			Pipeline->SetResultsContainer(this->Results);
			Pipeline->ScriptedExecutePreImportPipeline(this->BaseNodeContainer, SourceDatas);
		}
	};

	ensure(Results);
	ExecutePreImportPipelineFunc(TexturePipeline);
	ExecutePreImportPipelineFunc(MaterialPipeline);
	ExecutePreImportPipelineFunc(MeshPipeline);
	ExecutePreImportPipelineFunc(LevelPipeline);
	ExecutePreImportPipelineFunc(AnimationPipeline);

	const UInterchangeDatasmithSceneNode* DatasmithSceneNode = DatasmithSceneNodes[0];
	const FString PackageSubPath = DatasmithSceneNode->GetDisplayLabel();

	TArray<FString> DependenciesUids;

	// Textures
	for (UInterchangeTextureFactoryNode* TextureFactoryNode : NodeUtils::GetNodes<UInterchangeTextureFactoryNode>(BaseNodeContainer))
	{
		DependenciesUids.Add(TextureFactoryNode->GetUniqueID());
		TextureFactoryNode->SetCustomSubPath(FPaths::Combine(PackageSubPath, "Textures"));
		TextureFactoryNode->SetEnabled(true);
	}

	// Materials
	for (UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode : NodeUtils::GetNodes<UInterchangeBaseMaterialFactoryNode>(BaseNodeContainer))
	{
		DependenciesUids.Add(MaterialFactoryNode->GetUniqueID());
		MaterialFactoryNode->SetCustomSubPath(FPaths::Combine(PackageSubPath, "Materials"));
		MaterialFactoryNode->SetEnabled(true);
	}

	// StaticMeshes
	for (UInterchangeStaticMeshFactoryNode* StaticMeshFactoryNode : NodeUtils::GetNodes<UInterchangeStaticMeshFactoryNode>(BaseNodeContainer))
	{
		DependenciesUids.Add(StaticMeshFactoryNode->GetUniqueID());
		StaticMeshFactoryNode->SetCustomSubPath(FPaths::Combine(PackageSubPath, "Geometries"));
		StaticMeshFactoryNode->SetEnabled(true);
	}

	// LevelSequences
	for (UInterchangeAnimationTrackSetFactoryNode* AnimationTrackSetFactoryNode : NodeUtils::GetNodes<UInterchangeAnimationTrackSetFactoryNode>(BaseNodeContainer))
	{
		DependenciesUids.Add(AnimationTrackSetFactoryNode->GetUniqueID());
		AnimationTrackSetFactoryNode->SetCustomSubPath(FPaths::Combine(PackageSubPath, "Animations"));
		AnimationTrackSetFactoryNode->SetEnabled(true);
	}

	// LevelVariantSets
	for (UInterchangeSceneVariantSetsFactoryNode* LevelVariantSetFactoryNode : NodeUtils::GetNodes<UInterchangeSceneVariantSetsFactoryNode>(BaseNodeContainer))
	{
		DependenciesUids.Add(LevelVariantSetFactoryNode->GetUniqueID());
		LevelVariantSetFactoryNode->SetCustomSubPath(FPaths::Combine(PackageSubPath, "Variants"));
		LevelVariantSetFactoryNode->SetEnabled(true);
	}

	// Datasmith Scene
	{
		const FString DatasmithSceneUid = TEXT("Factory_") + DatasmithSceneNode->GetUniqueID();
		const FString DisplayLabel = DatasmithSceneNode->GetDisplayLabel();
		UInterchangeDatasmithSceneFactoryNode* DatasmithSceneFactoryNode = NewObject<UInterchangeDatasmithSceneFactoryNode>(BaseNodeContainer, NAME_None);
		if (!ensure(DatasmithSceneFactoryNode))
		{
			return;
		}

		DatasmithSceneFactoryNode->InitializeDatasmithFactorySceneNode(DatasmithSceneUid, DisplayLabel, UDatasmithScene::StaticClass()->GetName());
		DatasmithSceneFactoryNode->SetCustomSubPath(PackageSubPath);
		DatasmithSceneFactoryNode->AddTargetNodeUid(DatasmithSceneNode->GetUniqueID());
		DatasmithSceneNode->AddTargetNodeUid(DatasmithSceneFactoryNode->GetUniqueID());

		for (const FString& Uid : DependenciesUids)
		{
			DatasmithSceneFactoryNode->AddFactoryDependencyUid(Uid);
		}

		BaseNodeContainer->AddNode(DatasmithSceneFactoryNode);
	}
}

void UInterchangeDatasmithPipeline::ExecutePostImportPipeline(const UInterchangeBaseNodeContainer* InBaseNodeContainer, const FString& NodeKey, UObject* CreatedAsset, bool bIsAReimport)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UInterchangeDatasmithPipeline::ExecutePostImportPipeline);

	if (TexturePipeline)
	{
		TexturePipeline->ScriptedExecutePostImportPipeline(InBaseNodeContainer, NodeKey, CreatedAsset, bIsAReimport);
	}

	if (MaterialPipeline)
	{
		MaterialPipeline->ScriptedExecutePostImportPipeline(InBaseNodeContainer, NodeKey, CreatedAsset, bIsAReimport);
	}

	if (MeshPipeline)
	{
		MeshPipeline->ScriptedExecutePostImportPipeline(InBaseNodeContainer, NodeKey, CreatedAsset, bIsAReimport);
	}

	if (LevelPipeline)
	{
		LevelPipeline->ScriptedExecutePostImportPipeline(InBaseNodeContainer, NodeKey, CreatedAsset, bIsAReimport);
	}

	if (AnimationPipeline)
	{
		AnimationPipeline->ScriptedExecutePostImportPipeline(InBaseNodeContainer, NodeKey, CreatedAsset, bIsAReimport);
	}

	if (UDatasmithScene* DatasmithSceneAsset = Cast<UDatasmithScene>(CreatedAsset))
	{
		PostImportDatasmithSceneAsset(*DatasmithSceneAsset);
	}
}

void UInterchangeDatasmithPipeline::PostImportDatasmithSceneAsset(UDatasmithScene& DatasmithSceneAsset)
{
	using namespace UE::DatasmithInterchange;

#if WITH_EDITORONLY_DATA
	TArray<UInterchangeDatasmithSceneNode*> DatasmithSceneNodes = NodeUtils::GetNodes<UInterchangeDatasmithSceneNode>(BaseNodeContainer);
	if (!ensure(DatasmithSceneNodes.Num() == 1))
	{
		// TODO: Warn more than one Datasmith scene asset
		return;
	}

	TArray< uint8 > Bytes;
	FMemoryWriter MemoryWriter(Bytes, true);

	FDatasmithSceneXmlWriter DatasmithSceneXmlWriter;
	DatasmithSceneXmlWriter.Serialize(DatasmithSceneNodes[0]->DatasmithScene.ToSharedRef(), MemoryWriter);

	DatasmithSceneAsset.DatasmithSceneBulkData.Lock(LOCK_READ_WRITE);

	uint8* Dest = reinterpret_cast<uint8*>(DatasmithSceneAsset.DatasmithSceneBulkData.Realloc(Bytes.Num()));

	FPlatformMemory::Memcpy(Dest, Bytes.GetData(), Bytes.Num());

	DatasmithSceneAsset.DatasmithSceneBulkData.Unlock();

	// Todo: Fill up imported DatasmithScene with created assets
#endif
}

#undef LOCTEXT_NAMESPACE