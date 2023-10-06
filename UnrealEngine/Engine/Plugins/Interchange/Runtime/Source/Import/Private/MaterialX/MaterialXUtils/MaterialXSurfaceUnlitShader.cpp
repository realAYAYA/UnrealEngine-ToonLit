// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "MaterialXSurfaceUnlitShader.h"

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

	UInterchangeFunctionCallShaderNode* SurfaceUnlitShaderNode;

	const FString NodeUID = UInterchangeShaderNode::MakeNodeUid(ANSI_TO_TCHAR(SurfaceUnlitNode->getName().c_str()), FStringView{});

	if(SurfaceUnlitShaderNode = const_cast<UInterchangeFunctionCallShaderNode*>(Cast<UInterchangeFunctionCallShaderNode>(NodeContainer.GetNode(NodeUID))); !SurfaceUnlitShaderNode)
	{
		const FString NodeName = SurfaceUnlitNode->getName().c_str();
		SurfaceUnlitShaderNode = NewObject<UInterchangeFunctionCallShaderNode>(&NodeContainer);
		SurfaceUnlitShaderNode->InitializeNode(NodeUID, NodeName, EInterchangeNodeContainerType::TranslatedAsset);

		SurfaceUnlitShaderNode->SetCustomMaterialFunction(TEXT("/Interchange/Functions/MX_SurfaceUnlit.MX_SurfaceUnlit"));
		NodeContainer.AddNode(SurfaceUnlitShaderNode);

		ShaderNodes.Add({ NodeName, SurfaceUnlitNode->getNodeDef(mx::EMPTY_STRING, true)->getActiveOutputs()[0]->getName().c_str() }, SurfaceUnlitShaderNode);
	}

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

	// Outputs
	UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::EmissiveColor.ToString(), SurfaceUnlitShaderNode->GetUniqueID(), PBRMR::Parameters::EmissiveColor.ToString());

	//We can't have both Opacity and Opacity Mask we need to make a choice	
	if(UInterchangeShaderPortsAPI::HasInput(SurfaceUnlitShaderNode, SurfaceUnlit::Parameters::Transmission))
	{
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Opacity.ToString(), SurfaceUnlitShaderNode->GetUniqueID(), PBRMR::Parameters::Opacity.ToString());
	}
	else if(UInterchangeShaderPortsAPI::HasInput(SurfaceUnlitShaderNode, SurfaceUnlit::Parameters::Opacity))
	{
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Opacity.ToString(), SurfaceUnlitShaderNode->GetUniqueID(), TEXT("OpacityMask"));
		ShaderGraphNode->SetCustomOpacityMaskClipValue(1.f); // just to connect to the opacity mask
	}
}
#endif