// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "MaterialXUsdPreviewSurfaceShader.h"

namespace mx = MaterialX;

FMaterialXUsdPreviewSurfaceShader::FMaterialXUsdPreviewSurfaceShader(UInterchangeBaseNodeContainer& BaseNodeContainer)
	: FMaterialXSurfaceShaderAbstract(BaseNodeContainer)
{
	NodeDefinition = mx::NodeDefinition::UsdPreviewSurface;
}

TSharedRef<FMaterialXBase> FMaterialXUsdPreviewSurfaceShader::MakeInstance(UInterchangeBaseNodeContainer& BaseNodeContainer)
{
	TSharedRef<FMaterialXUsdPreviewSurfaceShader> Result = MakeShared<FMaterialXUsdPreviewSurfaceShader>(BaseNodeContainer);
	Result->RegisterConnectNodeOutputToInputDelegates();
	return Result;
}

void FMaterialXUsdPreviewSurfaceShader::Translate(MaterialX::NodePtr UsdPreviewSurfaceNode)
{
	this->SurfaceShaderNode = UsdPreviewSurfaceNode;

	using namespace UE::Interchange::Materials;

	UInterchangeFunctionCallShaderNode* UsdPreviewSurfaceShaderNode = CreateFunctionCallShaderNode(UsdPreviewSurfaceNode->getName().c_str(), UE::Interchange::MaterialX::IndexSurfaceShaders, uint8(EInterchangeMaterialXShaders::UsdPreviewSurface));

	// Inputs
	//Diffuse Color
	ConnectNodeOutputToInput(mx::UsdPreviewSurface::Input::DiffuseColor, UsdPreviewSurfaceShaderNode, UsdPreviewSurface::Parameters::DiffuseColor.ToString(), mx::UsdPreviewSurface::DefaultValue::Color3::DiffuseColor);

	//Emissive Color
	ConnectNodeOutputToInput(mx::UsdPreviewSurface::Input::EmissiveColor, UsdPreviewSurfaceShaderNode, UsdPreviewSurface::Parameters::EmissiveColor.ToString(), mx::UsdPreviewSurface::DefaultValue::Color3::EmissiveColor);

	//Specular Color
	ConnectNodeOutputToInput(mx::UsdPreviewSurface::Input::SpecularColor, UsdPreviewSurfaceShaderNode, UsdPreviewSurface::Parameters::SpecularColor.ToString(), mx::UsdPreviewSurface::DefaultValue::Color3::SpecularColor);

	//Metallic
	ConnectNodeOutputToInput(mx::UsdPreviewSurface::Input::Metallic, UsdPreviewSurfaceShaderNode, UsdPreviewSurface::Parameters::Metallic.ToString(), mx::UsdPreviewSurface::DefaultValue::Float::Metallic);

	//Roughness
	ConnectNodeOutputToInput(mx::UsdPreviewSurface::Input::Roughness, UsdPreviewSurfaceShaderNode, UsdPreviewSurface::Parameters::Roughness.ToString(), mx::UsdPreviewSurface::DefaultValue::Float::Roughness);

	//Clearcoat
	ConnectNodeOutputToInput(mx::UsdPreviewSurface::Input::Clearcoat, UsdPreviewSurfaceShaderNode, UsdPreviewSurface::Parameters::Clearcoat.ToString(), mx::UsdPreviewSurface::DefaultValue::Float::Clearcoat);

	//Clearcoat Roughness
	ConnectNodeOutputToInput(mx::UsdPreviewSurface::Input::ClearcoatRoughness, UsdPreviewSurfaceShaderNode, UsdPreviewSurface::Parameters::ClearcoatRoughness.ToString(), mx::UsdPreviewSurface::DefaultValue::Float::ClearcoatRoughness);

	//Opacity
	ConnectNodeOutputToInput(mx::UsdPreviewSurface::Input::Opacity, UsdPreviewSurfaceShaderNode, UsdPreviewSurface::Parameters::Opacity.ToString(), mx::UsdPreviewSurface::DefaultValue::Float::Opacity);

	//Opacity Threshold
	ConnectNodeOutputToInput(mx::UsdPreviewSurface::Input::OpacityThreshold, UsdPreviewSurfaceShaderNode, UsdPreviewSurface::Parameters::OpacityThreshold.ToString(), mx::UsdPreviewSurface::DefaultValue::Float::OpacityThreshold);

	//IOR
	ConnectNodeOutputToInput(mx::UsdPreviewSurface::Input::IOR, UsdPreviewSurfaceShaderNode, UsdPreviewSurface::Parameters::IOR.ToString(), mx::UsdPreviewSurface::DefaultValue::Float::IOR);

	//Normal
	ConnectNodeOutputToInput(mx::UsdPreviewSurface::Input::Normal, UsdPreviewSurfaceShaderNode, UsdPreviewSurface::Parameters::Normal.ToString(), mx::UsdPreviewSurface::DefaultValue::Vector3::Normal);

	//Displacement
	ConnectNodeOutputToInput(mx::UsdPreviewSurface::Input::Displacement, ShaderGraphNode, UsdPreviewSurface::Parameters::Displacement.ToString(), mx::UsdPreviewSurface::DefaultValue::Float::Displacement);

	//Occlusion
	ConnectNodeOutputToInput(mx::UsdPreviewSurface::Input::Occlusion, UsdPreviewSurfaceShaderNode, UsdPreviewSurface::Parameters::Occlusion.ToString(), mx::UsdPreviewSurface::DefaultValue::Float::Occlusion);

	if(!bIsSubstrateEnabled)
	// Outputs
	{
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::BaseColor.ToString(), UsdPreviewSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::BaseColor.ToString());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Metallic.ToString(), UsdPreviewSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Metallic.ToString());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Specular.ToString(), UsdPreviewSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Specular.ToString());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Roughness.ToString(), UsdPreviewSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Roughness.ToString());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::EmissiveColor.ToString(), UsdPreviewSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::EmissiveColor.ToString());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Normal.ToString(), UsdPreviewSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Normal.ToString());

		if(UInterchangeShaderPortsAPI::HasInput(UsdPreviewSurfaceShaderNode, UsdPreviewSurface::Parameters::Opacity))
		{
			UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Opacity.ToString(), UsdPreviewSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Opacity.ToString());
		}

		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Occlusion.ToString(), UsdPreviewSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Occlusion.ToString());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Refraction.ToString(), UsdPreviewSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Refraction.ToString());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, ClearCoat::Parameters::ClearCoat.ToString(), UsdPreviewSurfaceShaderNode->GetUniqueID(), ClearCoat::Parameters::ClearCoat.ToString());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, ClearCoat::Parameters::ClearCoatRoughness.ToString(), UsdPreviewSurfaceShaderNode->GetUniqueID(), ClearCoat::Parameters::ClearCoatRoughness.ToString());
	}
	else
	{
		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ShaderGraphNode, SubstrateMaterial::Parameters::FrontMaterial.ToString(), UsdPreviewSurfaceShaderNode->GetUniqueID());
	}
}
#endif