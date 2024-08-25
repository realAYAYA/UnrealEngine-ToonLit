// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "MaterialXStandardSurfaceShader.h"

#include "Engine/EngineTypes.h"

namespace mx = MaterialX;

FMaterialXStandardSurfaceShader::FMaterialXStandardSurfaceShader(UInterchangeBaseNodeContainer& BaseNodeContainer)
	: FMaterialXSurfaceShaderAbstract(BaseNodeContainer)
{
	NodeDefinition = mx::NodeDefinition::StandardSurface;
}

TSharedRef<FMaterialXBase> FMaterialXStandardSurfaceShader::MakeInstance(UInterchangeBaseNodeContainer& BaseNodeContainer)
{
	TSharedRef<FMaterialXStandardSurfaceShader> Result= MakeShared<FMaterialXStandardSurfaceShader>(BaseNodeContainer);
	Result->RegisterConnectNodeOutputToInputDelegates();
	return Result;
}

void FMaterialXStandardSurfaceShader::Translate(mx::NodePtr StandardSurfaceNode)
{
	this->SurfaceShaderNode = StandardSurfaceNode;
	
	using namespace UE::Interchange::Materials;

	UInterchangeFunctionCallShaderNode* StandardSurfaceShaderNode = CreateFunctionCallShaderNode(SurfaceShaderNode->getName().c_str(), UE::Interchange::MaterialX::IndexSurfaceShaders, uint8(EInterchangeMaterialXShaders::StandardSurface));

	if(bIsSubstrateEnabled)
	{
		ConnectToSubstrateStandardSurface(StandardSurfaceShaderNode);
	}
	else
	{
		ConnectToStandardSurface(StandardSurfaceShaderNode);
	}

	if(UInterchangeShaderPortsAPI::HasInput(StandardSurfaceShaderNode, StandardSurface::Parameters::Transmission))
	{
		StandardSurfaceShaderNode->AddInt32Attribute(UE::Interchange::MaterialX::Attributes::EnumType, UE::Interchange::MaterialX::IndexSurfaceShaders);
		StandardSurfaceShaderNode->AddInt32Attribute(UE::Interchange::MaterialX::Attributes::EnumValue, int32(EInterchangeMaterialXShaders::StandardSurfaceTransmission));
	}
}

void FMaterialXStandardSurfaceShader::ConnectToStandardSurface(UInterchangeFunctionCallShaderNode* StandardSurfaceShaderNode)
{
	using namespace UE::Interchange::Materials;
	using namespace mx::StandardSurface;
	constexpr bool bInputInTangentSpace = true;

	// Inputs
	//Base
	ConnectNodeOutputToInput(Input::Base, StandardSurfaceShaderNode, StandardSurface::Parameters::Base.ToString(), DefaultValue::Float::Base);

	//Base Color
	ConnectNodeOutputToInput(Input::BaseColor, StandardSurfaceShaderNode, StandardSurface::Parameters::BaseColor.ToString(), DefaultValue::Color3::BaseColor);

	//Diffuse Roughness
	ConnectNodeOutputToInput(Input::DiffuseRoughness, StandardSurfaceShaderNode, StandardSurface::Parameters::DiffuseRoughness.ToString(), DefaultValue::Float::DiffuseRoughness);

	//Specular
	ConnectNodeOutputToInput(Input::Specular, StandardSurfaceShaderNode, StandardSurface::Parameters::Specular.ToString(), DefaultValue::Float::Specular);

	//Specular Roughness
	ConnectNodeOutputToInput(Input::SpecularRoughness, StandardSurfaceShaderNode, StandardSurface::Parameters::SpecularRoughness.ToString(), DefaultValue::Float::SpecularRoughness);

	//Specular IOR
	ConnectNodeOutputToInput(Input::SpecularIOR, StandardSurfaceShaderNode, StandardSurface::Parameters::SpecularIOR.ToString(), DefaultValue::Float::SpecularIOR);

	//Specular Anisotropy
	ConnectNodeOutputToInput(Input::SpecularAnisotropy, StandardSurfaceShaderNode, StandardSurface::Parameters::SpecularAnisotropy.ToString(), DefaultValue::Float::SpecularAnisotropy);

	//Specular Rotation
	ConnectNodeOutputToInput(Input::SpecularRotation, StandardSurfaceShaderNode, StandardSurface::Parameters::SpecularRotation.ToString(), DefaultValue::Float::SpecularRotation);

	//Metallic
	ConnectNodeOutputToInput(Input::Metalness, StandardSurfaceShaderNode, StandardSurface::Parameters::Metalness.ToString(), DefaultValue::Float::Metalness);

	//Subsurface
	ConnectNodeOutputToInput(Input::Subsurface, StandardSurfaceShaderNode, StandardSurface::Parameters::Subsurface.ToString(), DefaultValue::Float::Subsurface);

	//Subsurface Color
	ConnectNodeOutputToInput(Input::SubsurfaceColor, StandardSurfaceShaderNode, StandardSurface::Parameters::SubsurfaceColor.ToString(), DefaultValue::Color3::SubsurfaceColor);

	//Subsurface Radius
	ConnectNodeOutputToInput(Input::SubsurfaceRadius, StandardSurfaceShaderNode, StandardSurface::Parameters::SubsurfaceRadius.ToString(), DefaultValue::Color3::SubsurfaceRadius);

	//Subsurface Scale
	ConnectNodeOutputToInput(Input::SubsurfaceScale, StandardSurfaceShaderNode, StandardSurface::Parameters::SubsurfaceScale.ToString(), DefaultValue::Float::SubsurfaceScale);

	//Sheen
	ConnectNodeOutputToInput(Input::Sheen, StandardSurfaceShaderNode, StandardSurface::Parameters::Sheen.ToString(), DefaultValue::Float::Sheen);

	//Sheen Color
	ConnectNodeOutputToInput(Input::SheenColor, StandardSurfaceShaderNode, StandardSurface::Parameters::SheenColor.ToString(), DefaultValue::Color3::SheenColor);

	//Sheen Roughness
	ConnectNodeOutputToInput(Input::SheenRoughness, StandardSurfaceShaderNode, StandardSurface::Parameters::SheenRoughness.ToString(), DefaultValue::Float::SheenRoughness);

	//Coat
	ConnectNodeOutputToInput(Input::Coat, StandardSurfaceShaderNode, StandardSurface::Parameters::Coat.ToString(), DefaultValue::Float::Coat);

	//Coat Color
	ConnectNodeOutputToInput(Input::CoatColor, StandardSurfaceShaderNode, StandardSurface::Parameters::CoatColor.ToString(), DefaultValue::Color3::CoatColor);

	//Coat Roughness
	ConnectNodeOutputToInput(Input::CoatRoughness, StandardSurfaceShaderNode, StandardSurface::Parameters::CoatRoughness.ToString(), DefaultValue::Float::CoatRoughness);

	//Coat Normal: No need to take the default input if there is no CoatNormal input
	ConnectNodeOutputToInput(Input::CoatNormal, StandardSurfaceShaderNode, StandardSurface::Parameters::CoatNormal.ToString(), nullptr, bInputInTangentSpace);

	//Thin Film Thickness
	ConnectNodeOutputToInput(Input::ThinFilmThickness, StandardSurfaceShaderNode, StandardSurface::Parameters::ThinFilmThickness.ToString(), DefaultValue::Float::ThinFilmThickness);

	//Emission
	ConnectNodeOutputToInput(Input::Emission, StandardSurfaceShaderNode, StandardSurface::Parameters::Emission.ToString(), DefaultValue::Float::Emission);

	//Emission Color
	ConnectNodeOutputToInput(Input::EmissionColor, StandardSurfaceShaderNode, StandardSurface::Parameters::EmissionColor.ToString(), DefaultValue::Color3::EmissionColor);

	//Normal: No need to take the default input if there is no Normal input
	ConnectNodeOutputToInput(Input::Normal, StandardSurfaceShaderNode, StandardSurface::Parameters::Normal.ToString(), nullptr, bInputInTangentSpace);

	//Tangent: No need to take the default input if there is no Tangent input
	ConnectNodeOutputToInput(Input::Tangent, StandardSurfaceShaderNode, StandardSurface::Parameters::Tangent.ToString(), nullptr, bInputInTangentSpace);

	//Transmission
	ConnectNodeOutputToInput(Input::Transmission, StandardSurfaceShaderNode, StandardSurface::Parameters::Transmission.ToString(), DefaultValue::Float::Transmission);

	//Transmission Color
	ConnectNodeOutputToInput(Input::TransmissionColor, StandardSurfaceShaderNode, StandardSurface::Parameters::TransmissionColor.ToString(), DefaultValue::Color3::TransmissionColor);

	//Transmission Depth
	ConnectNodeOutputToInput(Input::TransmissionDepth, StandardSurfaceShaderNode, StandardSurface::Parameters::TransmissionDepth.ToString(), DefaultValue::Float::TransmissionDepth);

	//Transmission Scatter
	ConnectNodeOutputToInput(Input::TransmissionScatter, StandardSurfaceShaderNode, StandardSurface::Parameters::TransmissionScatter.ToString(), DefaultValue::Color3::TransmissionScatter);

	//Transmission Scatter Anisotropy
	ConnectNodeOutputToInput(Input::TransmissionScatterAnisotropy, StandardSurfaceShaderNode, StandardSurface::Parameters::TransmissionScatterAnisotropy.ToString(), DefaultValue::Float::TransmissionScatterAnisotropy);

	//Transmission Dispersion
	ConnectNodeOutputToInput(Input::TransmissionDispersion, StandardSurfaceShaderNode, StandardSurface::Parameters::TransmissionDispersion.ToString(), DefaultValue::Float::TransmissionDispersion);

	//Transmission Extra Roughness
	ConnectNodeOutputToInput(Input::TransmissionExtraRoughness, StandardSurfaceShaderNode, StandardSurface::Parameters::TransmissionExtraRoughness.ToString(), DefaultValue::Float::TransmissionExtraRoughness);

	// Outputs
	UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::BaseColor.ToString(), StandardSurfaceShaderNode->GetUniqueID(), TEXT("Base Color"));
	UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Metallic.ToString(), StandardSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Metallic.ToString());
	UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Specular.ToString(), StandardSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Specular.ToString());
	UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Roughness.ToString(), StandardSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Roughness.ToString());
	UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::EmissiveColor.ToString(), StandardSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::EmissiveColor.ToString());
	UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Anisotropy.ToString(), StandardSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Anisotropy.ToString());
	UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Normal.ToString(), StandardSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Normal.ToString());
	UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Tangent.ToString(), StandardSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Tangent.ToString());

	// We can't have all shading models at once, so we have to make a choice here
	if(UInterchangeShaderPortsAPI::HasInput(StandardSurfaceShaderNode, StandardSurface::Parameters::Transmission))
	{
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Opacity.ToString(), StandardSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Opacity.ToString());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, ThinTranslucent::Parameters::TransmissionColor.ToString(), StandardSurfaceShaderNode->GetUniqueID(), ThinTranslucent::Parameters::TransmissionColor.ToString());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, Common::Parameters::Refraction.ToString(), StandardSurfaceShaderNode->GetUniqueID(), Common::Parameters::Refraction.ToString());
	}
	else if(UInterchangeShaderPortsAPI::HasInput(StandardSurfaceShaderNode, StandardSurface::Parameters::Sheen))
	{
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, Sheen::Parameters::SheenColor.ToString(), StandardSurfaceShaderNode->GetUniqueID(), Sheen::Parameters::SheenColor.ToString());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, Sheen::Parameters::SheenRoughness.ToString(), StandardSurfaceShaderNode->GetUniqueID(), Sheen::Parameters::SheenRoughness.ToString());
	}
	else if(UInterchangeShaderPortsAPI::HasInput(StandardSurfaceShaderNode, StandardSurface::Parameters::Coat))
	{
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, ClearCoat::Parameters::ClearCoat.ToString(), StandardSurfaceShaderNode->GetUniqueID(), ClearCoat::Parameters::ClearCoat.ToString());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, ClearCoat::Parameters::ClearCoatRoughness.ToString(), StandardSurfaceShaderNode->GetUniqueID(), ClearCoat::Parameters::ClearCoatRoughness.ToString());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, ClearCoat::Parameters::ClearCoatNormal.ToString(), StandardSurfaceShaderNode->GetUniqueID(), ClearCoat::Parameters::ClearCoatNormal.ToString());
	}
	else if(UInterchangeShaderPortsAPI::HasInput(StandardSurfaceShaderNode, StandardSurface::Parameters::Subsurface))
	{
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Opacity.ToString(), StandardSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Opacity.ToString());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, Subsurface::Parameters::SubsurfaceColor.ToString(), StandardSurfaceShaderNode->GetUniqueID(), Subsurface::Parameters::SubsurfaceColor.ToString());
	}
}

void FMaterialXStandardSurfaceShader::ConnectToSubstrateStandardSurface(UInterchangeFunctionCallShaderNode* StandardSurfaceShaderNode)
{
	using namespace UE::Interchange::Materials;
	using namespace mx::StandardSurface;
	constexpr bool bInputInTangentSpace = true;

	// Inputs
	//Base
	ConnectNodeOutputToInput(Input::Base, StandardSurfaceShaderNode, StandardSurface::Parameters::Base.ToString(), DefaultValue::Float::Base);

	//Base Color
	ConnectNodeOutputToInput(Input::BaseColor, StandardSurfaceShaderNode, StandardSurface::Parameters::BaseColor.ToString(), DefaultValue::Color3::BaseColor);

	//Diffuse Roughness
	ConnectNodeOutputToInput(Input::DiffuseRoughness, StandardSurfaceShaderNode, StandardSurface::Parameters::DiffuseRoughness.ToString(), DefaultValue::Float::DiffuseRoughness);

	//Metallic
	ConnectNodeOutputToInput(Input::Metalness, StandardSurfaceShaderNode, StandardSurface::Parameters::Metalness.ToString(), DefaultValue::Float::Metalness);

	//Specular
	ConnectNodeOutputToInput(Input::Specular, StandardSurfaceShaderNode, StandardSurface::Parameters::Specular.ToString(), DefaultValue::Float::Specular);

	//Specular Color
	ConnectNodeOutputToInput(Input::SpecularColor, StandardSurfaceShaderNode, StandardSurface::Parameters::SpecularColor.ToString(), DefaultValue::Color3::SpecularColor);

	//Specular Roughness
	ConnectNodeOutputToInput(Input::SpecularRoughness, StandardSurfaceShaderNode, StandardSurface::Parameters::SpecularRoughness.ToString(), DefaultValue::Float::SpecularRoughness);

	//Specular IOR
	ConnectNodeOutputToInput(Input::SpecularIOR, StandardSurfaceShaderNode, StandardSurface::Parameters::SpecularIOR.ToString(), DefaultValue::Float::SpecularIOR);

	//Specular Anisotropy
	ConnectNodeOutputToInput(Input::SpecularAnisotropy, StandardSurfaceShaderNode, StandardSurface::Parameters::SpecularAnisotropy.ToString(), DefaultValue::Float::SpecularAnisotropy);

	//Specular Rotation
	ConnectNodeOutputToInput(Input::SpecularRotation, StandardSurfaceShaderNode, StandardSurface::Parameters::SpecularRotation.ToString(), DefaultValue::Float::SpecularRotation);

	//Sheen
	ConnectNodeOutputToInput(Input::Sheen, StandardSurfaceShaderNode, StandardSurface::Parameters::Sheen.ToString(), DefaultValue::Float::Sheen);

	//Sheen Color
	ConnectNodeOutputToInput(Input::SheenColor, StandardSurfaceShaderNode, StandardSurface::Parameters::SheenColor.ToString(), DefaultValue::Color3::SheenColor);

	//Sheen Roughness
	ConnectNodeOutputToInput(Input::SheenRoughness, StandardSurfaceShaderNode, StandardSurface::Parameters::SheenRoughness.ToString(), DefaultValue::Float::SheenRoughness);

	//Coat
	ConnectNodeOutputToInput(Input::Coat, StandardSurfaceShaderNode, StandardSurface::Parameters::Coat.ToString(), DefaultValue::Float::Coat);

	//Coat Color
	ConnectNodeOutputToInput(Input::CoatColor, StandardSurfaceShaderNode, StandardSurface::Parameters::CoatColor.ToString(), DefaultValue::Color3::CoatColor);

	//Coat Roughness
	ConnectNodeOutputToInput(Input::CoatRoughness, StandardSurfaceShaderNode, StandardSurface::Parameters::CoatRoughness.ToString(), DefaultValue::Float::CoatRoughness);

	//Coat IOR
	ConnectNodeOutputToInput(Input::CoatIOR, StandardSurfaceShaderNode, StandardSurface::Parameters::CoatIOR.ToString(), DefaultValue::Float::CoatIOR);

	//Coat Anisotropy
	ConnectNodeOutputToInput(Input::CoatAnisotropy, StandardSurfaceShaderNode, StandardSurface::Parameters::CoatAnisotropy.ToString(), DefaultValue::Float::CoatAnisotropy);

	//Coat Rotation
	ConnectNodeOutputToInput(Input::CoatRotation, StandardSurfaceShaderNode, StandardSurface::Parameters::CoatRotation.ToString(), DefaultValue::Float::CoatAnisotropy);

	//Coat Normal: No need to take the default input if there is no CoatNormal input
	ConnectNodeOutputToInput(Input::CoatNormal, StandardSurfaceShaderNode, StandardSurface::Parameters::CoatNormal.ToString(), nullptr, bInputInTangentSpace);

	//Thin Film Thickness
	ConnectNodeOutputToInput(Input::ThinFilmThickness, StandardSurfaceShaderNode, StandardSurface::Parameters::ThinFilmThickness.ToString(), DefaultValue::Float::ThinFilmThickness);

	//Thin Film IOR
	ConnectNodeOutputToInput(Input::ThinFilmIOR, StandardSurfaceShaderNode, StandardSurface::Parameters::ThinFilmIOR.ToString(), DefaultValue::Float::ThinFilmIOR);

	//Emission
	ConnectNodeOutputToInput(Input::Emission, StandardSurfaceShaderNode, StandardSurface::Parameters::Emission.ToString(), DefaultValue::Float::Emission);

	//Emission Color
	ConnectNodeOutputToInput(Input::EmissionColor, StandardSurfaceShaderNode, StandardSurface::Parameters::EmissionColor.ToString(), DefaultValue::Color3::EmissionColor);

	//Normal: No need to take the default input if there is no Normal input
	ConnectNodeOutputToInput(Input::Normal, StandardSurfaceShaderNode, StandardSurface::Parameters::Normal.ToString(), nullptr, bInputInTangentSpace);

	//Tangent: No need to take the default input if there is no Tangent input
	ConnectNodeOutputToInput(Input::Tangent, StandardSurfaceShaderNode, StandardSurface::Parameters::Tangent.ToString(), nullptr, bInputInTangentSpace);

	//Opacity
	ConnectNodeOutputToInput(Input::Opacity, StandardSurfaceShaderNode, StandardSurface::Parameters::Opacity.ToString(), DefaultValue::Color3::Opacity);

	//Transmission
	ConnectNodeOutputToInput(Input::Transmission, StandardSurfaceShaderNode, StandardSurface::Parameters::Transmission.ToString(), DefaultValue::Float::Transmission);

	//Transmission Color
	ConnectNodeOutputToInput(Input::TransmissionColor, StandardSurfaceShaderNode, StandardSurface::Parameters::TransmissionColor.ToString(), DefaultValue::Color3::TransmissionColor);

	//Transmission Depth
	ConnectNodeOutputToInput(Input::TransmissionDepth, StandardSurfaceShaderNode, StandardSurface::Parameters::TransmissionDepth.ToString(), DefaultValue::Float::TransmissionDepth);

	//Transmission Scatter
	ConnectNodeOutputToInput(Input::TransmissionScatter, StandardSurfaceShaderNode, StandardSurface::Parameters::TransmissionScatter.ToString(), DefaultValue::Color3::TransmissionScatter);

	//Transmission Extra Roughness
	ConnectNodeOutputToInput(Input::TransmissionExtraRoughness, StandardSurfaceShaderNode, StandardSurface::Parameters::TransmissionExtraRoughness.ToString(), DefaultValue::Float::TransmissionExtraRoughness);

	// Outputs
	if(UInterchangeShaderPortsAPI::HasInput(StandardSurfaceShaderNode, StandardSurface::Parameters::Transmission))
	{
		ShaderGraphNode->SetCustomBlendMode(EBlendMode::BLEND_TranslucentColoredTransmittance);
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, PBRMR::Parameters::Opacity.ToString(), StandardSurfaceShaderNode->GetUniqueID(), PBRMR::Parameters::Opacity.ToString());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, SubstrateMaterial::Parameters::FrontMaterial.ToString(), StandardSurfaceShaderNode->GetUniqueID(), StandardSurface::SubstrateMaterial::Outputs::Translucent.ToString());
	}
	else
	{
		//Subsurface
		ConnectNodeOutputToInput(Input::Subsurface, StandardSurfaceShaderNode, StandardSurface::Parameters::Subsurface.ToString(), DefaultValue::Float::Subsurface);

		//Subsurface Color
		ConnectNodeOutputToInput(Input::SubsurfaceColor, StandardSurfaceShaderNode, StandardSurface::Parameters::SubsurfaceColor.ToString(), DefaultValue::Color3::SubsurfaceColor);

		//Subsurface Radius
		ConnectNodeOutputToInput(Input::SubsurfaceRadius, StandardSurfaceShaderNode, StandardSurface::Parameters::SubsurfaceRadius.ToString(), DefaultValue::Color3::SubsurfaceRadius);

		//Subsurface Scale
		ConnectNodeOutputToInput(Input::SubsurfaceScale, StandardSurfaceShaderNode, StandardSurface::Parameters::SubsurfaceScale.ToString(), DefaultValue::Float::SubsurfaceScale);

		//Subsurface Anisotropy
		ConnectNodeOutputToInput(Input::SubsurfaceAnisotropy, StandardSurfaceShaderNode, StandardSurface::Parameters::SubsurfaceAnisotropy.ToString(), DefaultValue::Float::SubsurfaceAnisotropy);

		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, SubstrateMaterial::Parameters::FrontMaterial.ToString(), StandardSurfaceShaderNode->GetUniqueID(), StandardSurface::SubstrateMaterial::Outputs::Opaque.ToString());
		if(UInterchangeShaderPortsAPI::HasInput(StandardSurfaceShaderNode, StandardSurface::Parameters::Opacity))
		{
			UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, SubstrateMaterial::Parameters::OpacityMask.ToString(), StandardSurfaceShaderNode->GetUniqueID(), StandardSurface::SubstrateMaterial::Outputs::Opacity.ToString());
			ShaderGraphNode->SetCustomBlendMode(EBlendMode::BLEND_Masked);
		}
	}
}
#endif
