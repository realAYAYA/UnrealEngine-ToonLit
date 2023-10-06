// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "MaterialXSurfaceMaterial.h"
#include "InterchangeImportLog.h"
#include "MaterialX/MaterialXUtils/MaterialXManager.h"
#include "MaterialX/MaterialXUtils/MaterialXStandardSurfaceShader.h"
#include "MaterialX/MaterialXUtils/MaterialXSurfaceUnlitShader.h"

namespace mx = MaterialX;

FMaterialXSurfaceMaterial::FMaterialXSurfaceMaterial(UInterchangeBaseNodeContainer& BaseNodeContainer)
	: FMaterialXBase(BaseNodeContainer)
{}

TSharedRef<FMaterialXBase> FMaterialXSurfaceMaterial::MakeInstance(UInterchangeBaseNodeContainer& BaseNodeContainer)
{
	return MakeShared<FMaterialXSurfaceMaterial>(BaseNodeContainer);
}

void FMaterialXSurfaceMaterial::Translate(MaterialX::NodePtr SurfaceMaterialNode)
{
	bool bHasSurfaceShader = false;
	bool bIsShaderGraphInContainer = true;

	 //We initialize the ShaderGraph outside of the loop, because a surfacematerial has only up to 2 inputs:
	 //- a surfaceshader that we handle
	 //- a displacementshader not yet supported
	const FString ShaderGraphNodeUID = UInterchangeShaderNode::MakeNodeUid(ANSI_TO_TCHAR(SurfaceMaterialNode->getName().c_str()), FStringView{});
	UInterchangeShaderGraphNode* ShaderGraphNode = const_cast<UInterchangeShaderGraphNode*>(Cast<UInterchangeShaderGraphNode>(NodeContainer.GetNode(ShaderGraphNodeUID)));

	if(!ShaderGraphNode)
	{
		ShaderGraphNode = NewObject<UInterchangeShaderGraphNode>(&NodeContainer);
		ShaderGraphNode->InitializeNode(ShaderGraphNodeUID, SurfaceMaterialNode->getName().c_str(), EInterchangeNodeContainerType::TranslatedAsset);
		NodeContainer.SetNodeParentUid(ShaderGraphNodeUID, FString{});
		bIsShaderGraphInContainer = false;
	}

	for(mx::InputPtr Input : SurfaceMaterialNode->getInputs())
	{
		mx::NodePtr ConnectedNode = Input->getConnectedNode();

		if(ConnectedNode)
		{
			// we use here a static_cast instead of a dynamic cast, because we know for sure that the underlying type is a surfaceshader
			// otherwise the document is ill-formed and the Translator already handles it
			
			TSharedPtr<FMaterialXSurfaceShaderAbstract> ShaderTranslator = StaticCastSharedPtr<FMaterialXSurfaceShaderAbstract>(FMaterialXManager::GetInstance().GetShaderTranslator(ConnectedNode->getCategory().c_str(), NodeContainer));
			bHasSurfaceShader = ShaderTranslator.IsValid();
			if(bHasSurfaceShader)
			{
				ShaderTranslator->ShaderGraphNode = ShaderGraphNode;
				ShaderTranslator->Translate(ConnectedNode);
			}
		}
	}

	// We only add the ShaderGraph to the container if we found a supported surfaceshader
	if(bHasSurfaceShader && !bIsShaderGraphInContainer)
	{
		NodeContainer.AddNode(ShaderGraphNode);
	}
	else
	{
		UE_LOG(LogInterchangeImport, Warning, TEXT("the surfaceshader of <%s> is not supported"), ANSI_TO_TCHAR(SurfaceMaterialNode->getName().c_str()));
	}
}

#endif