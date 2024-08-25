// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "MaterialXSurfaceShader.h"
#include "InterchangeImportLog.h"

namespace mx = MaterialX;

FMaterialXSurfaceShader::FMaterialXSurfaceShader(UInterchangeBaseNodeContainer& BaseNodeContainer)
	: FMaterialXSurfaceShaderAbstract(BaseNodeContainer)
{
	NodeDefinition = mx::NodeDefinition::Surface;
}

TSharedRef<FMaterialXBase> FMaterialXSurfaceShader::MakeInstance(UInterchangeBaseNodeContainer& BaseNodeContainer)
{
	TSharedRef<FMaterialXSurfaceShader > Result = MakeShared<FMaterialXSurfaceShader >(BaseNodeContainer);
	Result->RegisterConnectNodeOutputToInputDelegates();
	return Result;
}

void FMaterialXSurfaceShader::Translate(MaterialX::NodePtr SurfaceNode)
{
	this->SurfaceShaderNode = SurfaceNode;

	UInterchangeFunctionCallShaderNode* FunctionSurfaceShaderNode = CreateFunctionCallShaderNode(SurfaceNode->getName().c_str(), UE::Interchange::MaterialX::IndexSurfaceShaders, uint8(EInterchangeMaterialXShaders::Surface));

	using namespace UE::Interchange::Materials;

	// BSDF
	ConnectNodeOutputToInput(mx::Surface::Input::Bsdf, FunctionSurfaceShaderNode, Surface::Parameters::BSDF.ToString(), nullptr);

	// EDF
	ConnectNodeOutputToInput(mx::Surface::Input::Edf, FunctionSurfaceShaderNode, Surface::Parameters::EDF.ToString(), nullptr);

	// Opacity
	ConnectNodeOutputToInput(mx::Surface::Input::Opacity, FunctionSurfaceShaderNode, Surface::Parameters::Opacity.ToString(), mx::SurfaceUnlit::DefaultValue::Float::Opacity);

	// Outputs
	if(!bIsSubstrateEnabled)
	{
		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ShaderGraphNode, Common::Parameters::BxDF.ToString(), FunctionSurfaceShaderNode->GetUniqueID());
	}
	else
	{
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, SubstrateMaterial::Parameters::FrontMaterial.ToString(), FunctionSurfaceShaderNode->GetUniqueID(), Surface::Substrate::Outputs::Surface.ToString());
		if(UInterchangeShaderPortsAPI::HasInput(FunctionSurfaceShaderNode, Surface::Parameters::Opacity))
		{
			UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, SubstrateMaterial::Parameters::OpacityMask.ToString(), FunctionSurfaceShaderNode->GetUniqueID(), Surface::Substrate::Outputs::Opacity.ToString());
		}
	}
}
#endif