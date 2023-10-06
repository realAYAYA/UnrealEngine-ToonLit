// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gltf/InterchangeGLTFMaterial.h"
#include "GLTFMaterial.h"
#include "GLTFTexture.h"

#include "Nodes/InterchangeBaseNodeContainer.h"
#include "InterchangeTextureNode.h"
#include "InterchangeShaderGraphNode.h"
#include "InterchangeMaterialInstanceNode.h"
#include "InterchangeShaderGraphNode.h"
#include "InterchangeMaterialDefinitions.h"
#include "InterchangeImportLog.h"

#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Async/Async.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"

namespace UE::Interchange::GLTFMaterials
{

	FString ToString(EShadingModel ShadingModel)
	{
		switch (ShadingModel)
		{
		case DEFAULT:
			return TEXT("Default");
		case UNLIT:
			return TEXT("Unlit");
		case CLEARCOAT:
			return TEXT("ClearCoat");
		case SHEEN:
			return TEXT("Sheen");
		case TRANSMISSION:
			return TEXT("Transmission");
		case SPECULARGLOSSINESS:
			return TEXT("SpecularGlossiness");
		default:
			return TEXT("Unexpected");
		}
	}

	FString ToString(EAlphaMode AlphaMode)
	{
		switch (AlphaMode)
		{
		case EAlphaMode::Opaque:
			return TEXT("Opaque");
		case EAlphaMode::Mask:
			return TEXT("Mask");
		case EAlphaMode::Blend:
			return TEXT("Blend");
		default:
			return TEXT("Unexpected");
		}
	}

	FString GetIdentifier(EShadingModel ShadingModel, EAlphaMode AlphaMode, bool bTwoSided)
	{
		FString Name = TEXT("MI_");

		Name += ToString(ShadingModel);

		if (ShadingModel != EShadingModel::TRANSMISSION)
		{
			Name += TEXT("_") + ToString(AlphaMode);
		}

		Name += bTwoSided ? TEXT("_DS") : TEXT("");

		return Name;
	}

	FString GetMaterialFunctionPath(EShadingModel ShadingModel)
	{
		if (ShadingModelToMaterialFunctions.Contains(ShadingModel))
		{
			return ShadingModelToMaterialFunctions[ShadingModel].Key;
		}

		ensure(false);
		return ShadingModelToMaterialFunctions.begin()->Value.Key;
	}

	TArray<FString> GetOutputs(EShadingModel ShadingModel)
	{
		if (ShadingModelToMaterialFunctions.Contains(ShadingModel))
		{
			return ShadingModelToMaterialFunctions[ShadingModel].Value;
		}

		ensure(false);
		return ShadingModelToMaterialFunctions.begin()->Value.Value;
	}

	//Precedence order based on the logic in the InterchangeGenericMaterialPipeline: ClearCoat > Sheen > unlit
	EShadingModel GetShadingModel(GLTF::FMaterial::EShadingModel GltfShadingModel, bool bHasClearCoat, bool bHasSheen, bool bUnlit, bool bHasTransmission)
	{
		if (bUnlit)
		{
			return EShadingModel::UNLIT;
		}

		if (GltfShadingModel == GLTF::FMaterial::EShadingModel::SpecularGlossiness)
		{
			return EShadingModel::SPECULARGLOSSINESS;
		}

		if (bHasTransmission)
		{
			return EShadingModel::TRANSMISSION;
		}

		if (bHasClearCoat)
		{
			return EShadingModel::CLEARCOAT;
		}

		if (bHasSheen)
		{
			return EShadingModel::SHEEN;
		}

		return EShadingModel::DEFAULT;
	}

	FString GetMaterialInstanceInputName(const FString& Name)
	{
		return *InterchangeGltfMaterialAttributeIdentifier + Name;
	}

	FString GetMaterialInputName(const FString& Name)
	{
		return UInterchangeShaderPortsAPI::MakeInputValueKey(Name);
	}

	struct FGLTFMaterialProcessor
	{
		UInterchangeBaseNodeContainer& NodeContainer;
		const GLTF::FMaterial& GltfMaterial;
		const TArray<GLTF::FTexture>& Textures;
		
		UInterchangeShaderGraphNode* ShaderGraphNode = nullptr; //Set by the Constructor
		UInterchangeShaderNode* MaterialNode = nullptr;			//Set by ConfigureMaterialNode
		UInterchangeShaderNode* MaterialInstanceNode = nullptr; //Set by ConfigureMaterialInstanceNode

		EShadingModel ShadingModel = EShadingModel::DEFAULT;
		EAlphaMode AlphaMode = EAlphaMode::Opaque;

		FGLTFMaterialProcessor(UInterchangeBaseNodeContainer& InNodeContainer, const GLTF::FMaterial& InGltfMaterial, const TArray<GLTF::FTexture>& InTextures, UInterchangeShaderGraphNode* InShaderGraphNode)
			: NodeContainer(InNodeContainer)
			, GltfMaterial(InGltfMaterial)
			, Textures(InTextures)
			, ShaderGraphNode(InShaderGraphNode)
		{
			//Generate base properties:
			ShadingModel = GetShadingModel(GltfMaterial.ShadingModel, GltfMaterial.bHasClearCoat, GltfMaterial.bHasSheen, GltfMaterial.bIsUnlitShadingModel, GltfMaterial.bHasTransmission);
			AlphaMode = EAlphaMode(GltfMaterial.AlphaMode);

			//Configure Nodes:
			ConfigureMaterialNode();
			ConfigureMaterialInstanceNode();
		}

		////////////////////////////
		//Start of Config Functions:
		////////////////////////////
		void ConfigureMaterialNode()
		{
			FString MaterialFunctionPath = GetMaterialFunctionPath(ShadingModel);
			FString MaterailFunctionFileName = FPaths::GetBaseFilename(MaterialFunctionPath);

			UInterchangeFunctionCallShaderNode* MF_Body_Node = NewObject<UInterchangeFunctionCallShaderNode>(&NodeContainer);
			MF_Body_Node->InitializeNode(UInterchangeShaderNode::MakeNodeUid(MaterailFunctionFileName, ShaderGraphNode->GetUniqueID()), MaterailFunctionFileName, EInterchangeNodeContainerType::TranslatedAsset);
			MF_Body_Node->SetCustomMaterialFunction(MaterialFunctionPath);
			NodeContainer.AddNode(MF_Body_Node);

			MaterialNode = MF_Body_Node;

			ShaderGraphNode->SetCustomTwoSided(GltfMaterial.bIsDoubleSided);
			if (ShadingModel == EShadingModel::TRANSMISSION)
			{
				ShaderGraphNode->SetCustomScreenSpaceReflections(true);
				ShaderGraphNode->SetCustomTwoSidedTransmission(true);
			}

			TArray<FString> Outputs = GetOutputs(ShadingModel);

			switch (AlphaMode)
			{
			case UE::Interchange::GLTFMaterials::Opaque:
				Outputs.Remove(TEXT("Opacity"));
				Outputs.Remove(TEXT("OpacityMask"));
				break;
			case UE::Interchange::GLTFMaterials::Mask:
				if (ShadingModel != EShadingModel::TRANSMISSION)
				{
					Outputs.Remove(TEXT("Opacity"));
				}
				break;
			case UE::Interchange::GLTFMaterials::Blend:
				Outputs.Remove(TEXT("OpacityMask"));
				break;
			default:
				break;
			}

			for (const FString& Input : Outputs)
			{
				UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, Input, MF_Body_Node->GetUniqueID(), Input);
			}

			//Set OpacityMaskClipValue if needed:
			if (!GltfMaterial.bIsUnlitShadingModel &&
				GltfMaterial.AlphaMode == GLTF::FMaterial::EAlphaMode::Mask)
			{
				if (ShadingModel == EShadingModel::TRANSMISSION)
				{
					//Transmission (even for Materials) supports AlphaCutoff through the MF_Body instead of the automatic OpacityMaskClipValue
					SetScalar(Inputs::AlphaCutoff, GltfMaterial.AlphaCutoff, 0.f, EProcessType::MATERIAL);
				}
				else
				{
					ShaderGraphNode->SetCustomOpacityMaskClipValue(GltfMaterial.AlphaCutoff);
				}
			}
		}

		void ConfigureMaterialInstanceNode()
		{
			MaterialInstanceNode = ShaderGraphNode;

			FString ParentIdentifier = GetIdentifier(ShadingModel, AlphaMode, GltfMaterial.bIsDoubleSided);
			MaterialInstanceNode->AddStringAttribute(*(InterchangeGltfMaterialAttributeIdentifier + TEXT("ParentIdentifier")), ParentIdentifier);

			//Set AlphaCutOff if needed:
			if (!GltfMaterial.bIsUnlitShadingModel &&
				GltfMaterial.AlphaMode == GLTF::FMaterial::EAlphaMode::Mask)
			{
				//AlphaCutoff
				SetScalar(Inputs::AlphaCutoff, GltfMaterial.AlphaCutoff, 0.f, EProcessType::MATERIALINSTANCE);
			}
		}
		////////////////////////////
		//End of Config Functions:
		////////////////////////////


		///////////////////////////////////
		//Start of Helper Setter Functions:
		///////////////////////////////////
		enum EProcessType : uint8
		{
			MATERIAL = 1,
			MATERIALINSTANCE = 2
		};

		void SetScalar(const FString& Name, float Value, float DefaultValue, int ProcessType = (EProcessType::MATERIAL | EProcessType::MATERIALINSTANCE))
		{
			if (!FMath::IsNearlyEqual(Value, DefaultValue))
			{
				if (ProcessType & EProcessType::MATERIAL)
				{
					MaterialNode->AddFloatAttribute(GetMaterialInputName(Name), Value);
				}

				if (ProcessType & EProcessType::MATERIALINSTANCE)
				{
					MaterialInstanceNode->AddFloatAttribute(GetMaterialInstanceInputName(Name), Value);
				}
			}
		}

		void SetVec3(const FString& Name, const FVector3f& Value, const FVector3f& DefaultValue, int ProcessType = (EProcessType::MATERIAL | EProcessType::MATERIALINSTANCE))
		{
			if (!Value.Equals(DefaultValue, UE_SMALL_NUMBER))
			{

				if (ProcessType & EProcessType::MATERIAL)
				{
					MaterialNode->AddLinearColorAttribute(GetMaterialInputName(Name), Value);
				}

				if (ProcessType & EProcessType::MATERIALINSTANCE)
				{
					MaterialInstanceNode->AddLinearColorAttribute(GetMaterialInstanceInputName(Name), Value);
				}
			}
		}

		void SetVec4(const FString& Name, const FVector4f& Value, const FVector4f& DefaultValue, int ProcessType = (EProcessType::MATERIAL | EProcessType::MATERIALINSTANCE))
		{
			if (!Value.Equals(DefaultValue, UE_SMALL_NUMBER))
			{
				if (ProcessType & EProcessType::MATERIAL)
				{
					MaterialNode->AddLinearColorAttribute(GetMaterialInputName(Name), Value);
				}

				if (ProcessType & EProcessType::MATERIALINSTANCE)
				{
					MaterialInstanceNode->AddLinearColorAttribute(GetMaterialInstanceInputName(Name), Value);
				}
			}
		}

		void SetColor(const FString& Name, const FVector4f& Value, const FVector4f& DefaultValue)
		{
			{//Material specific settings:
				SetVec3(Name + Inputs::Color_RGB, FVector3f(Value), FVector3f(DefaultValue), EProcessType::MATERIAL);
				SetScalar(Name + Inputs::Color_A, Value.W, DefaultValue.W, EProcessType::MATERIAL);
			}

			{//MaterialInstance specific settings:
				SetVec4(Name, Value, DefaultValue, EProcessType::MATERIALINSTANCE);
			}
		}

		void SetMap(const GLTF::FTextureMap& TextureMap, const FString& Name)
		{
			if (Textures.IsValidIndex(TextureMap.TextureIndex))
			{
				{//Material specific settings:
					//create TextureObject:
					UInterchangeShaderNode* ColorNode = UInterchangeShaderNode::Create(&NodeContainer, Name, MaterialNode->GetUniqueID());
					ColorNode->SetCustomShaderType(UE::Interchange::Materials::Standard::Nodes::TextureObject::Name.ToString());

					ColorNode->AddStringAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(UE::Interchange::Materials::Standard::Nodes::TextureObject::Inputs::Texture.ToString()), UInterchangeTextureNode::MakeNodeUid(Textures[TextureMap.TextureIndex].UniqueId));

					UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(MaterialNode, Name, ColorNode->GetUniqueID());
				}

				{//MaterialInstance specific settings:
					MaterialInstanceNode->AddStringAttribute(GetMaterialInstanceInputName(Name), UInterchangeTextureNode::MakeNodeUid(Textures[TextureMap.TextureIndex].UniqueId));
				}

				//TexCoord decision in MaterialInstances work based on a Switch Node
				//Currently Only supports UV0 and UV1
				// [0...1) -> UV0
				// [1...2) -> UV1
				// [2...3) -> UV2
				// [3...4) -> UV3
				// else    -> UV0 (defaults to 0)
				SetScalar(Name + TEXT("_TexCoord"), TextureMap.TexCoord, 0.f);
			}

			if (TextureMap.bHasTextureTransform)
			{
				{//Material specific settings:
					SetScalar(Name + TEXT("_Offset_X"), TextureMap.TextureTransform.Offset[0], 0.f, EProcessType::MATERIAL);
					SetScalar(Name + TEXT("_Offset_Y"), TextureMap.TextureTransform.Offset[1], 0.f, EProcessType::MATERIAL);

					SetScalar(Name + TEXT("_Scale_X"), TextureMap.TextureTransform.Scale[0], 1.f, EProcessType::MATERIAL);
					SetScalar(Name + TEXT("_Scale_Y"), TextureMap.TextureTransform.Scale[1], 1.f, EProcessType::MATERIAL);
				}

				{//MaterialInstance specific settings:
					FVector4f OffsetScale(TextureMap.TextureTransform.Offset[0], TextureMap.TextureTransform.Offset[1], TextureMap.TextureTransform.Scale[0], TextureMap.TextureTransform.Scale[1]);
					SetVec4(Name + TEXT("_OffsetScale"), OffsetScale, FVector4f(0, 0, 1, 1), EProcessType::MATERIALINSTANCE);
				}

				if (!FMath::IsNearlyZero(TextureMap.TextureTransform.Rotation))
				{
					float AngleRadians = TextureMap.TextureTransform.Rotation;

					if (AngleRadians < 0.0f)
					{
						AngleRadians = TWO_PI - AngleRadians;
					}

					AngleRadians = 1.0f - (AngleRadians / TWO_PI);

					SetScalar(Name + TEXT("_Rotation"), AngleRadians, 0.f);
				}
			}
		}
		////////////////////////////////
		//End Of Helper Setter Functions
		////////////////////////////////
		

		////////////////////////////////////////////////////////////////////////////////////
		/// Parse Material Values from GltfMaterial to MaterialNode and MaterialInstanceNode
		////////////////////////////////////////////////////////////////////////////////////
		void ParseMaterialValues()
		{
			if (GltfMaterial.bIsUnlitShadingModel)
			{
				//BaseColorTexture
				//BaseColorTexture_OffsetScale
				//BaseColorTExture_Rotation
				//BaseColorTExture_TexCoord
				SetMap(GltfMaterial.BaseColor, Inputs::BaseColorTexture);

				//BaseColorFactor
				SetColor(Inputs::BaseColorFactor, GltfMaterial.BaseColorFactor, FVector4f(1, 1, 1, 1));
				return;
			}

			{
				//NormalTexture
				//NormalTexture_OffsetScale
				//NormalTexture_Rotation
				//NormalTexture_TexCoord
				SetMap(GltfMaterial.Normal, Inputs::NormalTexture);

				//NormalScale
				SetScalar(Inputs::NormalScale, GltfMaterial.NormalScale, 1.f);
			}

			if (!GltfMaterial.bHasTransmission)
			{
				//EmissiveTexture
				//EmissiveTexture_OffsetScale
				//EmissiveTexture_Rotation
				//EmissiveTexture_TexCoord
				SetMap(GltfMaterial.Emissive, Inputs::EmissiveTexture);

				//EmissiveFactor
				SetVec3(Inputs::EmissiveFactor, GltfMaterial.EmissiveFactor, FVector3f(0, 0, 0));

				//EmissiveStrength
				SetScalar(Inputs::EmissiveStrength, GltfMaterial.bHasEmissiveStrength ? GltfMaterial.EmissiveStrength : 1.f, 1.f);
			}

			{
				//OcclusionTexture
				//OcclusionTexture_OffsetScale
				//OcclusionTexture_Rotation
				//OcclusionTexture_TexCoord
				SetMap(GltfMaterial.Occlusion, Inputs::OcclusionTexture);

				//OcclusionStrength
				SetScalar(Inputs::OcclusionStrength, GltfMaterial.OcclusionStrength, 1.f);
			}

			if (GltfMaterial.ShadingModel == GLTF::FMaterial::EShadingModel::SpecularGlossiness)
			{
				//////////////////////
				//specular glossiness:
				//////////////////////

				{
					//DiffuseTexture
					//DiffuseTexture_OffsetScale
					//DiffuseTexture_Rotation
					//DiffuseTexture_TexCoord
					SetMap(GltfMaterial.BaseColor, Inputs::DiffuseTexture);

					//DiffuseFactor
					SetColor(Inputs::DiffuseFactor, GltfMaterial.BaseColorFactor, FVector4f(1, 1, 1, 1));
				}

				{
					//SpecularGlossinessTexture
					//SpecularGlossinessTexture_OffsetScale
					//SpecularGlossinessTexture_Rotation
					//SpecularGlossinessTexture_TexCoord
					SetMap(GltfMaterial.SpecularGlossiness.Map, Inputs::SpecularGlossinessTexture);

					//SpecFactor
					FVector3f SpecularFactor(GltfMaterial.SpecularGlossiness.SpecularFactor[0], GltfMaterial.SpecularGlossiness.SpecularFactor[1], GltfMaterial.SpecularGlossiness.SpecularFactor[2]);
					SetVec3(Inputs::SpecFactor, SpecularFactor, FVector3f(1, 1, 1));

					//GlossinessFactor
					SetScalar(Inputs::GlossinessFactor, GltfMaterial.SpecularGlossiness.GlossinessFactor, 1.f);
				}

				return;
			}
			else if (GltfMaterial.ShadingModel == GLTF::FMaterial::EShadingModel::MetallicRoughness)
			{
				{
					//BaseColorTexture
					//BaseColorTexture_OffsetScale
					//BaseColorTexture_Rotation
					//BaseColorTexture_TexCoord
					SetMap(GltfMaterial.BaseColor, Inputs::BaseColorTexture);

					//BaseColorFactor
					SetColor(Inputs::BaseColorFactor, GltfMaterial.BaseColorFactor, FVector4f(1, 1, 1, 1));
				}

				{
					//MetallicRoughnessTexture
					//MetallicRoughnessTexture_OffsetScale
					//MetallicRoughnessTexture_Rotation
					//MetallicRoughnessTexture_TexCoord
					SetMap(GltfMaterial.MetallicRoughness.Map, Inputs::MetallicRoughnessTexture);

					//MetallicFactor
					SetScalar(Inputs::MetallicFactor, GltfMaterial.MetallicRoughness.MetallicFactor, 1.f);

					//RoughnessFactor
					SetScalar(Inputs::RoughnessFactor, GltfMaterial.MetallicRoughness.RoughnessFactor, 1.f);
				}

				if (GltfMaterial.bHasSpecular)
				{
					//SpecularTexture
					//SpecularTexture_OffsetScale
					//SpecularTexture_Rotation
					//SpecularTexture_TexCoord
					SetMap(GltfMaterial.Specular.SpecularMap, Inputs::SpecularTexture);

					//SpecularFactor
					SetScalar(Inputs::SpecularFactor, GltfMaterial.Specular.SpecularFactor, 1.f);
				}
			}
			else
			{
				ensure(false);
			}

			if (GltfMaterial.bHasIOR)
			{
				//ior
				SetScalar(Inputs::IOR, GltfMaterial.IOR, 1.5f);
			}

			if (GltfMaterial.bHasTransmission)
			{
				{
					//TransmissionTexture
					//TransmissionTexture_OffsetScale
					//TransmissionTexture_Rotation
					//TransmissionTexture_TexCoord
					SetMap(GltfMaterial.Transmission.TransmissionMap, Inputs::TransmissionTexture);

					//TransmissionFactor
					SetScalar(Inputs::TransmissionFactor, GltfMaterial.Transmission.TransmissionFactor, 0.f);

					//AlphaMode
					SetScalar(Inputs::AlphaMode, AlphaMode, EAlphaMode::Blend);
				}
			}
			else if (GltfMaterial.bHasClearCoat)
			{
				{
					//ClearCoatTexture
					//ClearCoatTexture_OffsetScale
					//ClearCoatTexture_Rotation
					//ClearCoatTexture_TexCoord
					SetMap(GltfMaterial.ClearCoat.ClearCoatMap, Inputs::ClearCoatTexture);

					//ClearCoatFactor
					SetScalar(Inputs::ClearCoatFactor, GltfMaterial.ClearCoat.ClearCoatFactor, 0.f);
				}

				{
					//ClearCoatRoughnessTexture
					//ClearCoatRoughnessTexture_OffsetScale
					//ClearCoatRoughnessTexture_Rotation
					//ClearCoatRoughnessTexture_TexCoord
					SetMap(GltfMaterial.ClearCoat.RoughnessMap, Inputs::ClearCoatRoughnessTexture);

					//ClearCoatRoughnessFactor
					SetScalar(Inputs::ClearCoatRoughnessFactor, GltfMaterial.ClearCoat.Roughness, 0.f);
				}

				{
					//ClearCoatNormalTexture
					//ClearCoatNormalTexture_OffsetScale
					//ClearCoatNormalTexture_Rotation
					//ClearCoatNormalTexture_TexCoord
					SetMap(GltfMaterial.ClearCoat.NormalMap, Inputs::ClearCoatNormalTexture);

					//ClearCoatNormalFactor
					SetScalar(Inputs::ClearCoatNormalScale, GltfMaterial.ClearCoat.NormalMapUVScale, 1.f);
				}
			}
			else if (GltfMaterial.bHasSheen)
			{
				{
					//SheenColorTexture
					//SheenColorTexture_OffsetScale
					//SheenColorTexture_Rotation
					//SheenColorTexture_TexCoord
					SetMap(GltfMaterial.Sheen.SheenColorMap, Inputs::SheenColorTexture);

					//SheenColorFactor
					FVector3f SheenColorFactor(GltfMaterial.Sheen.SheenColorFactor[0], GltfMaterial.Sheen.SheenColorFactor[1], GltfMaterial.Sheen.SheenColorFactor[2]);
					SetVec3(Inputs::SheenColorFactor, SheenColorFactor, FVector3f(0, 0, 0));

					//SheenRoughnessTexture
					//SheenRoughnessTexture_OffsetScale
					//SheenRoughnessTexture_Rotation
					//SheenRoughnessTexture_TexCoord
					SetMap(GltfMaterial.Sheen.SheenRoughnessMap, Inputs::SheenRoughnessTexture);

					//SheenRoughnessFactor
					SetScalar(Inputs::SheenRoughnessFactor, GltfMaterial.Sheen.SheenRoughnessFactor, 0.f);
				}
			}
		}
	};

	void HandleGltfMaterial(UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FMaterial& GltfMaterial, const TArray<GLTF::FTexture>& Textures, UInterchangeShaderGraphNode* ShaderGraphNode)
	{
		using namespace UE::Interchange::GLTFMaterials;

		//Constructor of FGLTFMaterialProcessor will configure and generate necessary properties for Parsing:
		FGLTFMaterialProcessor GLTFMaterailProcessor(NodeContainer, GltfMaterial, Textures, ShaderGraphNode);

		GLTFMaterailProcessor.ParseMaterialValues();
	}

	bool AreRequiredPackagesLoaded()
	{
		auto ArePackagesLoaded = [](const TArray<FString>& PackagePaths) -> bool
		{
			bool bAllLoaded = true;

			for (const FString& PackagePath : PackagePaths)
			{
				const FString ObjectPath(FPackageName::ExportTextPathToObjectPath(PackagePath));

				if (FPackageName::DoesPackageExist(ObjectPath))
				{
					if (FSoftObjectPath(ObjectPath).TryLoad())
					{
						continue;
					}
					else
					{
						UE_LOG(LogInterchangeImport, Warning, TEXT("Couldn't load %s"), *PackagePath);
					}
				}
				else
				{
					UE_LOG(LogInterchangeImport, Warning, TEXT("Couldn't find %s"), *PackagePath);
				}

				bAllLoaded = false;
			}

			return bAllLoaded;
		};

		TArray<FString> RequiredPackages = GetRequiredMaterialFunctionPaths();

		static const bool bRequiredPackagesLoaded = ArePackagesLoaded(RequiredPackages);

		return bRequiredPackagesLoaded;
	}
}

