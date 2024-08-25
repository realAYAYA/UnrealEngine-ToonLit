// Copyright Epic Games, Inc. All Rights Reserved. 

#include "InterchangeMaterialXPipeline.h"

#include "InterchangeImportModule.h"
#include "InterchangeMaterialDefinitions.h"
#include "InterchangeMaterialFactoryNode.h"
#include "InterchangePipelineLog.h"

#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunction.h"
#include "Misc/PackageName.h"

#define MATERIALX_FUNCTIONS_SUBSTRATE_PATH(Name)  \
	constexpr const TCHAR* Name##FunctionsPath = TEXT("/Interchange/Functions/") TEXT("MX_") TEXT(#Name) TEXT(".") TEXT("MX_") TEXT(#Name);  \
	constexpr const TCHAR* Name##SubstratePath = TEXT("/Interchange/Substrate/") TEXT("MX_") TEXT(#Name) TEXT(".") TEXT("MX_") TEXT(#Name);

#define MATERIALX_MATERIALFUNCTION_PATH(Name) \
	!MaterialXSettings->bIsSubstrateEnabled ? Name##FunctionsPath : Name##SubstratePath

namespace
{
	constexpr const TCHAR* OpenPBRSurfaceFunctionsPath = TEXT("/Interchange/Functions/MX_OpenPBR_Opaque.MX_OpenPBR_Opaque");
	constexpr const TCHAR* OpenPBRSurfaceSubstratePath = TEXT("/Engine/Functions/Substrate/MF_Substrate_OpenPBR_Opaque.MF_Substrate_OpenPBR_Opaque");
	constexpr const TCHAR* OpenPBRTransmissionSurfaceFunctionsPath = TEXT("/Interchange/Functions/MX_OpenPBR_Translucent.MX_OpenPBR_Translucent");
	constexpr const TCHAR* OpenPBRTransmissionSurfaceSubstratePath = TEXT("/Engine/Functions/Substrate/MF_Substrate_OpenPBR_Translucent.MF_Substrate_OpenPBR_Translucent");

	constexpr const TCHAR* StandardSurfaceFunctionsPath = TEXT("/Interchange/Functions/MX_StandardSurface.MX_StandardSurface");
	constexpr const TCHAR* StandardSurfaceSubstratePath = TEXT("/Engine/Functions/Substrate/Substrate-StandardSurface-Opaque.Substrate-StandardSurface-Opaque");
	constexpr const TCHAR* TransmissionSurfaceFunctionsPath = TEXT("/Interchange/Functions/MX_TransmissionSurface.MX_TransmissionSurface");
	constexpr const TCHAR* TransmissionSurfaceSubstratePath = TEXT("/Engine/Functions/Substrate/Substrate-StandardSurface-Translucent.Substrate-StandardSurface-Translucent");
	MATERIALX_FUNCTIONS_SUBSTRATE_PATH(SurfaceUnlit);
	MATERIALX_FUNCTIONS_SUBSTRATE_PATH(Surface);
	MATERIALX_FUNCTIONS_SUBSTRATE_PATH(UsdPreviewSurface);
	
	MATERIALX_FUNCTIONS_SUBSTRATE_PATH(OrenNayarBSDF);
	MATERIALX_FUNCTIONS_SUBSTRATE_PATH(BurleyDiffuseBSDF);
	MATERIALX_FUNCTIONS_SUBSTRATE_PATH(DielectricBSDF);
	MATERIALX_FUNCTIONS_SUBSTRATE_PATH(ConductorBSDF);
	MATERIALX_FUNCTIONS_SUBSTRATE_PATH(SheenBSDF);
	MATERIALX_FUNCTIONS_SUBSTRATE_PATH(SubsurfaceBSDF);
	MATERIALX_FUNCTIONS_SUBSTRATE_PATH(ThinFilmBSDF);
	MATERIALX_FUNCTIONS_SUBSTRATE_PATH(GeneralizedSchlickBSDF);
	MATERIALX_FUNCTIONS_SUBSTRATE_PATH(TranslucentBSDF);
	
	MATERIALX_FUNCTIONS_SUBSTRATE_PATH(UniformEDF);
	MATERIALX_FUNCTIONS_SUBSTRATE_PATH(ConicalEDF);
	MATERIALX_FUNCTIONS_SUBSTRATE_PATH(MeasuredEDF);
	
	MATERIALX_FUNCTIONS_SUBSTRATE_PATH(AbsorptionVDF);
	MATERIALX_FUNCTIONS_SUBSTRATE_PATH(AnisotropicVDF);
}

TMap<FString, EInterchangeMaterialXSettings> UInterchangeMaterialXPipeline::PathToEnumMapping;
#if WITH_EDITOR
TMap<EInterchangeMaterialXSettings, TPair<TSet<FName>, TSet<FName>>> UMaterialXPipelineSettings::SettingsInputsOutputs;
#endif // WITH_EDITOR

UMaterialXPipelineSettings::UMaterialXPipelineSettings()
{
#if WITH_EDITOR
	if(HasAnyFlags(EObjectFlags::RF_ClassDefaultObject))
	{
		bIsSubstrateEnabled = IInterchangeImportModule::IsAvailable() ? IInterchangeImportModule::Get().IsSubstrateEnabled() : false;

		SettingsInputsOutputs = {
			//Surface Shaders
						{
				UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXShaders::OpenPBRSurface),
				{
				// OpenPBRSurface Inputs
				TSet<FName>{
					UE::Interchange::Materials::OpenPBRSurface::Parameters::BaseWeight,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::BaseColor,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::BaseRoughness,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::BaseMetalness,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::SpecularWeight,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::SpecularColor,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::SpecularRoughness,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::SpecularIOR,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::SpecularIORLevel,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::SpecularAnisotropy,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::SpecularRotation,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::SubsurfaceWeight,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::SubsurfaceColor,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::SubsurfaceRadius,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::SubsurfaceRadiusScale,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::SubsurfaceAnisotropy,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::FuzzWeight,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::FuzzColor,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::FuzzRoughness,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::CoatWeight,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::CoatColor,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::CoatRoughness,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::CoatAnisotropy,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::CoatRotation,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::CoatIOR,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::CoatIORLevel,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::GeometryCoatNormal,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::ThinFilmThickness,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::ThinFilmIOR,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::EmissionLuminance,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::EmissionColor,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::GeometryNormal,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::GeometryTangent,
					UE::Interchange::Materials::OpenPBRSurface::Parameters::GeometryOpacity,
				},
				// OpenPBRSurface Outputs
				!bIsSubstrateEnabled ?
				TSet<FName>{
					UE::Interchange::Materials::PBRMR::Parameters::BaseColor,
						UE::Interchange::Materials::PBRMR::Parameters::Metallic,
						UE::Interchange::Materials::PBRMR::Parameters::Specular,
						UE::Interchange::Materials::PBRMR::Parameters::Roughness,
						UE::Interchange::Materials::PBRMR::Parameters::Anisotropy,
						UE::Interchange::Materials::PBRMR::Parameters::EmissiveColor,
						UE::Interchange::Materials::PBRMR::Parameters::Opacity,
						UE::Interchange::Materials::PBRMR::Parameters::Normal,
						UE::Interchange::Materials::PBRMR::Parameters::Tangent,
						UE::Interchange::Materials::Sheen::Parameters::SheenRoughness,
						UE::Interchange::Materials::Sheen::Parameters::SheenColor,
						UE::Interchange::Materials::Subsurface::Parameters::SubsurfaceColor,
						UE::Interchange::Materials::ClearCoat::Parameters::ClearCoat,
						UE::Interchange::Materials::ClearCoat::Parameters::ClearCoatRoughness,
						UE::Interchange::Materials::ClearCoat::Parameters::ClearCoatNormal
					}	:
					TSet<FName>{
						UE::Interchange::Materials::OpenPBRSurface::SubstrateMaterial::Outputs::FrontMaterial,
						UE::Interchange::Materials::OpenPBRSurface::SubstrateMaterial::Outputs::OpacityMask
					}
				}
			},
			{
				UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXShaders::OpenPBRSurfaceTransmission),
				{
					// OpenPBRSurfaceTransmission Inputs
					TSet<FName>{
						UE::Interchange::Materials::OpenPBRSurface::Parameters::BaseWeight,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::BaseColor,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::BaseRoughness,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::BaseMetalness,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::SpecularWeight,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::SpecularColor,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::SpecularRoughness,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::SpecularIOR,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::SpecularIORLevel,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::SpecularAnisotropy,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::SpecularRotation,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::TransmissionWeight,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::TransmissionColor,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::TransmissionDepth,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::TransmissionDispersionScale,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::TransmissionDispersionAbbeNumber,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::TransmissionScatter,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::TransmissionScatterAnisotropy,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::FuzzWeight,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::FuzzColor,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::FuzzRoughness,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::CoatWeight,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::CoatColor,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::CoatRoughness,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::CoatAnisotropy,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::CoatRotation,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::CoatIOR,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::CoatIORLevel,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::GeometryCoatNormal,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::ThinFilmThickness,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::ThinFilmIOR,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::EmissionLuminance,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::EmissionColor,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::GeometryNormal,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::GeometryTangent,
						UE::Interchange::Materials::OpenPBRSurface::Parameters::GeometryOpacity,
					},
					// OpenPBRSurfaceTransmission Outputs
					!bIsSubstrateEnabled ?
					TSet<FName>{
						UE::Interchange::Materials::PBRMR::Parameters::BaseColor,
						UE::Interchange::Materials::PBRMR::Parameters::Metallic,
						UE::Interchange::Materials::PBRMR::Parameters::Specular,
						UE::Interchange::Materials::PBRMR::Parameters::Roughness,
						UE::Interchange::Materials::PBRMR::Parameters::Anisotropy,
						UE::Interchange::Materials::PBRMR::Parameters::EmissiveColor,
						UE::Interchange::Materials::PBRMR::Parameters::Opacity,
						UE::Interchange::Materials::PBRMR::Parameters::Normal,
						UE::Interchange::Materials::PBRMR::Parameters::Tangent,
						UE::Interchange::Materials::PBRMR::Parameters::Refraction,
						UE::Interchange::Materials::ThinTranslucent::Parameters::TransmissionColor
					}	:
					TSet<FName>{
						UE::Interchange::Materials::OpenPBRSurface::SubstrateMaterial::Outputs::FrontMaterial,
						UE::Interchange::Materials::OpenPBRSurface::SubstrateMaterial::Outputs::OpacityMask
					}
				}
			},
			{
				UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXShaders::StandardSurface),
				{
					// StandardSurface Inputs
					TSet<FName>{
						UE::Interchange::Materials::StandardSurface::Parameters::Base,
						UE::Interchange::Materials::StandardSurface::Parameters::BaseColor,
						UE::Interchange::Materials::StandardSurface::Parameters::DiffuseRoughness,
						UE::Interchange::Materials::StandardSurface::Parameters::Metalness,
						UE::Interchange::Materials::StandardSurface::Parameters::Specular,
						UE::Interchange::Materials::StandardSurface::Parameters::SpecularRoughness,
						UE::Interchange::Materials::StandardSurface::Parameters::SpecularIOR,
						UE::Interchange::Materials::StandardSurface::Parameters::SpecularAnisotropy,
						UE::Interchange::Materials::StandardSurface::Parameters::SpecularRotation,
						UE::Interchange::Materials::StandardSurface::Parameters::Subsurface,
						UE::Interchange::Materials::StandardSurface::Parameters::SubsurfaceColor,
						UE::Interchange::Materials::StandardSurface::Parameters::SubsurfaceRadius,
						UE::Interchange::Materials::StandardSurface::Parameters::SubsurfaceScale,
						UE::Interchange::Materials::StandardSurface::Parameters::Sheen,
						UE::Interchange::Materials::StandardSurface::Parameters::SheenColor,
						UE::Interchange::Materials::StandardSurface::Parameters::SheenRoughness,
						UE::Interchange::Materials::StandardSurface::Parameters::Coat,
						UE::Interchange::Materials::StandardSurface::Parameters::CoatColor,
						UE::Interchange::Materials::StandardSurface::Parameters::CoatRoughness,
						UE::Interchange::Materials::StandardSurface::Parameters::CoatNormal,
						UE::Interchange::Materials::StandardSurface::Parameters::ThinFilmThickness,
						UE::Interchange::Materials::StandardSurface::Parameters::Emission,
						UE::Interchange::Materials::StandardSurface::Parameters::EmissionColor,
						UE::Interchange::Materials::StandardSurface::Parameters::Normal,
						UE::Interchange::Materials::StandardSurface::Parameters::Tangent,
						bIsSubstrateEnabled ? UE::Interchange::Materials::StandardSurface::Parameters::SpecularColor : FName{},
						bIsSubstrateEnabled ? UE::Interchange::Materials::StandardSurface::Parameters::CoatIOR : FName{},
						bIsSubstrateEnabled ? UE::Interchange::Materials::StandardSurface::Parameters::CoatAnisotropy : FName{},
						bIsSubstrateEnabled ? UE::Interchange::Materials::StandardSurface::Parameters::CoatRotation : FName{},
						bIsSubstrateEnabled ? UE::Interchange::Materials::StandardSurface::Parameters::ThinFilmIOR : FName{},
						bIsSubstrateEnabled ? UE::Interchange::Materials::StandardSurface::Parameters::Opacity : FName{},
						bIsSubstrateEnabled ? UE::Interchange::Materials::StandardSurface::Parameters::SubsurfaceAnisotropy : FName{}
					},
					// StandardSurface Outputs
					!bIsSubstrateEnabled ?
					TSet<FName>{
						TEXT("Base Color"), // MX_StandardSurface has BaseColor with a whitespace, this should be fixed in further release
						UE::Interchange::Materials::PBRMR::Parameters::Metallic,
						UE::Interchange::Materials::PBRMR::Parameters::Specular,
						UE::Interchange::Materials::PBRMR::Parameters::Roughness,
						UE::Interchange::Materials::PBRMR::Parameters::Anisotropy,
						UE::Interchange::Materials::PBRMR::Parameters::EmissiveColor,
						UE::Interchange::Materials::PBRMR::Parameters::Opacity,
						UE::Interchange::Materials::PBRMR::Parameters::Normal,
						UE::Interchange::Materials::PBRMR::Parameters::Tangent,
						UE::Interchange::Materials::Sheen::Parameters::SheenRoughness,
						UE::Interchange::Materials::Sheen::Parameters::SheenColor,
						UE::Interchange::Materials::Subsurface::Parameters::SubsurfaceColor,
						UE::Interchange::Materials::ClearCoat::Parameters::ClearCoat,
						UE::Interchange::Materials::ClearCoat::Parameters::ClearCoatRoughness,
						UE::Interchange::Materials::ClearCoat::Parameters::ClearCoatNormal
					}	:
					TSet<FName>{
						UE::Interchange::Materials::StandardSurface::SubstrateMaterial::Outputs::Opaque,
						UE::Interchange::Materials::StandardSurface::SubstrateMaterial::Outputs::Opacity
					}
				}
			},
			{
				UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXShaders::StandardSurfaceTransmission),
				{
					// StandardSurfaceTransmission Inputs
					TSet<FName>{
						UE::Interchange::Materials::StandardSurface::Parameters::Base,
						UE::Interchange::Materials::StandardSurface::Parameters::BaseColor,
						UE::Interchange::Materials::StandardSurface::Parameters::DiffuseRoughness,
						UE::Interchange::Materials::StandardSurface::Parameters::Metalness,
						UE::Interchange::Materials::StandardSurface::Parameters::Specular,
						bIsSubstrateEnabled ? UE::Interchange::Materials::StandardSurface::Parameters::SpecularColor : FName{},
						UE::Interchange::Materials::StandardSurface::Parameters::SpecularRoughness,
						UE::Interchange::Materials::StandardSurface::Parameters::SpecularIOR,
						UE::Interchange::Materials::StandardSurface::Parameters::SpecularAnisotropy,
						UE::Interchange::Materials::StandardSurface::Parameters::SpecularRotation,
						UE::Interchange::Materials::StandardSurface::Parameters::Transmission,
						UE::Interchange::Materials::StandardSurface::Parameters::TransmissionColor,
						UE::Interchange::Materials::StandardSurface::Parameters::TransmissionDepth,
						UE::Interchange::Materials::StandardSurface::Parameters::TransmissionScatter,
						UE::Interchange::Materials::StandardSurface::Parameters::TransmissionScatterAnisotropy,
						UE::Interchange::Materials::StandardSurface::Parameters::TransmissionDispersion,
						UE::Interchange::Materials::StandardSurface::Parameters::TransmissionExtraRoughness,
						!bIsSubstrateEnabled ? UE::Interchange::Materials::StandardSurface::Parameters::Subsurface : FName{},
						!bIsSubstrateEnabled ? UE::Interchange::Materials::StandardSurface::Parameters::SubsurfaceColor : FName{},
						!bIsSubstrateEnabled ? UE::Interchange::Materials::StandardSurface::Parameters::SubsurfaceRadius : FName{},
						!bIsSubstrateEnabled ? UE::Interchange::Materials::StandardSurface::Parameters::SubsurfaceScale : FName{},
						UE::Interchange::Materials::StandardSurface::Parameters::Sheen,
						UE::Interchange::Materials::StandardSurface::Parameters::SheenColor,
						UE::Interchange::Materials::StandardSurface::Parameters::SheenRoughness,
						UE::Interchange::Materials::StandardSurface::Parameters::Coat,
						UE::Interchange::Materials::StandardSurface::Parameters::CoatColor,
						UE::Interchange::Materials::StandardSurface::Parameters::CoatRoughness,
						bIsSubstrateEnabled ? UE::Interchange::Materials::StandardSurface::Parameters::CoatAnisotropy : FName{},
						bIsSubstrateEnabled ? UE::Interchange::Materials::StandardSurface::Parameters::CoatRotation : FName{},
						bIsSubstrateEnabled ? UE::Interchange::Materials::StandardSurface::Parameters::CoatIOR : FName{},
						UE::Interchange::Materials::StandardSurface::Parameters::CoatNormal,
						UE::Interchange::Materials::StandardSurface::Parameters::ThinFilmThickness,
						bIsSubstrateEnabled ? UE::Interchange::Materials::StandardSurface::Parameters::ThinFilmIOR : FName{},
						UE::Interchange::Materials::StandardSurface::Parameters::Emission,
						UE::Interchange::Materials::StandardSurface::Parameters::EmissionColor,
						bIsSubstrateEnabled ? UE::Interchange::Materials::StandardSurface::Parameters::Opacity : FName{},
						UE::Interchange::Materials::StandardSurface::Parameters::Normal,
						UE::Interchange::Materials::StandardSurface::Parameters::Tangent
					},
					// StandardSurfaceTransmission Outputs
					!bIsSubstrateEnabled ?
					TSet<FName>{
						UE::Interchange::Materials::PBRMR::Parameters::BaseColor,
						UE::Interchange::Materials::PBRMR::Parameters::Metallic,
						UE::Interchange::Materials::PBRMR::Parameters::Specular,
						UE::Interchange::Materials::PBRMR::Parameters::Roughness,
						UE::Interchange::Materials::PBRMR::Parameters::Anisotropy,
						UE::Interchange::Materials::PBRMR::Parameters::EmissiveColor,
						UE::Interchange::Materials::PBRMR::Parameters::Opacity,
						UE::Interchange::Materials::PBRMR::Parameters::Normal,
						UE::Interchange::Materials::PBRMR::Parameters::Tangent,
						UE::Interchange::Materials::PBRMR::Parameters::Refraction,
						UE::Interchange::Materials::ThinTranslucent::Parameters::TransmissionColor
					}	:
					TSet<FName>{UE::Interchange::Materials::StandardSurface::SubstrateMaterial::Outputs::Translucent}
				}
			},
			{
				UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXShaders::SurfaceUnlit),
				{
					// SurfaceUnlit Inputs
					TSet<FName>{
						UE::Interchange::Materials::SurfaceUnlit::Parameters::Emission,
						UE::Interchange::Materials::SurfaceUnlit::Parameters::EmissionColor,
						UE::Interchange::Materials::SurfaceUnlit::Parameters::Transmission,
						UE::Interchange::Materials::SurfaceUnlit::Parameters::TransmissionColor,
						UE::Interchange::Materials::SurfaceUnlit::Parameters::Opacity
					},
					// SurfaceUnlit Outputs
					!bIsSubstrateEnabled ?
					TSet<FName>{
						UE::Interchange::Materials::Common::Parameters::EmissiveColor,
						UE::Interchange::Materials::Common::Parameters::Opacity,
						UE::Interchange::Materials::SurfaceUnlit::Outputs::OpacityMask
						} :
					TSet<FName>{
						UE::Interchange::Materials::SurfaceUnlit::Substrate::Outputs::OpacityMask,
						UE::Interchange::Materials::SurfaceUnlit::Substrate::Outputs::SurfaceUnlit
					}
				}
			},
			{
				UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXShaders::UsdPreviewSurface),
				{
					// UsdPreviewSurface Inputs
					TSet<FName>{
						UE::Interchange::Materials::UsdPreviewSurface::Parameters::DiffuseColor,
						UE::Interchange::Materials::UsdPreviewSurface::Parameters::EmissiveColor,
						UE::Interchange::Materials::UsdPreviewSurface::Parameters::SpecularColor,
						UE::Interchange::Materials::UsdPreviewSurface::Parameters::Metallic,
						UE::Interchange::Materials::UsdPreviewSurface::Parameters::Roughness,
						UE::Interchange::Materials::UsdPreviewSurface::Parameters::Clearcoat,
						UE::Interchange::Materials::UsdPreviewSurface::Parameters::ClearcoatRoughness,
						UE::Interchange::Materials::UsdPreviewSurface::Parameters::Opacity,
						UE::Interchange::Materials::UsdPreviewSurface::Parameters::OpacityThreshold,
						UE::Interchange::Materials::UsdPreviewSurface::Parameters::IOR,
						UE::Interchange::Materials::UsdPreviewSurface::Parameters::Normal,
						UE::Interchange::Materials::UsdPreviewSurface::Parameters::Displacement,
						UE::Interchange::Materials::UsdPreviewSurface::Parameters::Occlusion
					},
					// UsdPreviewSurface Outputs
					!bIsSubstrateEnabled ?
					TSet<FName>{
						UE::Interchange::Materials::PBRMR::Parameters::BaseColor,
						UE::Interchange::Materials::PBRMR::Parameters::Metallic,
						UE::Interchange::Materials::PBRMR::Parameters::Specular,
						UE::Interchange::Materials::PBRMR::Parameters::Roughness,
						UE::Interchange::Materials::PBRMR::Parameters::EmissiveColor,
						UE::Interchange::Materials::PBRMR::Parameters::Opacity,
						UE::Interchange::Materials::PBRMR::Parameters::Normal,
						UE::Interchange::Materials::Common::Parameters::Refraction,
						UE::Interchange::Materials::Common::Parameters::Occlusion,
						UE::Interchange::Materials::ClearCoat::Parameters::ClearCoat,
						UE::Interchange::Materials::ClearCoat::Parameters::ClearCoatRoughness,
					} :
					TSet<FName>{TEXT("Substrate UsdPreviewSurface"), TEXT("Opacity")}
				}
			},
			{
				UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXShaders::Surface),
				{
					// Surface Inputs
					TSet<FName>{
						UE::Interchange::Materials::Surface::Parameters::BSDF,
						UE::Interchange::Materials::Surface::Parameters::EDF,
						UE::Interchange::Materials::Surface::Parameters::Opacity
					},
					// Surface Outputs
					bIsSubstrateEnabled ?
					TSet<FName>{UE::Interchange::Materials::Surface::Outputs::Surface} :
					TSet<FName>{UE::Interchange::Materials::Surface::Substrate::Outputs::Surface, UE::Interchange::Materials::Surface::Substrate::Outputs::Opacity}
				}
			},
			// BSDF
			{
				UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::OrenNayarDiffuse),
				{
					// OrenNayarDiffuse Inputs
					TSet<FName>{
						TEXT("weight"),
						TEXT("color"),
						TEXT("roughness"),
						TEXT("normal")
					},
					// OrenNayarDiffuse Outputs
					TSet<FName>{
						TEXT("Output")
					}
				}
			},
			{
				UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::BurleyDiffuse),
				{
					// BurleyDiffuse Inputs
					TSet<FName>{
						TEXT("weight"),
						TEXT("color"),
						TEXT("roughness"),
						TEXT("normal")
					},
					// BurleyDiffuse Outputs
					TSet<FName>{
						TEXT("Output")
					}
				}
			},
			{
				UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::Translucent),
				{
					// Translucent Inputs
					TSet<FName>{
						TEXT("weight"),
						TEXT("color"),
						TEXT("normal")
					},
					// Translucent Outputs
					TSet<FName>{
						TEXT("Output")
					}
				}
			},
			{
				UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::Dielectric),
				{
					// Dielectric Inputs
					TSet<FName>{
						TEXT("weight"),
						TEXT("tint"),
						TEXT("ior"),
						TEXT("roughness"),
						TEXT("normal"),
						TEXT("tangent")
					},
					// Dielectric Outputs
					TSet<FName>{
						TEXT("Output")
					}
				}
			},
			{
				UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::Conductor),
				{
					// Conductor Inputs
					TSet<FName>{
						TEXT("weight"),
						TEXT("ior"),
						TEXT("extinction"),
						TEXT("roughness"),
						TEXT("normal"),
						TEXT("tangent")
					},
					// Conductor Outputs
					TSet<FName>{
						TEXT("Output")
					}
				}
			},
			{
				UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::GeneralizedSchlick),
				{
					// GeneralizedSchlick Inputs
					TSet<FName>{
						TEXT("weight"),
						TEXT("color0"),
						TEXT("color90"),
						TEXT("exponent"),
						TEXT("roughness"),
						TEXT("normal"),
						TEXT("tangent")
					},
					// GeneralizedSchlick Outputs
					TSet<FName>{
						TEXT("Output")
					}
				}
			},
			{
				UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::Subsurface),
				{
					// Subsurface Inputs
					TSet<FName>{
						TEXT("weight"),
						TEXT("color"),
						TEXT("radius"),
						TEXT("anisotropy"),
						TEXT("normal")
					},
					// Subsurface Outputs
					TSet<FName>{
						TEXT("Output")
					}
				}
			},
			{
				UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::Sheen),
				{
					// Sheen Inputs
					TSet<FName>{
						TEXT("weight"),
						TEXT("color"),
						TEXT("roughness"),
						TEXT("normal")
					},
					// Sheen Outputs
					TSet<FName>{
						TEXT("Output")
					}
				}
			},
			{
				UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::ThinFilm),
				{
					// ThinFilm Inputs
					TSet<FName>{
						TEXT("thickness"),
						TEXT("ior")
					},
					// ThinFilm Outputs
					TSet<FName>{
						TEXT("Output")
					}
				}
			},
			// EDF
			{
				UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXEDF::Uniform),
				{
					// Uniform Inputs
					TSet<FName>{
						TEXT("color")
					},
					// Uniform Outputs
					TSet<FName>{
						TEXT("Output")
					}
				}
			},
			{
				UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXEDF::Conical),
				{
					// Conical Inputs
					TSet<FName>{
						TEXT("color"),
						TEXT("normal"),
						TEXT("inner_angle"),
						TEXT("outer_angle"),
					},
					// Conical Outputs
					TSet<FName>{
						TEXT("Output")
					}
				}
			},
			{
				UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXEDF::Measured),
				{
					// Measured Inputs
					TSet<FName>{
						TEXT("color"),
						TEXT("normal"),
						TEXT("file")
					},
					// Measured Outputs
					TSet<FName>{
						TEXT("Output")
					}
				}
			},
			// VDF
			{
				UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXVDF::Absorption),
				{
					// Absorption Inputs
					TSet<FName>{
						TEXT("absorption")
					},
					// Absorption Outputs
					TSet<FName>{
						TEXT("Output")
					}
				}
			},
			{
				UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXVDF::Anisotropic),
				{
					// Anisotropic Inputs
					TSet<FName>{
						TEXT("absorption"),
						TEXT("scattering"),
						TEXT("anisotropy"),
					},
					// Anisotropic Outputs
					TSet<FName>{
						TEXT("Output")
					}
				}
			}
		};
	}
#endif // WITH_EDITOR
}

UInterchangeMaterialXPipeline::UInterchangeMaterialXPipeline()
	: MaterialXSettings(UMaterialXPipelineSettings::StaticClass()->GetDefaultObject<UMaterialXPipelineSettings>())
{
	if(HasAnyFlags(EObjectFlags::RF_ClassDefaultObject))
	{
#if WITH_EDITOR
		PathToEnumMapping =	{
			{MATERIALX_MATERIALFUNCTION_PATH(OpenPBRSurface),		      UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXShaders::OpenPBRSurface)},
			{MATERIALX_MATERIALFUNCTION_PATH(OpenPBRTransmissionSurface), UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXShaders::OpenPBRSurfaceTransmission)},
			{MATERIALX_MATERIALFUNCTION_PATH(StandardSurface),			  UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXShaders::StandardSurface)},
			{MATERIALX_MATERIALFUNCTION_PATH(TransmissionSurface),		  UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXShaders::StandardSurfaceTransmission)},
			{MATERIALX_MATERIALFUNCTION_PATH(SurfaceUnlit),				  UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXShaders::SurfaceUnlit)},
			{MATERIALX_MATERIALFUNCTION_PATH(Surface),					  UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXShaders::Surface)},
			{MATERIALX_MATERIALFUNCTION_PATH(UsdPreviewSurface),		  UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXShaders::UsdPreviewSurface)},
																		  
			{MATERIALX_MATERIALFUNCTION_PATH(OrenNayarBSDF),			  UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::OrenNayarDiffuse)},
			{MATERIALX_MATERIALFUNCTION_PATH(BurleyDiffuseBSDF),		  UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::BurleyDiffuse)},
			{MATERIALX_MATERIALFUNCTION_PATH(DielectricBSDF),			  UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::Dielectric)},
			{MATERIALX_MATERIALFUNCTION_PATH(ConductorBSDF),			  UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::Conductor)},
			{MATERIALX_MATERIALFUNCTION_PATH(SheenBSDF),				  UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::Sheen)},
			{MATERIALX_MATERIALFUNCTION_PATH(SubsurfaceBSDF),			  UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::Subsurface)},
			{MATERIALX_MATERIALFUNCTION_PATH(ThinFilmBSDF),				  UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::ThinFilm)},
			{MATERIALX_MATERIALFUNCTION_PATH(GeneralizedSchlickBSDF),	  UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::GeneralizedSchlick)},
			{MATERIALX_MATERIALFUNCTION_PATH(TranslucentBSDF),			  UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::Translucent)},
																		  
			{MATERIALX_MATERIALFUNCTION_PATH(UniformEDF),				  UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXEDF::Uniform)},
			{MATERIALX_MATERIALFUNCTION_PATH(ConicalEDF),				  UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXEDF::Conical)},
			{MATERIALX_MATERIALFUNCTION_PATH(MeasuredEDF),				  UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXEDF::Measured)},
																		  
			{MATERIALX_MATERIALFUNCTION_PATH(AbsorptionVDF),			  UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXVDF::Absorption)},
			{MATERIALX_MATERIALFUNCTION_PATH(AnisotropicVDF),			  UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXVDF::Anisotropic)},
		};

		MaterialXSettings->InitPredefinedAssets();
#endif // WITH_EDITOR
	}


	for (const TPair<EInterchangeMaterialXShaders, FSoftObjectPath>& Entry : MaterialXSettings->PredefinedSurfaceShaders)
	{
		PathToEnumMapping.FindOrAdd(Entry.Value.GetAssetPathString(), EInterchangeMaterialXSettings{ TInPlaceType<EInterchangeMaterialXShaders>{}, Entry.Key });
	}

	for(const TPair<EInterchangeMaterialXBSDF, FSoftObjectPath>& Entry : MaterialXSettings->PredefinedBSDF)
	{
		PathToEnumMapping.FindOrAdd(Entry.Value.GetAssetPathString(), EInterchangeMaterialXSettings{ TInPlaceType<EInterchangeMaterialXBSDF>{}, Entry.Key });
	}

	for(const TPair<EInterchangeMaterialXEDF, FSoftObjectPath>& Entry : MaterialXSettings->PredefinedEDF)
	{
		PathToEnumMapping.FindOrAdd(Entry.Value.GetAssetPathString(), EInterchangeMaterialXSettings{ TInPlaceType<EInterchangeMaterialXEDF>{}, Entry.Key });
	}

	for(const TPair<EInterchangeMaterialXVDF, FSoftObjectPath>& Entry : MaterialXSettings->PredefinedVDF)
	{
		PathToEnumMapping.FindOrAdd(Entry.Value.GetAssetPathString(), EInterchangeMaterialXSettings{ TInPlaceType<EInterchangeMaterialXVDF>{}, Entry.Key });
	}
}

void UInterchangeMaterialXPipeline::AdjustSettingsForContext(EInterchangePipelineContext ImportType, TObjectPtr<UObject> ReimportAsset)
{
	Super::AdjustSettingsForContext(ImportType, ReimportAsset);

	if (!MaterialXSettings->AreRequiredPackagesLoaded())
	{
		UE_LOG(LogInterchangePipeline, Warning, TEXT("UInterchangeGenericMaterialPipeline: Some required packages are missing. Material import might be wrong"));
	}
}

void UInterchangeMaterialXPipeline::ExecutePipeline(UInterchangeBaseNodeContainer* NodeContainer, const TArray<UInterchangeSourceData*>& InSourceDatas, const FString& ContentBasePath)
{
	Super::ExecutePipeline(NodeContainer, InSourceDatas, ContentBasePath);

#if WITH_EDITOR
	auto UpdateMaterialXNodes = [this, NodeContainer](const FString& NodeUid, UInterchangeMaterialFunctionCallExpressionFactoryNode* FactorNode)
	{
		const FString MaterialFunctionMemberName = GET_MEMBER_NAME_CHECKED(UMaterialExpressionMaterialFunctionCall, MaterialFunction).ToString();

		FString FunctionShaderNodeUID = FactorNode->GetUniqueID();
		FunctionShaderNodeUID.RemoveFromStart(TEXT("Factory_"));

		const UInterchangeFunctionCallShaderNode* FunctionCallShaderNode = Cast<UInterchangeFunctionCallShaderNode>(NodeContainer->GetNode(FunctionShaderNodeUID));
		
		if(int32 EnumType; FunctionCallShaderNode->GetInt32Attribute(UE::Interchange::MaterialX::Attributes::EnumType, EnumType))
		{
			int32 EnumValue;
			FunctionCallShaderNode->GetInt32Attribute(UE::Interchange::MaterialX::Attributes::EnumValue, EnumValue);
			FactorNode->AddStringAttribute(MaterialFunctionMemberName, MaterialXSettings->GetAssetPathString(MaterialXSettings->ToEnumKey(EnumType, EnumValue)));
		}
		if(FString MaterialFunctionPath; FactorNode->GetStringAttribute(MaterialFunctionMemberName, MaterialFunctionPath))
		{
			if (const EInterchangeMaterialXSettings* EnumPtr = PathToEnumMapping.Find(MaterialFunctionPath))
			{
				FactorNode->AddStringAttribute(MaterialFunctionMemberName, MaterialXSettings->GetAssetPathString(*EnumPtr));
			}
		}
	};

	//Find all translated node we need for this pipeline
	NodeContainer->IterateNodesOfType<UInterchangeMaterialFunctionCallExpressionFactoryNode>(UpdateMaterialXNodes);
#endif
}

bool UMaterialXPipelineSettings::AreRequiredPackagesLoaded()
{
	auto ArePackagesLoaded = [&](const auto& ObjectPaths)
	{
		bool bAllLoaded = true;

		for(const auto& Pair : ObjectPaths)
		{
			const FSoftObjectPath& ObjectPath = Pair.Value;

			if(!ObjectPath.ResolveObject())
			{
				FString PackagePath = ObjectPath.GetLongPackageName();
				if(FPackageName::DoesPackageExist(PackagePath))
				{
					UObject* Asset = ObjectPath.TryLoad();
					if(!Asset)
					{
						UE_LOG(LogInterchangePipeline, Warning, TEXT("Couldn't load %s"), *PackagePath);
						bAllLoaded = false;
					}
#if WITH_EDITOR
					else
					{

						using EnumT = decltype(Pair.Key);

						static_assert(std::is_same_v<EnumT, EInterchangeMaterialXShaders> ||
									  std::is_same_v<EnumT, EInterchangeMaterialXBSDF> ||
									  std::is_same_v<EnumT, EInterchangeMaterialXEDF> ||
									  std::is_same_v<EnumT, EInterchangeMaterialXVDF>,
									  "Enum type not supported");

						uint8 EnumType = UE::Interchange::MaterialX::IndexSurfaceShaders;

						if constexpr(std::is_same_v<EnumT, EInterchangeMaterialXBSDF>)
						{
							EnumType = UE::Interchange::MaterialX::IndexBSDF;
						}
						else if constexpr(std::is_same_v<EnumT, EInterchangeMaterialXEDF>)
						{
							EnumType = UE::Interchange::MaterialX::IndexEDF;
						}
						else if constexpr(std::is_same_v<EnumT, EInterchangeMaterialXVDF>)
						{
							EnumType = UE::Interchange::MaterialX::IndexVDF;
						}

						if(FMaterialXSettings::ValueType* Settings = SettingsInputsOutputs.Find(ToEnumKey(EnumType, uint8(Pair.Key))))
						{
							bAllLoaded = !ShouldFilterAssets(Cast<UMaterialFunction>(Asset), Settings->Key, Settings->Value);
						}
					}
#endif // WITH_EDITOR
				}
				else
				{
					UE_LOG(LogInterchangePipeline, Warning, TEXT("Couldn't find %s"), *PackagePath);
					bAllLoaded = false;
				}
			}
		}

		return bAllLoaded;
	};

	return ArePackagesLoaded(PredefinedSurfaceShaders) && ArePackagesLoaded(PredefinedBSDF) && ArePackagesLoaded(PredefinedEDF) && ArePackagesLoaded(PredefinedVDF);
}

#if WITH_EDITOR
void UMaterialXPipelineSettings::InitPredefinedAssets()
{
	if(bIsSubstrateEnabled)
	{
		TArray<TTuple<EInterchangeMaterialXSettings, FString, FString>> MappingToSubstrate
		{
			{UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXShaders::OpenPBRSurface), OpenPBRSurfaceFunctionsPath, OpenPBRSurfaceSubstratePath},
			{UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXShaders::OpenPBRSurfaceTransmission), OpenPBRTransmissionSurfaceFunctionsPath, OpenPBRTransmissionSurfaceSubstratePath},
			{UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXShaders::Surface), SurfaceFunctionsPath, SurfaceSubstratePath},
			{UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXShaders::Surface), SurfaceFunctionsPath, SurfaceSubstratePath},
			{UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXShaders::StandardSurface), StandardSurfaceFunctionsPath, StandardSurfaceSubstratePath},
			{UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXShaders::StandardSurfaceTransmission), TransmissionSurfaceFunctionsPath, TransmissionSurfaceSubstratePath},
			{UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXShaders::SurfaceUnlit), SurfaceUnlitFunctionsPath, SurfaceUnlitSubstratePath},
			{UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXShaders::UsdPreviewSurface), UsdPreviewSurfaceFunctionsPath, UsdPreviewSurfaceSubstratePath},

			{UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::OrenNayarDiffuse), OrenNayarBSDFFunctionsPath, OrenNayarBSDFSubstratePath},
			{UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::BurleyDiffuse), BurleyDiffuseBSDFFunctionsPath, BurleyDiffuseBSDFSubstratePath},
			{UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::Dielectric), DielectricBSDFFunctionsPath, DielectricBSDFSubstratePath},
			{UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::Conductor), ConductorBSDFFunctionsPath, ConductorBSDFSubstratePath},
			{UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::Sheen), SheenBSDFFunctionsPath, SheenBSDFSubstratePath},
			{UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::Subsurface), SubsurfaceBSDFFunctionsPath, SubsurfaceBSDFSubstratePath},
			{UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::ThinFilm), ThinFilmBSDFFunctionsPath, ThinFilmBSDFSubstratePath},
			{UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::GeneralizedSchlick), GeneralizedSchlickBSDFFunctionsPath, GeneralizedSchlickBSDFSubstratePath},
			{UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXBSDF::Translucent), TranslucentBSDFFunctionsPath, TranslucentBSDFSubstratePath},

			{UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXEDF::Uniform), UniformEDFFunctionsPath, UniformEDFSubstratePath},
			{UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXEDF::Conical), ConicalEDFFunctionsPath, ConicalEDFSubstratePath},
			{UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXEDF::Measured), MeasuredEDFFunctionsPath, MeasuredEDFSubstratePath},

			{UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXVDF::Absorption), AbsorptionVDFFunctionsPath, AbsorptionVDFSubstratePath},
			{UMaterialXPipelineSettings::ToEnumKey(EInterchangeMaterialXVDF::Anisotropic), AnisotropicVDFFunctionsPath, AnisotropicVDFSubstratePath},
		};

		for(const TTuple<EInterchangeMaterialXSettings, FString, FString> & Mapping : MappingToSubstrate)
		{
			const EInterchangeMaterialXSettings& ShadersSettings = Mapping.Get<0>();
			if(FString Path = GetAssetPathString(ShadersSettings); Path == Mapping.Get<1>())
			{
				if(ShadersSettings.IsType<EInterchangeMaterialXShaders>())
				{
					PredefinedSurfaceShaders.Add(ShadersSettings.Get<EInterchangeMaterialXShaders>(), FSoftObjectPath{ Mapping.Get<2>() });
				}
				else if(ShadersSettings.IsType<EInterchangeMaterialXBSDF>())
				{
					PredefinedBSDF.Add(ShadersSettings.Get<EInterchangeMaterialXBSDF>(), FSoftObjectPath{ Mapping.Get<2>() });
				}
				else if(ShadersSettings.IsType<EInterchangeMaterialXEDF>())
				{
					PredefinedEDF.Add(ShadersSettings.Get<EInterchangeMaterialXEDF>(), FSoftObjectPath{ Mapping.Get<2>() });
				}
				else if(ShadersSettings.IsType<EInterchangeMaterialXVDF>())
				{
					PredefinedVDF.Add(ShadersSettings.Get<EInterchangeMaterialXVDF>(), FSoftObjectPath{ Mapping.Get<2>() });
				}
			}
		}
	}
}
#endif // WITH_EDITOR

FString UMaterialXPipelineSettings::GetAssetPathString(EInterchangeMaterialXSettings EnumValue) const
{
	auto FindAssetPathString = [](const auto& PredefinedEnumPath, auto Enum) -> FString
	{
		if(const FSoftObjectPath* ObjectPath = PredefinedEnumPath.Find(Enum))
		{
			return ObjectPath->GetAssetPathString();
		}

		return {};
	};

	if(EnumValue.IsType<EInterchangeMaterialXShaders>())
	{
		return FindAssetPathString(PredefinedSurfaceShaders, EnumValue.Get<EInterchangeMaterialXShaders>());
	}
	else if(EnumValue.IsType<EInterchangeMaterialXBSDF>())
	{
		return FindAssetPathString(PredefinedBSDF, EnumValue.Get<EInterchangeMaterialXBSDF>());
	}
	else if(EnumValue.IsType<EInterchangeMaterialXEDF>())
	{
		return FindAssetPathString(PredefinedEDF, EnumValue.Get<EInterchangeMaterialXEDF>());
	}
	else if(EnumValue.IsType<EInterchangeMaterialXVDF>())
	{
		return FindAssetPathString(PredefinedVDF, EnumValue.Get<EInterchangeMaterialXVDF>());
	}

	return {};
}

#if WITH_EDITOR
bool UMaterialXPipelineSettings::ShouldFilterAssets(UMaterialFunction* Asset, const TSet<FName>& Inputs, const TSet<FName>& Outputs)
{
	int32 InputMatches = 0;
	int32 OutputMatches = 0;

	if (Asset != nullptr)
	{
		TArray<FFunctionExpressionInput> ExpressionInputs;
		TArray<FFunctionExpressionOutput> ExpressionOutputs;
		Asset->GetInputsAndOutputs(ExpressionInputs, ExpressionOutputs);

		for (const FFunctionExpressionInput& ExpressionInput : ExpressionInputs)
		{
			if (Inputs.Find(ExpressionInput.Input.InputName))
			{
				InputMatches++;
			}
		}

		for (const FFunctionExpressionOutput& ExpressionOutput : ExpressionOutputs)
		{
			if (Outputs.Find(ExpressionOutput.Output.OutputName))
			{
				OutputMatches++;
			}
		}
	}

	// we allow at least one input of the same name, but we should have exactly the same outputs
	return !(InputMatches > 0 && OutputMatches == Outputs.Num());
}

EInterchangeMaterialXSettings UMaterialXPipelineSettings::ToEnumKey(uint8 EnumType, uint8 EnumValue)
{
	switch(EnumType)
	{
	case UE::Interchange::MaterialX::IndexBSDF:
		return EInterchangeMaterialXSettings{ TInPlaceType<EInterchangeMaterialXBSDF>{}, EInterchangeMaterialXBSDF{EnumValue} };

	case UE::Interchange::MaterialX::IndexEDF:
		return EInterchangeMaterialXSettings{ TInPlaceType<EInterchangeMaterialXEDF>{}, EInterchangeMaterialXEDF{EnumValue} };

	case UE::Interchange::MaterialX::IndexVDF:
		return EInterchangeMaterialXSettings{ TInPlaceType<EInterchangeMaterialXVDF>{}, EInterchangeMaterialXVDF{EnumValue} };

	default:
		return EInterchangeMaterialXSettings{ TInPlaceType<EInterchangeMaterialXShaders>{}, EInterchangeMaterialXShaders{EnumValue} };
	}
}
#endif // WITH_EDITOR

namespace
{
	static uint8 GetMaterialXSettingsIndexValue(const EInterchangeMaterialXSettings Enum, SIZE_T& Index)
	{
		Index = Enum.GetIndex();
		const uint8* RawValuePointer = 
			Index == UE::Interchange::MaterialX::IndexBSDF ? reinterpret_cast<const uint8*>(Enum.TryGet<EInterchangeMaterialXBSDF>()) :
			Index == UE::Interchange::MaterialX::IndexEDF ? reinterpret_cast<const uint8*>(Enum.TryGet<EInterchangeMaterialXEDF>()) :
			Index == UE::Interchange::MaterialX::IndexVDF ? reinterpret_cast<const uint8*>(Enum.TryGet<EInterchangeMaterialXVDF>()) :
			reinterpret_cast<const uint8*>(Enum.TryGet<EInterchangeMaterialXShaders>());
		return *RawValuePointer;
	}
}

uint32 GetTypeHash(EInterchangeMaterialXSettings Key)
{
	SIZE_T Index;
	const uint8 UnderlyingValue = GetMaterialXSettingsIndexValue(Key, Index);
	return HashCombine(Index, UnderlyingValue);
}

bool operator==(EInterchangeMaterialXSettings Lhs, EInterchangeMaterialXSettings Rhs)
{
	SIZE_T LhsIndex;
	const uint8 LhsUnderlyingValue = GetMaterialXSettingsIndexValue(Lhs, LhsIndex);

	SIZE_T RhsIndex;
	const uint8 RhsUnderlyingValue = GetMaterialXSettingsIndexValue(Rhs, RhsIndex);

	return LhsIndex == RhsIndex && LhsUnderlyingValue == RhsUnderlyingValue;
}

#undef MATERIALX_FUNCTIONS_SUBSTRATE_PATH
#undef MATERIALX_MATERIALFUNCTION_PATH