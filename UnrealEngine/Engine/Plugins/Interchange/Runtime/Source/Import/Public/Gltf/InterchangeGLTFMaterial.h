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
												 const FString MapName##Texture_TexCoord = TEXT(INTERCHANGE_GLTF_STRINGIFY(MapName ## Texture_TexCoord));

namespace UE::Interchange::GLTFMaterials
{
	//Inputs/Parameters (Materials/MaterialInstances)
	namespace Inputs
	{
		const FString Color_RGB = TEXT("_RGB");
		const FString Color_A = TEXT("_A");

		//MetalRoughness specific:
		DECLARE_INTERCHANGE_GLTF_MI_MAP(BaseColor)
		const FString BaseColorFactor = TEXT("BaseColorFactor");
		const FString BaseColorFactor_RGB = BaseColorFactor + Color_RGB; //Connection to inputs from BaseColorFactor.RGB
		const FString BaseColorFactor_A = BaseColorFactor + Color_A; //Connection to inputs from BaseColorFactor.A

		DECLARE_INTERCHANGE_GLTF_MI_MAP(MetallicRoughness)
		const FString MetallicFactor = TEXT("MetallicFactor");
		const FString RoughnessFactor = TEXT("RoughnessFactor");

		DECLARE_INTERCHANGE_GLTF_MI_MAP(Specular)
		const FString SpecularFactor = TEXT("SpecularFactor");


		//SpecularGlossiness specific
		DECLARE_INTERCHANGE_GLTF_MI_MAP(Diffuse)
		const FString DiffuseFactor = TEXT("DiffuseFactor");
		const FString DiffuseFactor_RGB = DiffuseFactor + Color_RGB; //Connection to inputs from BaseColorFactor.RGB
		const FString DiffuseFactor_A = DiffuseFactor + Color_A; //Connection to inputs from BaseColorFactor.A

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

	static TMap<EShadingModel, TPair<FString, TArray<FString>>> ShadingModelToMaterialFunctions = {
		{EShadingModel::DEFAULT,
			TPair<FString, TArray<FString>>(
				TEXT("/Interchange/gltf/MaterialBodies/MF_Default_Body.MF_Default_Body"),
				TArray<FString>{
					TEXT("BaseColor"),
					TEXT("Metallic"),
					TEXT("Specular"),
					TEXT("Roughness"),
					TEXT("EmissiveColor"),
					TEXT("Opacity"),
					TEXT("OpacityMask"),
					TEXT("Normal"),
					TEXT("Occlusion")})},

		{EShadingModel::UNLIT,
			TPair<FString, TArray<FString>>(
				TEXT("/Interchange/gltf/MaterialBodies/MF_Unlit_Body.MF_Unlit_Body"),
				TArray<FString>{
					TEXT("UnlitColor"),
					TEXT("Opacity"),
					TEXT("OpacityMask")})},

		{EShadingModel::CLEARCOAT,
			TPair<FString, TArray<FString>>(
				TEXT("/Interchange/gltf/MaterialBodies/MF_ClearCoat_Body.MF_ClearCoat_Body"),
				TArray<FString>{
					TEXT("ClearCoatBottomNormal"),
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
					TEXT("Occlusion")})},

		{EShadingModel::SHEEN,
			TPair<FString, TArray<FString>>(
				TEXT("/Interchange/gltf/MaterialBodies/MF_Sheen_Body.MF_Sheen_Body"),
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
					TEXT("Occlusion")})},

		{EShadingModel::TRANSMISSION,
			TPair<FString, TArray<FString>>(
				TEXT("/Interchange/gltf/MaterialBodies/MF_Transmission_Body.MF_Transmission_Body"),
				TArray<FString>{
					TEXT("TransmissionColor"),
					TEXT("BaseColor"),
					TEXT("Metallic"),
					TEXT("Specular"),
					TEXT("Roughness"),
					TEXT("EmissiveColor"),
					TEXT("Opacity"),
					TEXT("Normal"),
					TEXT("Occlusion"),
					TEXT("Refraction")})},

		{EShadingModel::SPECULARGLOSSINESS,
			TPair<FString, TArray<FString>>(
				TEXT("/Interchange/gltf/MaterialBodies/MF_SpecularGlossiness_Body.MF_SpecularGlossiness_Body"),
				TArray<FString>{
					TEXT("BaseColor"),
					TEXT("Metallic"),
					TEXT("Roughness"),
					TEXT("EmissiveColor"),
					TEXT("Opacity"),
					TEXT("OpacityMask"),
					TEXT("Normal"),
					TEXT("Occlusion")})},
	};

	inline TArray<FString> GetRequiredMaterialFunctionPaths()
	{
		TArray<FString> Result;
		for (const TPair<EShadingModel, TPair<FString, TArray<FString>>>& Entry : ShadingModelToMaterialFunctions)
		{
			Result.Add(Entry.Value.Key);
		}
		return Result;
	}

	void HandleGltfMaterial(UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FMaterial& GltfMaterial, const TArray<GLTF::FTexture>& Textures, UInterchangeShaderGraphNode* ShaderGraphNode);

	bool AreRequiredPackagesLoaded();
};
