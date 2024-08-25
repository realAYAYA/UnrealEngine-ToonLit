// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "MaterialXOpenPBRSurfaceShader.h"

#include "Engine/EngineTypes.h"

namespace mx = MaterialX;

FMaterialXOpenPBRSurfaceShader::FMaterialXOpenPBRSurfaceShader(UInterchangeBaseNodeContainer& BaseNodeContainer)
	: FMaterialXSurfaceShaderAbstract(BaseNodeContainer)
{
	NodeDefinition = mx::NodeDefinition::OpenPBRSurface;
}

TSharedRef<FMaterialXBase> FMaterialXOpenPBRSurfaceShader::MakeInstance(UInterchangeBaseNodeContainer& BaseNodeContainer)
{
	TSharedRef<FMaterialXOpenPBRSurfaceShader> Result = MakeShared<FMaterialXOpenPBRSurfaceShader>(BaseNodeContainer);
	Result->RegisterConnectNodeOutputToInputDelegates();
	return Result;
}

void FMaterialXOpenPBRSurfaceShader::Translate(MaterialX::NodePtr OpenPBRSurfaceNode)
{
	using namespace UE::Interchange::Materials;
	using namespace mx::OpenPBRSurface;

	this->SurfaceShaderNode = OpenPBRSurfaceNode;
	UInterchangeFunctionCallShaderNode* OpenPBRSurfaceShaderNode = CreateFunctionCallShaderNode(SurfaceShaderNode->getName().c_str(), UE::Interchange::MaterialX::IndexSurfaceShaders, uint8(EInterchangeMaterialXShaders::OpenPBRSurface));

	const bool bTangentSpace = true;

	// Inputs
	//Base
	ConnectNodeOutputToInput(Input::BaseWeight, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::BaseWeight.ToString(), DefaultValue::BaseWeight);
	ConnectNodeOutputToInput(Input::BaseColor, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::BaseColor.ToString(), DefaultValue::BaseColor);
	ConnectNodeOutputToInput(Input::BaseMetalness, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::BaseMetalness.ToString(), DefaultValue::BaseMetalness);
	ConnectNodeOutputToInput(Input::BaseRoughness, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::BaseRoughness.ToString(), DefaultValue::BaseRoughness);

	//Specular
	ConnectNodeOutputToInput(Input::SpecularAnisotropy, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::SpecularAnisotropy.ToString(), DefaultValue::SpecularAnisotropy);
	ConnectNodeOutputToInput(Input::SpecularColor, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::SpecularColor.ToString(), DefaultValue::SpecularColor);
	ConnectNodeOutputToInput(Input::SpecularIOR, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::SpecularIOR.ToString(), DefaultValue::SpecularIOR);
	ConnectNodeOutputToInput(Input::SpecularIORLevel, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::SpecularIORLevel.ToString(), DefaultValue::SpecularIORLevel);
	ConnectNodeOutputToInput(Input::SpecularRotation, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::SpecularRotation.ToString(), DefaultValue::SpecularRotation);
	ConnectNodeOutputToInput(Input::SpecularRoughness, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::SpecularRoughness.ToString(), DefaultValue::SpecularRoughness);
	ConnectNodeOutputToInput(Input::SpecularWeight, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::SpecularWeight.ToString(), DefaultValue::SpecularWeight);

	//Coat
	ConnectNodeOutputToInput(Input::CoatAnisotropy, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::CoatAnisotropy.ToString(), DefaultValue::CoatAnisotropy);
	ConnectNodeOutputToInput(Input::CoatColor, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::CoatColor.ToString(), DefaultValue::CoatColor);
	ConnectNodeOutputToInput(Input::CoatIOR, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::CoatIOR.ToString(), DefaultValue::CoatIOR);
	ConnectNodeOutputToInput(Input::CoatIORLevel, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::CoatIORLevel.ToString(), DefaultValue::CoatIORLevel);
	ConnectNodeOutputToInput(Input::CoatRotation, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::CoatRotation.ToString(), DefaultValue::CoatRotation);
	ConnectNodeOutputToInput(Input::CoatRoughness, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::CoatRoughness.ToString(), DefaultValue::CoatRoughness);
	ConnectNodeOutputToInput(Input::CoatWeight, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::CoatWeight.ToString(), DefaultValue::CoatWeight);

	//Emission
	ConnectNodeOutputToInput(Input::EmissionColor, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::EmissionColor.ToString(), DefaultValue::EmissionColor);
	ConnectNodeOutputToInput(Input::EmissionLuminance, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::EmissionLuminance.ToString(), DefaultValue::EmissionLuminance);

	//ThinFilm
	ConnectNodeOutputToInput(Input::ThinFilmIOR, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::ThinFilmIOR.ToString(), DefaultValue::ThinFilmIOR);
	ConnectNodeOutputToInput(Input::ThinFilmThickness, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::ThinFilmThickness.ToString(), DefaultValue::ThinFilmThickness);

	//Transmission
	ConnectNodeOutputToInput(Input::TransmissionColor, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::TransmissionColor.ToString(), DefaultValue::TransmissionColor);
	ConnectNodeOutputToInput(Input::TransmissionDepth, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::TransmissionDepth.ToString(), DefaultValue::TransmissionDepth);
	ConnectNodeOutputToInput(Input::TransmissionDispersionScale, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::TransmissionDispersionScale.ToString(), DefaultValue::TransmissionDispersionScale);
	ConnectNodeOutputToInput(Input::TransmissionDispersionAbbeNumber, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::TransmissionDispersionAbbeNumber.ToString(), DefaultValue::TransmissionDispersionAbbeNumber);
	ConnectNodeOutputToInput(Input::TransmissionScatter, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::TransmissionScatter.ToString(), DefaultValue::TransmissionScatter);
	ConnectNodeOutputToInput(Input::TransmissionScatterAnisotropy, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::TransmissionScatterAnisotropy.ToString(), DefaultValue::TransmissionScatterAnisotropy);
	ConnectNodeOutputToInput(Input::TransmissionWeight, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::TransmissionWeight.ToString(), DefaultValue::TransmissionWeight);

	//Geometry
	ConnectNodeOutputToInput(Input::GeometryCoatNormal, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::GeometryCoatNormal.ToString(), DefaultValue::GeometryCoatNormal, bTangentSpace);
	ConnectNodeOutputToInput(Input::GeometryNormal, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::GeometryNormal.ToString(), DefaultValue::GeometryNormal, bTangentSpace);
	ConnectNodeOutputToInput(Input::GeometryOpacity, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::GeometryOpacity.ToString(), DefaultValue::GeometryOpacity);
	ConnectNodeOutputToInput(Input::GeometryTangent, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::GeometryTangent.ToString(), DefaultValue::GeometryTangent, bTangentSpace);
	ConnectNodeOutputToInput(Input::GeometryThinWalled, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::GeometryThinWalled.ToString(), DefaultValue::GeometryThinWalled);
	
	//Fuzz
	ConnectNodeOutputToInput(Input::FuzzColor, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::FuzzColor.ToString(), DefaultValue::FuzzColor);
	ConnectNodeOutputToInput(Input::FuzzRoughness, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::FuzzRoughness.ToString(), DefaultValue::FuzzRoughness);
	ConnectNodeOutputToInput(Input::FuzzWeight, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::FuzzWeight.ToString(), DefaultValue::FuzzWeight);

	// We can't have Subsurface inputs if we have Transmission inputs
	if(UInterchangeShaderPortsAPI::HasInput(OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::TransmissionWeight))
	{
		OpenPBRSurfaceShaderNode->AddInt32Attribute(UE::Interchange::MaterialX::Attributes::EnumType, UE::Interchange::MaterialX::IndexSurfaceShaders);
		OpenPBRSurfaceShaderNode->AddInt32Attribute(UE::Interchange::MaterialX::Attributes::EnumValue, int32(EInterchangeMaterialXShaders::OpenPBRSurfaceTransmission));
		ShaderGraphNode->SetCustomBlendMode(EBlendMode::BLEND_TranslucentColoredTransmittance);
	}
	else
	{
		//Subsurface
		ConnectNodeOutputToInput(Input::SubsurfaceAnisotropy, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::SubsurfaceAnisotropy.ToString(), DefaultValue::SubsurfaceAnisotropy);
		ConnectNodeOutputToInput(Input::SubsurfaceColor, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::SubsurfaceColor.ToString(), DefaultValue::SubsurfaceColor);
		ConnectNodeOutputToInput(Input::SubsurfaceRadius, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::SubsurfaceRadius.ToString(), DefaultValue::SubsurfaceRadius);
		ConnectNodeOutputToInput(Input::SubsurfaceRadiusScale, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::SubsurfaceRadiusScale.ToString(), DefaultValue::SubsurfaceRadiusScale);
		ConnectNodeOutputToInput(Input::SubsurfaceWeight, OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::SubsurfaceWeight.ToString(), DefaultValue::SubsurfaceWeight);
	}

	if(bIsSubstrateEnabled)
	{
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, SubstrateMaterial::Parameters::FrontMaterial.ToString(), OpenPBRSurfaceShaderNode->GetUniqueID(), OpenPBRSurface::SubstrateMaterial::Outputs::FrontMaterial.ToString());

		if(UInterchangeShaderPortsAPI::HasInput(OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::GeometryOpacity))
		{
			UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, SubstrateMaterial::Parameters::OpacityMask.ToString(), OpenPBRSurfaceShaderNode->GetUniqueID(), OpenPBRSurface::SubstrateMaterial::Outputs::OpacityMask.ToString());
			ShaderGraphNode->SetCustomBlendMode(EBlendMode::BLEND_Masked);
		}
	}
	else
	{	// Outputs
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::BaseColor.ToString(), OpenPBRSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::BaseColor.ToString());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Metallic.ToString(), OpenPBRSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Metallic.ToString());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Specular.ToString(), OpenPBRSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Specular.ToString());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Roughness.ToString(), OpenPBRSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Roughness.ToString());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::EmissiveColor.ToString(), OpenPBRSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::EmissiveColor.ToString());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Anisotropy.ToString(), OpenPBRSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Anisotropy.ToString());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Normal.ToString(), OpenPBRSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Normal.ToString());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Tangent.ToString(), OpenPBRSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Tangent.ToString());

		// We can't have all shading models at once, so we have to make a choice here
		if(UInterchangeShaderPortsAPI::HasInput(OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::TransmissionWeight))
		{
			UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Opacity.ToString(), OpenPBRSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Opacity.ToString());
			UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, ThinTranslucent::Parameters::TransmissionColor.ToString(), OpenPBRSurfaceShaderNode->GetUniqueID(), ThinTranslucent::Parameters::TransmissionColor.ToString());
			UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, Common::Parameters::Refraction.ToString(), OpenPBRSurfaceShaderNode->GetUniqueID(), Common::Parameters::Refraction.ToString());
		}
		else if(UInterchangeShaderPortsAPI::HasInput(OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::FuzzWeight))
		{
			UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, Sheen::Parameters::SheenColor.ToString(), OpenPBRSurfaceShaderNode->GetUniqueID(), Sheen::Parameters::SheenColor.ToString());
			UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, Sheen::Parameters::SheenRoughness.ToString(), OpenPBRSurfaceShaderNode->GetUniqueID(), Sheen::Parameters::SheenRoughness.ToString());
		}
		else if(UInterchangeShaderPortsAPI::HasInput(OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::CoatWeight))
		{
			UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, ClearCoat::Parameters::ClearCoat.ToString(), OpenPBRSurfaceShaderNode->GetUniqueID(), ClearCoat::Parameters::ClearCoat.ToString());
			UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, ClearCoat::Parameters::ClearCoatRoughness.ToString(), OpenPBRSurfaceShaderNode->GetUniqueID(), ClearCoat::Parameters::ClearCoatRoughness.ToString());
			UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, ClearCoat::Parameters::ClearCoatNormal.ToString(), OpenPBRSurfaceShaderNode->GetUniqueID(), ClearCoat::Parameters::ClearCoatNormal.ToString());
		}
		else if(UInterchangeShaderPortsAPI::HasInput(OpenPBRSurfaceShaderNode, OpenPBRSurface::Parameters::SubsurfaceWeight))
		{
			UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Opacity.ToString(), OpenPBRSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Opacity.ToString());
			UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, Subsurface::Parameters::SubsurfaceColor.ToString(), OpenPBRSurfaceShaderNode->GetUniqueID(), Subsurface::Parameters::SubsurfaceColor.ToString());
		}
	}

	// Outputs
}

#endif //WITH_EDITOR
