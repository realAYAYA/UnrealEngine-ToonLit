// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "MaterialXSurfaceUnlitShader.h"

#include "Engine/EngineTypes.h"

namespace mx = MaterialX;

FMaterialXSurfaceUnlitShader::FMaterialXSurfaceUnlitShader(UInterchangeBaseNodeContainer& BaseNodeContainer)
	: FMaterialXSurfaceShaderAbstract(BaseNodeContainer)
{
	NodeDefinition = mx::NodeDefinition::SurfaceUnlit;
}

TSharedRef<FMaterialXBase> FMaterialXSurfaceUnlitShader::MakeInstance(UInterchangeBaseNodeContainer& BaseNodeContainer)
{
	TSharedRef<FMaterialXSurfaceUnlitShader> Result = MakeShared<FMaterialXSurfaceUnlitShader>(BaseNodeContainer);
	Result->RegisterConnectNodeOutputToInputDelegates();
	return Result;
}

void FMaterialXSurfaceUnlitShader::Translate(mx::NodePtr SurfaceUnlitNode)
{
	this->SurfaceShaderNode = SurfaceUnlitNode;

	UInterchangeFunctionCallShaderNode* SurfaceUnlitShaderNode = CreateFunctionCallShaderNode(SurfaceUnlitNode->getName().c_str(), UE::Interchange::MaterialX::IndexSurfaceShaders, uint8(EInterchangeMaterialXShaders::SurfaceUnlit));

	using namespace UE::Interchange::Materials;

	//Emission
	ConnectNodeOutputToInput(mx::SurfaceUnlit::Input::Emission, SurfaceUnlitShaderNode, SurfaceUnlit::Parameters::Emission.ToString(), mx::SurfaceUnlit::DefaultValue::Float::Emission);

	//Emission Color
	ConnectNodeOutputToInput(mx::SurfaceUnlit::Input::EmissionColor, SurfaceUnlitShaderNode, SurfaceUnlit::Parameters::EmissionColor.ToString(), mx::SurfaceUnlit::DefaultValue::Color3::EmissionColor);

	//Opacity
	ConnectNodeOutputToInput(mx::SurfaceUnlit::Input::Opacity, SurfaceUnlitShaderNode, SurfaceUnlit::Parameters::Opacity.ToString(), mx::SurfaceUnlit::DefaultValue::Float::Opacity);

	//Transmission
	ConnectNodeOutputToInput(mx::SurfaceUnlit::Input::Transmission, SurfaceUnlitShaderNode, SurfaceUnlit::Parameters::Transmission.ToString(), mx::SurfaceUnlit::DefaultValue::Float::Transmission);

	//Transmission Color
	ConnectNodeOutputToInput(mx::SurfaceUnlit::Input::TransmissionColor, SurfaceUnlitShaderNode, SurfaceUnlit::Parameters::TransmissionColor.ToString(), mx::SurfaceUnlit::DefaultValue::Color3::TransmissionColor);

	if(!bIsSubstrateEnabled)
	{
		// Outputs
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::EmissiveColor.ToString(), SurfaceUnlitShaderNode->GetUniqueID(), PBRMR::Parameters::EmissiveColor.ToString());

		//We can't have both Opacity and Opacity Mask we need to make a choice	
		if(UInterchangeShaderPortsAPI::HasInput(SurfaceUnlitShaderNode, SurfaceUnlit::Parameters::Transmission))
		{
			UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Opacity.ToString(), SurfaceUnlitShaderNode->GetUniqueID(), PBRMR::Parameters::Opacity.ToString());
		}
		else if(UInterchangeShaderPortsAPI::HasInput(SurfaceUnlitShaderNode, SurfaceUnlit::Parameters::Opacity))
		{
			UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Opacity.ToString(), SurfaceUnlitShaderNode->GetUniqueID(), SurfaceUnlit::Outputs::OpacityMask.ToString());
			ShaderGraphNode->SetCustomOpacityMaskClipValue(1.f); // just to connect to the opacity mask
		}
	}
	else
	{
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, SubstrateMaterial::Parameters::FrontMaterial.ToString(), SurfaceUnlitShaderNode->GetUniqueID(), SurfaceUnlit::Substrate::Outputs::SurfaceUnlit.ToString());
		if(UInterchangeShaderPortsAPI::HasInput(SurfaceUnlitShaderNode, SurfaceUnlit::Parameters::Transmission))
		{
			ShaderGraphNode->SetCustomBlendMode(EBlendMode::BLEND_TranslucentColoredTransmittance);
		}
		else if(UInterchangeShaderPortsAPI::HasInput(SurfaceUnlitShaderNode, SurfaceUnlit::Parameters::Opacity))
		{
			UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, SubstrateMaterial::Parameters::OpacityMask.ToString(), SurfaceUnlitShaderNode->GetUniqueID(), SurfaceUnlit::Substrate::Outputs::OpacityMask.ToString());
			ShaderGraphNode->SetCustomBlendMode(EBlendMode::BLEND_Masked);
		}
	}
}
#endif