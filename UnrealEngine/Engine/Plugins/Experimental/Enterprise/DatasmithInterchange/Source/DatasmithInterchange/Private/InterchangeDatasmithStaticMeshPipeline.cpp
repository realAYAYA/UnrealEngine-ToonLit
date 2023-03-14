// Copyright Epic Games, Inc. All Rights Reserved. 

#include "InterchangeDatasmithStaticMeshPipeline.h"

#include "InterchangeDatasmithUtils.h"

#include "InterchangeMaterialFactoryNode.h"
#include "InterchangeStaticMeshFactoryNode.h"
#include "InterchangeMeshNode.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

void UInterchangeDatasmithStaticMeshPipeline::ExecutePreImportPipeline(UInterchangeBaseNodeContainer* NodeContainer, const TArray<UInterchangeSourceData*>& InSourceDatas)
{
	using namespace UE::DatasmithInterchange;

	Super::ExecutePreImportPipeline(NodeContainer, InSourceDatas);

	// Add material factory dependencies for meshes where all slots are filled with the same material
	for (UInterchangeStaticMeshFactoryNode* MeshFactoryNode : NodeUtils::GetNodes<UInterchangeStaticMeshFactoryNode>(NodeContainer))
	{
		TArray<FString> TargetNodes;
		MeshFactoryNode->GetTargetNodeUids(TargetNodes);
		if (TargetNodes.Num() == 0)
		{
			continue;
		}

		const UInterchangeMeshNode* MeshNode = Cast< UInterchangeMeshNode>(NodeContainer->GetNode(TargetNodes[0]));
		if (!MeshNode)
		{
			continue;
		}

		FString MaterialUid;
		if (!MeshNode->GetStringAttribute(MeshUtils::MeshMaterialAttrName, MaterialUid))
		{
			continue;
		}

		const FString MaterialFactoryUid = UInterchangeMaterialFactoryNode::GetMaterialFactoryNodeUidFromMaterialNodeUid(MaterialUid);
		MeshFactoryNode->AddFactoryDependencyUid(MaterialFactoryUid);
		MeshFactoryNode->AddStringAttribute(MeshUtils::MeshMaterialAttrName, MaterialFactoryUid);
	}
}

void UInterchangeDatasmithStaticMeshPipeline::ExecutePostImportPipeline(const UInterchangeBaseNodeContainer* NodeContainer, const FString& FactoryNodeKey, UObject* CreatedAsset, bool bIsAReimport)
{
	using namespace UE::DatasmithInterchange;

	if (!NodeContainer || !CreatedAsset)
	{
		return;
	}

	Super::ExecutePostImportPipeline(NodeContainer, FactoryNodeKey, CreatedAsset, bIsAReimport);

	// If applicable, update FStaticMaterial of newly create mesh
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(CreatedAsset);
	if (!StaticMesh)
	{
		return;
	}

	const UInterchangeStaticMeshFactoryNode* FactoryNode = Cast< UInterchangeStaticMeshFactoryNode>(NodeContainer->GetFactoryNode(FactoryNodeKey));
	if (!FactoryNode)
	{
		return;
	}

	FString MaterialFactoryUid;
	if (!FactoryNode->GetStringAttribute(MeshUtils::MeshMaterialAttrName, MaterialFactoryUid))
	{
		return;
	}

	const UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode = Cast< UInterchangeBaseMaterialFactoryNode>(NodeContainer->GetFactoryNode(MaterialFactoryUid));
	if (!MaterialFactoryNode)
	{
		return;
	}

	FSoftObjectPath MaterialFactoryNodeReferenceObject;
	MaterialFactoryNode->GetCustomReferenceObject(MaterialFactoryNodeReferenceObject);
	if (UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(MaterialFactoryNodeReferenceObject.ResolveObject()))
	{
		TArray<FStaticMaterial>& StaticMaterials = StaticMesh->GetStaticMaterials();
		for (FStaticMaterial& StaticMaterial : StaticMaterials)
		{
			StaticMaterial.MaterialInterface = MaterialInterface;
		}
	}
}
