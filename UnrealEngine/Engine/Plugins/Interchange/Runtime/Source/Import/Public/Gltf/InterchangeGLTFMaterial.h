// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

namespace GLTF {
	struct FMaterial;
	struct FTexture;
}
class UInterchangeShaderGraphNode;
class UInterchangeBaseNodeContainer;

const FString InterchangeGltfMaterialAttributeIdentifier = TEXT("Gltf_MI_AttributeIdentifier_");

//OffsetScale for MaterialInstances
//Offset_X Offset_Y Scale_X Scale_Y for Materials
#define INTERCHANGE_GLTF_STRINGIFY(x) #x
#define DECLARE_INTERCHANGE_GLTF_MI_MAP(MapName) const FString MapName##Texture = TEXT(INTERCHANGE_GLTF_STRINGIFY(MapName ## Texture)); \
												 const FString MapName##Texture_OffsetScale = TEXT(INTERCHANGE_GLTF_STRINGIFY(MapName ## Texture_OffsetScale)); \
												 const FString MapName##Texture_Offset_X = TEXT(INTERCHANGE_GLTF_STRINGIFY(MapName ## Texture_Offset_X)); \
												 const FString MapName##Texture_Offset_Y = TEXT(INTERCHANGE_GLTF_STRINGIFY(MapName ## Texture_Offset_Y)); \
												 const FString MapName##Texture_Scale_X = TEXT(INTERCHANGE_GLTF_STRINGIFY(MapName ## Texture_Scale_X)); \
												 const FString MapName##Texture_Scale_Y = TEXT(INTERCHANGE_GLTF_STRINGIFY(MapName ## Texture_Scale_Y)); \
												 const FString MapName##Texture_Rotation = TEXT(INTERCHANGE_GLTF_STRINGIFY(MapName ## Texture_Rotation)); \
												 const FString MapName##Texture_TexCoord = TEXT(INTERCHANGE_GLTF_STRINGIFY(MapName ## Texture_TexCoord)); \
												 const FString MapName##Texture_TilingMethod = TEXT(INTERCHANGE_GLTF_STRINGIFY(MapName ## Texture_TilingMethod));

namespace UE::Interchange::GLTFMaterials
{
	//Inputs/Parameters (Materials/MaterialInstances)
	namespace Inputs
	{
		///PostFixes
		namespace PostFix
		{
			const FString Color_RGB = TEXT("_RGB");
			const FString Color_A = TEXT("_A");

			const FString TexCoord = TEXT("_TexCoord");

			const FString OffsetX = TEXT("_Offset_X");
			const FString OffsetY = TEXT("_Offset_Y");
			const FString ScaleX = TEXT("_Scale_X");
			const FString ScaleY = TEXT("_Scale_Y");

			const FString OffsetScale = TEXT("_OffsetScale");

			const FString Rotation = TEXT("_Rotation");

			const FString TilingMethod = TEXT("_TilingMethod");
		}

		//MetalRoughness specific:
		DECLARE_INTERCHANGE_GLTF_MI_MAP(BaseColor)
		const FString BaseColorFactor = TEXT("BaseColorFactor");
		const FString BaseColorFactor_RGB = BaseColorFactor + PostFix::Color_RGB; //Connection to inputs from BaseColorFactor.RGB
		const FString BaseColorFactor_A = BaseColorFactor + PostFix::Color_A; //Connection to inputs from BaseColorFactor.A

		DECLARE_INTERCHANGE_GLTF_MI_MAP(MetallicRoughness)
		const FString MetallicFactor = TEXT("MetallicFactor");
		const FString RoughnessFactor = TEXT("RoughnessFactor");

		DECLARE_INTERCHANGE_GLTF_MI_MAP(Specular)
		const FString SpecularFactor = TEXT("SpecularFactor");


		//SpecularGlossiness specific
		DECLARE_INTERCHANGE_GLTF_MI_MAP(Diffuse)
		const FString DiffuseFactor = TEXT("DiffuseFactor");
		const FString DiffuseFactor_RGB = DiffuseFactor + PostFix::Color_RGB; //Connection to inputs from BaseColorFactor.RGB
		const FString DiffuseFactor_A = DiffuseFactor + PostFix::Color_A; //Connection to inputs from BaseColorFactor.A

		DECLARE_INTERCHANGE_GLTF_MI_MAP(SpecularGlossiness)
		const FString SpecFactor = TEXT("SpecFactor");
		const FString GlossinessFactor = TEXT("GlossinessFactor");


		//Generic:
		DECLARE_INTERCHANGE_GLTF_MI_MAP(Normal)
		const FString NormalScale = TEXT("NormalScale");

		DECLARE_INTERCHANGE_GLTF_MI_MAP(Emissive)
		const FString EmissiveFactor = TEXT("EmissiveFactor");
		const FString EmissiveStrength = TEXT("EmissiveStrength");

		DECLARE_INTERCHANGE_GLTF_MI_MAP(Occlusion)
		const FString OcclusionStrength = TEXT("OcclusionStrength");

		const FString IOR = TEXT("IOR");

		const FString AlphaCutoff = TEXT("AlphaCutoff");
		const FString AlphaMode = TEXT("AlphaMode"); //Specifically for Transmission Materials

		//ClearCoat specific:
		DECLARE_INTERCHANGE_GLTF_MI_MAP(ClearCoat)
		const FString ClearCoatFactor = TEXT("ClearCoatFactor");

		DECLARE_INTERCHANGE_GLTF_MI_MAP(ClearCoatRoughness)
		const FString ClearCoatRoughnessFactor = TEXT("ClearCoatRoughnessFactor");

		DECLARE_INTERCHANGE_GLTF_MI_MAP(ClearCoatNormal)
		const FString ClearCoatNormalScale = TEXT("ClearCoatNormalScale");


		//Sheen specific:
		DECLARE_INTERCHANGE_GLTF_MI_MAP(SheenColor)
		const FString SheenColorFactor = TEXT("SheenColorFactor");

		DECLARE_INTERCHANGE_GLTF_MI_MAP(SheenRoughness)
		const FString SheenRoughnessFactor = TEXT("SheenRoughnessFactor");


		//Transmission specific:
		DECLARE_INTERCHANGE_GLTF_MI_MAP(Transmission)
		const FString TransmissionFactor = TEXT("TransmissionFactor");


		//Iridescence specific:
		const FString IridescenceIOR = TEXT("IridescenceIOR");
		
		DECLARE_INTERCHANGE_GLTF_MI_MAP(Iridescence)
		const FString IridescenceFactor = TEXT("IridescenceFactor");

		DECLARE_INTERCHANGE_GLTF_MI_MAP(IridescenceThickness)
		const FString IridescenceThicknessMinimum = TEXT("IridescenceThicknessMinimum");
		const FString IridescenceThicknessMaximum = TEXT("IridescenceThicknessMaximum");
	}

	enum EShadingModel : uint8
	{
		DEFAULT = 0, //MetalRoughness
		UNLIT,
		CLEARCOAT,
		SHEEN,
		TRANSMISSION,
		SPECULARGLOSSINESS,
		SHADINGMODELCOUNT
	};

	enum EAlphaMode : uint8
	{
		Opaque = 0,
		Mask,
		Blend
	};

	struct FGLTFMaterialInformation
	{
		FString MaterialFunctionPath;
		FString MaterialPath;
		TArray<FString> MaterialFunctionOutputs;

		FGLTFMaterialInformation(const FString& InMaterialFunctionPath,
			const FString& InMaterialPath,
			const TArray<FString>& InMaterialFunctionOutputs)
			: MaterialFunctionPath(InMaterialFunctionPath)
			, MaterialPath(InMaterialPath)
			, MaterialFunctionOutputs(InMaterialFunctionOutputs)
		{
		}
	};

	static const TMap<EShadingModel, FGLTFMaterialInformation> ShadingModelToMaterialInformation = {
		{EShadingModel::DEFAULT,
		FGLTFMaterialInformation(
			TEXT("/Interchange/gltf/MaterialBodies/MF_Default_Body.MF_Default_Body"),
			TEXT("/Interchange/gltf/M_Default.M_Default"),
			TArray<FString>{
					TEXT("BaseColor"),
					TEXT("Metallic"),
					TEXT("Specular"),
					TEXT("Roughness"),
					TEXT("EmissiveColor"),
					TEXT("Opacity"),
					TEXT("OpacityMask"),
					TEXT("Normal"),
					TEXT("Occlusion")})
		},

		{EShadingModel::UNLIT,
		FGLTFMaterialInformation(
			TEXT("/Interchange/gltf/MaterialBodies/MF_Unlit_Body.MF_Unlit_Body"),
			TEXT("/Interchange/gltf/M_Unlit.M_Unlit"),
			TArray<FString>{
					TEXT("UnlitColor"),
					TEXT("Opacity"),
					TEXT("OpacityMask")})
		},

		{EShadingModel::CLEARCOAT,
		FGLTFMaterialInformation(
			TEXT("/Interchange/gltf/MaterialBodies/MF_ClearCoat_Body.MF_ClearCoat_Body"),
			TEXT("/Interchange/gltf/M_ClearCoat.M_ClearCoat"),
			TArray<FString>{
					TEXT("ClearCoatNormal"),
					TEXT("BaseColor"),
					TEXT("Metallic"),
					TEXT("Specular"),
					TEXT("Roughness"),
					TEXT("EmissiveColor"),
					TEXT("Opacity"),
					TEXT("OpacityMask"),
					TEXT("Normal"),
					TEXT("ClearCoat"),
					TEXT("ClearCoatRoughness"),
					TEXT("Occlusion")})
		},

		{EShadingModel::SHEEN,
		FGLTFMaterialInformation(
			TEXT("/Interchange/gltf/MaterialBodies/MF_Sheen_Body.MF_Sheen_Body"),
			TEXT("/Interchange/gltf/M_Sheen.M_Sheen"),
			TArray<FString>{
					TEXT("BaseColor"),
					TEXT("Metallic"),
					TEXT("Specular"),
					TEXT("Roughness"),
					TEXT("EmissiveColor"),
					TEXT("Opacity"),
					TEXT("OpacityMask"),
					TEXT("Normal"),
					TEXT("SheenColor"),
					TEXT("SheenRoughness"),
					TEXT("Occlusion")})
		},

		{EShadingModel::TRANSMISSION,
		FGLTFMaterialInformation(
			TEXT("/Interchange/gltf/MaterialBodies/MF_Transmission_Body.MF_Transmission_Body"),
			TEXT("/Interchange/gltf/M_Transmission.M_Transmission"),
			TArray<FString>{
					TEXT("TransmissionColor"),
					TEXT("BaseColor"),
					TEXT("Metallic"),
					TEXT("Specular"),
					TEXT("Roughness"),
					TEXT("EmissiveColor"),
					TEXT("Opacity"),
					TEXT("Normal"),
					TEXT("Occlusion")})
		},

		{EShadingModel::SPECULARGLOSSINESS,
		FGLTFMaterialInformation(
			TEXT("/Interchange/gltf/MaterialBodies/MF_SpecularGlossiness_Body.MF_SpecularGlossiness_Body"),
			TEXT("/Interchange/gltf/M_SpecularGlossiness.M_SpecularGlossiness"),
			TArray<FString>{
					TEXT("BaseColor"),
					TEXT("Metallic"),
					TEXT("Roughness"),
					TEXT("EmissiveColor"),
					TEXT("Opacity"),
					TEXT("OpacityMask"),
					TEXT("Normal"),
					TEXT("Occlusion")})
		}
	};

	inline TArray<FString> GetRequiredMaterialFunctionPaths()
	{
		TArray<FString> Result;
		for (const TPair<EShadingModel, FGLTFMaterialInformation>& Entry : ShadingModelToMaterialInformation)
		{
			Result.Add(Entry.Value.MaterialFunctionPath);
		}
		return Result;
	}

	inline TMap<FString, EShadingModel> GetMaterialFunctionPathsToShadingModels()
	{
		TMap<FString, EShadingModel> Result;
		for (const TPair<EShadingModel, FGLTFMaterialInformation>& Entry : ShadingModelToMaterialInformation)
		{
			Result.Add(Entry.Value.MaterialFunctionPath, Entry.Key);
		}
		return Result;
	}

	inline TMap<FString, EShadingModel> GetMaterialPathsToShadingModels()
	{
		TMap<FString, EShadingModel> Result;
		for (const TPair<EShadingModel, FGLTFMaterialInformation>& Entry : ShadingModelToMaterialInformation)
		{
			Result.Add(Entry.Value.MaterialPath, Entry.Key);
		}
		return Result;
	}

	void HandleGltfMaterial(UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FMaterial& GltfMaterial, const TArray<GLTF::FTexture>& Textures, UInterchangeShaderGraphNode* ShaderGraphNode);

	bool AreRequiredPackagesLoaded();
};
