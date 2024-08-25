// Copyright Epic Games, Inc. All Rights Reserved.

#include "FbxMaterial.h"

#include "FbxAPI.h"
#include "FbxConvert.h"
#include "FbxHelper.h"
#include "FbxInclude.h"
#include "Fbx/InterchangeFbxMessages.h"
#include "InterchangeMaterialDefinitions.h"
#include "InterchangeResultsContainer.h"
#include "InterchangeSceneNode.h"
#include "InterchangeShaderGraphNode.h"
#include "InterchangeTexture2DNode.h"
#include "InterchangeTextureNode.h"
#include "Misc/Paths.h"
#include "Nodes/InterchangeBaseNodeContainer.h"



#define LOCTEXT_NAMESPACE "InterchangeFbxMaterial"

namespace UE
{
	namespace Interchange
	{
		namespace Private
		{
			UInterchangeShaderGraphNode* FFbxMaterial::CreateShaderGraphNode(UInterchangeBaseNodeContainer& NodeContainer, const FString& NodeUid, const FString& NodeName)
			{
				UInterchangeShaderGraphNode* ShaderGraphNode = NewObject<UInterchangeShaderGraphNode>(&NodeContainer);
				ShaderGraphNode->InitializeNode(NodeUid, NodeName, EInterchangeNodeContainerType::TranslatedAsset);
				NodeContainer.AddNode(ShaderGraphNode);

				return ShaderGraphNode;
			}

			const UInterchangeTexture2DNode* FFbxMaterial::CreateTexture2DNode(UInterchangeBaseNodeContainer& NodeContainer, const FString& TextureFilePath)
			{
				if (TextureFilePath.IsEmpty())
				{
					return nullptr;
				}
				FString NormalizeFilePath = TextureFilePath;
				FPaths::NormalizeFilename(NormalizeFilePath);
				if (!FPaths::FileExists(NormalizeFilePath))
				{
					return nullptr;
				}
				const FString TextureName = FPaths::GetBaseFilename(TextureFilePath);
				const FString TextureNodeID = UInterchangeTextureNode::MakeNodeUid(TextureName);

				if (const UInterchangeTexture2DNode* TextureNode = Cast<const UInterchangeTexture2DNode>(NodeContainer.GetNode(TextureNodeID)))
				{
					return TextureNode;
				}

				UInterchangeTexture2DNode* NewTextureNode = UInterchangeTexture2DNode::Create(&NodeContainer, TextureNodeID);
				NewTextureNode->SetDisplayLabel(TextureName);

				//All texture translator expect a file as the payload key
				NewTextureNode->SetPayLoadKey(NormalizeFilePath);

				return NewTextureNode;
			}

			const UInterchangeShaderNode* FFbxMaterial::CreateTextureSampler(FbxFileTexture* FbxTexture, UInterchangeBaseNodeContainer& NodeContainer, const FString& ShaderUniqueID, const FString& InputName)
			{
				using namespace Materials::Standard::Nodes;

				if (!FbxTexture)
				{
					return nullptr;
				}

				const FString TextureFilename = FbxTexture ? UTF8_TO_TCHAR(FbxTexture->GetFileName()) : TEXT("");
				const FString NodeName = TEXT("Sampler_") + InputName;

				// Return already created node if applicable.
				const FString SamplerNodeUid = UInterchangeShaderNode::MakeNodeUid(NodeName, ShaderUniqueID);
				if (const UInterchangeShaderNode* SamplerNode = Cast<UInterchangeShaderNode>(NodeContainer.GetNode(SamplerNodeUid)))
				{
					return SamplerNode;
				}

				UInterchangeShaderNode* TextureSampleShader = UInterchangeShaderNode::Create(&NodeContainer, NodeName, ShaderUniqueID);
				TextureSampleShader->SetDisplayLabel(InputName);
				TextureSampleShader->SetCustomShaderType(TextureSample::Name.ToString());

				// Return incomplete texture sampler if texture file does not exist
				if (TextureFilename.IsEmpty() || !FPaths::FileExists(TextureFilename))
				{
					const UInterchangeBaseNode* ShaderGraphNode = NodeContainer.GetNode(ShaderUniqueID);
					if(!GIsAutomationTesting)
					{
						UInterchangeResultTextureWarning_TextureFileDoNotExist* Message = Parser.AddMessage<UInterchangeResultTextureWarning_TextureFileDoNotExist>();
						Message->TextureName = TextureFilename;
						Message->MaterialName = ShaderGraphNode ? ShaderGraphNode->GetDisplayLabel() : TEXT("Unknown");
					}

					return TextureSampleShader;
				}

				const UInterchangeTexture2DNode* TextureNode = CreateTexture2DNode(NodeContainer, TextureFilename);

				TextureSampleShader->AddStringAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TextureSample::Inputs::Texture.ToString()), TextureNode->GetUniqueID());

				if (!FMath::IsNearlyEqual(FbxTexture->GetScaleU(), 1.0) || !FMath::IsNearlyEqual(FbxTexture->GetScaleV(), 1.0))
				{
					UInterchangeShaderNode* TextureCoordinateShader = UInterchangeShaderNode::Create(&NodeContainer, InputName + TEXT("_Coordinate"), ShaderUniqueID);
					TextureCoordinateShader->SetCustomShaderType(TextureCoordinate::Name.ToString());

					TextureCoordinateShader->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TextureCoordinate::Inputs::UTiling.ToString()), (float)FbxTexture->GetScaleU());
					TextureCoordinateShader->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TextureCoordinate::Inputs::VTiling.ToString()), (float)FbxTexture->GetScaleV());

					UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(TextureSampleShader, TextureSample::Inputs::Coordinates.ToString(), TextureCoordinateShader->GetUniqueID());
				}

				return TextureSampleShader;
			}

			bool FFbxMaterial::ConvertPropertyToShaderNode(UInterchangeBaseNodeContainer& NodeContainer, UInterchangeShaderGraphNode* ShaderGraphNode, FbxProperty& Property, float Factor, FName InputName,
														   const TVariant<FLinearColor, float>& DefaultValue, bool bInverse)
			{
				using namespace Materials::Standard::Nodes;

				const int32 TextureCount = Property.GetSrcObjectCount<FbxFileTexture>();
				const EFbxType DataType = Property.GetPropertyDataType().GetType();
				FString InputToConnectTo = InputName.ToString();

				if (TextureCount == 0)
				{
					const FString InputAttributeKey = UInterchangeShaderPortsAPI::MakeInputValueKey(InputName.ToString());

					if (DataType == eFbxDouble || DataType == eFbxFloat || DataType == eFbxInt)
					{
						const float PropertyValue = Property.Get<float>() * Factor;
						ShaderGraphNode->AddFloatAttribute(InputAttributeKey, bInverse ? 1.f - PropertyValue : PropertyValue);
					}
					else if (DataType == eFbxDouble3 || DataType == eFbxDouble4)
					{
						FbxDouble3 Color = DataType == eFbxDouble3 ? Property.Get<FbxDouble3>() : Property.Get<FbxDouble4>();
						FVector3f FbxValue = FVector3f(Color[0], Color[1], Color[2]) * Factor;
						FLinearColor PropertyValue = bInverse ? FVector3f::OneVector - FbxValue : FbxValue;

						if (DefaultValue.IsType<FLinearColor>())
						{
							ShaderGraphNode->AddLinearColorAttribute(InputAttributeKey, PropertyValue);
						}
						else if (DefaultValue.IsType<float>())
						{
							// We're connecting a linear color to a float input. Ideally, we'd go through a desaturate, but for now we'll just take the red channel and ignore the rest.
							ShaderGraphNode->AddFloatAttribute(InputAttributeKey, PropertyValue.R);
						}
					}

					return true;
				}

				UInterchangeShaderNode* NodeToConnectTo = ShaderGraphNode;

				if (bInverse)
				{
					const FString OneMinusNodeName = InputName.ToString() + TEXT("OneMinus");
					UInterchangeShaderNode* OneMinusNode = UInterchangeShaderNode::Create(&NodeContainer, OneMinusNodeName, ShaderGraphNode->GetUniqueID());
					OneMinusNode->SetCustomShaderType(OneMinus::Name.ToString());

					UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ShaderGraphNode, InputName.ToString(), OneMinusNode->GetUniqueID());

					NodeToConnectTo = OneMinusNode;
					InputToConnectTo = OneMinus::Inputs::Input.ToString();
				}

				{
					FString LerpNodeName = InputName.ToString() + TEXT("Lerp");
					UInterchangeShaderNode* LerpNode = UInterchangeShaderNode::Create(&NodeContainer, LerpNodeName, ShaderGraphNode->GetUniqueID());
					LerpNode->SetCustomShaderType(Lerp::Name.ToString());

					if (DefaultValue.IsType<float>())
					{
						LerpNode->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(Lerp::Inputs::B.ToString()), DefaultValue.Get<float>());
					}
					else if (DefaultValue.IsType<FLinearColor>())
					{
						LerpNode->AddLinearColorAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(Lerp::Inputs::B.ToString()), DefaultValue.Get<FLinearColor>());
					}
					
					const FString WeightNodeName = InputName.ToString() + TEXT("MapWeight");
					UInterchangeShaderNode* WeightNode = UInterchangeShaderNode::Create(&NodeContainer, WeightNodeName, LerpNode->GetUniqueID());
					WeightNode->SetCustomShaderType(ScalarParameter::Name.ToString());

					const float InverseFactor = 1.f - Factor; // We lerp from A to B and prefer to put the strongest input in A so we need to flip the lerp factor
					WeightNode->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputParameterKey(ScalarParameter::Attributes::DefaultValue.ToString()), InverseFactor);

					UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(LerpNode, Lerp::Inputs::Factor.ToString() , WeightNode->GetUniqueID());

					UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(NodeToConnectTo, InputToConnectTo, LerpNode->GetUniqueID());

					NodeToConnectTo = LerpNode;
					InputToConnectTo = Lerp::Inputs::A.ToString();
				}

				// Handles max one texture per property.
				FbxFileTexture* FbxTexture = Property.GetSrcObject<FbxFileTexture>(0);
				if (const UInterchangeShaderNode* TextureSampleShader = CreateTextureSampler(FbxTexture, NodeContainer, ShaderGraphNode->GetUniqueID(), InputName.ToString() + TEXT("Map")))
				{
					UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(NodeToConnectTo, InputToConnectTo, TextureSampleShader->GetUniqueID());
				}
				else
				{
					if (!GIsAutomationTesting)
					{
						UInterchangeResultTextureWarning_TextureFileDoNotExist* Message = Parser.AddMessage<UInterchangeResultTextureWarning_TextureFileDoNotExist>();
						Message->TextureName = FbxTexture ? UTF8_TO_TCHAR(FbxTexture->GetFileName()) : TEXT("Undefined");
						Message->MaterialName = ShaderGraphNode->GetDisplayLabel();
					}

					return false;
				}

				return true;
			}

			void FFbxMaterial::ConvertShininessToShaderNode(FbxSurfaceMaterial& SurfaceMaterial, UInterchangeBaseNodeContainer& NodeContainer, UInterchangeShaderGraphNode* ShaderGraphNode)
			{
				using namespace UE::Interchange::Materials;
				using namespace UE::Interchange::Materials::Standard::Nodes;

				FBXSDK_NAMESPACE::FbxProperty MaterialProperty = SurfaceMaterial.FindProperty(FbxSurfaceMaterial::sShininess);

				if (!MaterialProperty.IsValid())
				{
					return;
				}

				const FString InputName = Phong::Parameters::Shininess.ToString();

				if (MaterialProperty.GetSrcObjectCount<FBXSDK_NAMESPACE::FbxTexture>() > 0)
				{
					FbxFileTexture* FbxTexture = MaterialProperty.GetSrcObject<FbxFileTexture>(0);
					if (const UInterchangeShaderNode* TextureSampleShader = CreateTextureSampler(FbxTexture, NodeContainer, ShaderGraphNode->GetUniqueID(), TEXT("ShininessMap")))
					{
						FString MultiplyNodeName = Phong::Parameters::Shininess.ToString() + TEXT("_Multiply");
						UInterchangeShaderNode* MultiplyNode = UInterchangeShaderNode::Create(&NodeContainer, MultiplyNodeName, ShaderGraphNode->GetUniqueID());
						MultiplyNode->SetCustomShaderType(Multiply::Name.ToString());
						
						UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ShaderGraphNode, InputName, MultiplyNode->GetUniqueID());

						// Scale texture output from [0-1] to [0-1000]
						MultiplyNode->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(Multiply::Inputs::B.ToString()), 1000.0f);

						UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(MultiplyNode, Multiply::Inputs::A.ToString(), TextureSampleShader->GetUniqueID());
					}
				}
				else if (!FBXSDK_NAMESPACE::FbxProperty::HasDefaultValue(MaterialProperty))
				{
					const FString InputValueKey = UInterchangeShaderPortsAPI::MakeInputValueKey(InputName);
					ShaderGraphNode->AddFloatAttribute(InputValueKey, MaterialProperty.Get<float>());
				}
			}

			const UInterchangeShaderGraphNode* FFbxMaterial::AddShaderGraphNode(FbxSurfaceMaterial* SurfaceMaterial, UInterchangeBaseNodeContainer& NodeContainer)
			{
				using namespace UE::Interchange::Materials;

				if (!SurfaceMaterial)
				{
					return nullptr;
				}

				//Create a material node
				FString MaterialName = Parser.GetFbxHelper()->GetFbxObjectName(SurfaceMaterial);
				FString NodeUid = TEXT("\\Material\\") + MaterialName;
				const UInterchangeShaderGraphNode* ExistingShaderGraphNode = Cast<const UInterchangeShaderGraphNode>(NodeContainer.GetNode(NodeUid));
				if (ExistingShaderGraphNode)
				{
					return ExistingShaderGraphNode;
				}


				UInterchangeShaderGraphNode* ShaderGraphNode = CreateShaderGraphNode(NodeContainer, NodeUid, MaterialName);
				if (ShaderGraphNode == nullptr)
				{
					FFormatNamedArguments Args
					{
						{ TEXT("MaterialName"), FText::FromString(MaterialName) }
					};
					UInterchangeResultError_Generic* Message = Parser.AddMessage<UInterchangeResultError_Generic>();
					Message->Text = FText::Format(LOCTEXT("CannotCreateFBXMaterial", "Cannot create FBX material '{MaterialName}'."), Args);
					return nullptr;
				}

				TFunction<bool(FBXSDK_NAMESPACE::FbxProperty&)> ShouldConvertProperty = [&](FBXSDK_NAMESPACE::FbxProperty& MaterialProperty) -> bool
				{
					bool bShouldConvertProperty = false;

					if (MaterialProperty.IsValid())
					{
						// FbxProperty::HasDefaultValue(..) can return true while the property has textures attached to it.
						bShouldConvertProperty = MaterialProperty.GetSrcObjectCount<FBXSDK_NAMESPACE::FbxTexture>() > 0
							|| !FBXSDK_NAMESPACE::FbxProperty::HasDefaultValue(MaterialProperty);
					}

					return bShouldConvertProperty;
				};

				TFunction<float(const char*)> GetFactor = [&](const char* FactorName)
				{
					FBXSDK_NAMESPACE::FbxProperty Property = SurfaceMaterial->FindProperty(FactorName);
					return Property.IsValid() ? (float)Property.Get<FbxDouble>() : 1.;
				};

				TFunction<bool(FName, const char*, float , TVariant<FLinearColor, float>&, bool)>  ConnectInput;
				ConnectInput = [&](FName InputName, const char* FbxPropertyName, float Factor, TVariant<FLinearColor, float>& DefaultValue, bool bInverse) -> bool
				{
					FBXSDK_NAMESPACE::FbxProperty MaterialProperty = SurfaceMaterial->FindProperty(FbxPropertyName);
					if (ShouldConvertProperty(MaterialProperty))
					{
						return ConvertPropertyToShaderNode(NodeContainer, ShaderGraphNode, MaterialProperty, Factor, InputName, DefaultValue, bInverse);
					}

					return false;
				};

				// Diffuse
				{
					const float Factor = GetFactor(FbxSurfaceMaterial::sDiffuseFactor);
					TVariant<FLinearColor, float> DefaultValue;
					DefaultValue.Set<FLinearColor>(FLinearColor::Black);
					ConnectInput(Phong::Parameters::DiffuseColor, FbxSurfaceMaterial::sDiffuse, Factor, DefaultValue, false);
				}

				// Ambient
				{
					const float Factor = GetFactor(FbxSurfaceMaterial::sAmbientFactor);
					TVariant<FLinearColor, float> DefaultValue;
					DefaultValue.Set<FLinearColor>(FLinearColor::Black);
					ConnectInput(Phong::Parameters::AmbientColor, FbxSurfaceMaterial::sAmbient, Factor, DefaultValue, false);
				}

				// Emissive
				{
					const float Factor = GetFactor(FbxSurfaceMaterial::sEmissiveFactor);
					TVariant<FLinearColor, float> DefaultValue;
					DefaultValue.Set<FLinearColor>(FLinearColor::Black);
					ConnectInput(Phong::Parameters::EmissiveColor, FbxSurfaceMaterial::sEmissive, Factor, DefaultValue, false);
				}

				// Normal
				{
					// FbxSurfaceMaterial can have either a normal map or a bump map, check for both
					FBXSDK_NAMESPACE::FbxProperty MaterialProperty = SurfaceMaterial->FindProperty(FbxSurfaceMaterial::sNormalMap);
					if (MaterialProperty.IsValid() && MaterialProperty.GetSrcObjectCount<FbxTexture>() > 0)
					{
						TVariant<FLinearColor, float> DefaultValue;
						DefaultValue.Set<FLinearColor>(FLinearColor(FVector::UpVector));
						ConvertPropertyToShaderNode(NodeContainer, ShaderGraphNode, MaterialProperty, 1.f, Phong::Parameters::Normal, DefaultValue);
					}
					else
					{
						const float Factor = GetFactor(FbxSurfaceMaterial::sBumpFactor);
						MaterialProperty = SurfaceMaterial->FindProperty(FbxSurfaceMaterial::sBump);
						if (MaterialProperty.IsValid() && MaterialProperty.GetSrcObjectCount<FbxTexture>() > 0)
						{
							using namespace Materials::Standard::Nodes;

							if (FbxFileTexture* FbxTexture = MaterialProperty.GetSrcObject<FbxFileTexture>(0))
							{
								const FString TexturePath = FbxTexture->GetFileName();
								if (!TexturePath.IsEmpty() && FPaths::FileExists(TexturePath))
								{
									if (const UInterchangeTexture2DNode* TextureNode = CreateTexture2DNode(NodeContainer, TexturePath))
									{
										const FString TextureName = FPaths::GetBaseFilename(TexturePath);

										// NormalFromHeightmap needs TextureObject(not just a sample as it takes multiple samples from it)
										UInterchangeShaderNode* TextureObjectNode = UInterchangeShaderNode::Create(&NodeContainer, TEXT("NormalMap"), ShaderGraphNode->GetUniqueID());
										TextureObjectNode->SetDisplayLabel(TEXT("NormalMap"));
										TextureObjectNode->SetCustomShaderType(TextureObject::Name.ToString());
										TextureObjectNode->AddStringAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TextureObject::Inputs::Texture.ToString()), TextureNode->GetUniqueID());

										UInterchangeShaderNode* HeightMapNode = UInterchangeShaderNode::Create(&NodeContainer, NormalFromHeightMap::Name.ToString(), ShaderGraphNode->GetUniqueID());
										HeightMapNode->SetCustomShaderType(NormalFromHeightMap::Name.ToString());

										UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(HeightMapNode, NormalFromHeightMap::Inputs::HeightMap.ToString(), TextureObjectNode->GetUniqueID());
										HeightMapNode->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(NormalFromHeightMap::Inputs::Intensity.ToString()), Factor);

										UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ShaderGraphNode, Materials::Common::Parameters::Normal.ToString(), HeightMapNode->GetUniqueID());
									}
								}
							}
						}
					}
				}

				// Opacity
				// Connect only if transparency is either a texture or different from 0.f
				{
					FBXSDK_NAMESPACE::FbxProperty MaterialProperty = SurfaceMaterial->FindProperty(FbxSurfaceMaterial::sTransparentColor);
					if (ShouldConvertProperty(MaterialProperty))
					{
						const float Factor = GetFactor(FbxSurfaceMaterial::sTransparencyFactor);

						if (MaterialProperty.GetSrcObjectCount<FbxFileTexture>() > 0)
						{
							TVariant<FLinearColor, float> DefaultValue;
							DefaultValue.Set<float>(0.f); // Opaque
							FName InputName = Phong::Parameters::Opacity;
							// The texture is hooked to the OpacityMask when transparency is with a texture
							if (MaterialProperty.Get<FbxDouble>() == 0.)
							{
								InputName = Phong::Parameters::OpacityMask;
								ShaderGraphNode->SetCustomOpacityMaskClipValue(0.333f);
							}
							ConvertPropertyToShaderNode(NodeContainer, ShaderGraphNode, MaterialProperty, Factor, InputName, DefaultValue, true);
						}
						else
						{
							const float OpacityScalar = 1.f - (MaterialProperty.Get<float>() * Factor);
							if (OpacityScalar < 1.f)
							{
								const FString InputName = UInterchangeShaderPortsAPI::MakeInputValueKey(Phong::Parameters::Opacity.ToString());
								ShaderGraphNode->AddFloatAttribute(InputName, OpacityScalar);
							}
						}
					}
				}

				// Specular
				{
					const float Factor = GetFactor(FbxSurfaceMaterial::sSpecularFactor);
					TVariant<FLinearColor, float> DefaultValue;
					DefaultValue.Set<FLinearColor>(FLinearColor::Black);
					ConnectInput(Phong::Parameters::SpecularColor, FbxSurfaceMaterial::sSpecular, Factor, DefaultValue, false);
				}

				// Shininess
				ConvertShininessToShaderNode(*SurfaceMaterial, NodeContainer, ShaderGraphNode);

				// If no valid property found, create a material anyway
				// Use random color because there may be multiple materials, then they can be different
				TArray<FString> InputNames;
				UInterchangeShaderPortsAPI::GatherInputs(ShaderGraphNode, InputNames);
				if (InputNames.Num() == 0)
				{
					FLinearColor BaseColor;
					BaseColor.R = 0.5f + 0.5f * FMath::FRand();
					BaseColor.G = 0.5f + 0.5f * FMath::FRand();
					BaseColor.B = 0.5f + 0.5f * FMath::FRand();

					const FString InputValueKey = UInterchangeShaderPortsAPI::MakeInputValueKey(Phong::Parameters::DiffuseColor.ToString());
					ShaderGraphNode->AddLinearColorAttribute(InputValueKey, BaseColor);
				}

				return ShaderGraphNode;
			}

			void FFbxMaterial::AddAllTextures(FbxScene* SDKScene, UInterchangeBaseNodeContainer& NodeContainer)
			{
				int32 TextureCount = SDKScene->GetSrcObjectCount<FbxFileTexture>();
				for (int32 TextureIndex = 0; TextureIndex < TextureCount; ++TextureIndex)
				{
					FbxFileTexture* Texture = SDKScene->GetSrcObject<FbxFileTexture>(TextureIndex);
					FString TextureFilename = UTF8_TO_TCHAR(Texture->GetFileName());
					//Only import texture that exist on disk
					if (!FPaths::FileExists(TextureFilename))
					{
						if (!GIsAutomationTesting)
						{
							UInterchangeResultTextureWarning_TextureFileDoNotExist* Message = Parser.AddMessage<UInterchangeResultTextureWarning_TextureFileDoNotExist>();
							Message->TextureName = TextureFilename;
							Message->MaterialName.Empty();
						}
						continue;
					}
					//Create a texture node and make it child of the material node
					const FString TextureName = FPaths::GetBaseFilename(TextureFilename);
					const UInterchangeTexture2DNode* TextureNode = Cast<UInterchangeTexture2DNode>(NodeContainer.GetNode(UInterchangeTextureNode::MakeNodeUid(TextureName)));
					if (!TextureNode)
					{
						CreateTexture2DNode(NodeContainer, TextureFilename);
					}
				}
			}
			
			void FFbxMaterial::AddAllNodeMaterials(UInterchangeSceneNode* SceneNode, FbxNode* ParentFbxNode, UInterchangeBaseNodeContainer& NodeContainer)
			{
				int32 MaterialCount = ParentFbxNode->GetMaterialCount();
				TMap<FbxSurfaceMaterial*, int32> UniqueSlotNames;
				UniqueSlotNames.Reserve(MaterialCount);
				for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
				{
					if (FbxSurfaceMaterial* SurfaceMaterial = ParentFbxNode->GetMaterial(MaterialIndex))
					{
						const UInterchangeShaderGraphNode* ShaderGraphNode = AddShaderGraphNode(SurfaceMaterial, NodeContainer);
						
						int32& SlotMaterialCount = UniqueSlotNames.FindOrAdd(SurfaceMaterial);
						FString MaterialSlotName = Parser.GetFbxHelper()->GetFbxObjectName(SurfaceMaterial);
						if (SlotMaterialCount > 0)
						{
							MaterialSlotName += TEXT("_Section") + FString::FromInt(SlotMaterialCount);
						}
						SceneNode->SetSlotMaterialDependencyUid(MaterialSlotName, ShaderGraphNode->GetUniqueID());
						SlotMaterialCount++;
					}
				}
			}

			void FFbxMaterial::AddAllMaterials(FbxScene* SDKScene, UInterchangeBaseNodeContainer& NodeContainer)
			{
				int32 MaterialCount = SDKScene->GetMaterialCount();
				for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
				{
					if (FbxSurfaceMaterial* SurfaceMaterial = SDKScene->GetMaterial(MaterialIndex))
					{
						AddShaderGraphNode(SurfaceMaterial, NodeContainer);
					}
				}
			}
		} //ns Private
	} //ns Interchange
}//ns UE

#undef LOCTEXT_NAMESPACE
