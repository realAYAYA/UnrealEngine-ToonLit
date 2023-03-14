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

			UInterchangeTexture2DNode* FFbxMaterial::CreateTexture2DNode(UInterchangeBaseNodeContainer& NodeContainer, const FString& TextureFilePath)
			{
				const FString TextureName = FPaths::GetBaseFilename(TextureFilePath);
				UInterchangeTexture2DNode* TextureNode = UInterchangeTexture2DNode::Create(&NodeContainer, TextureName);

				//All texture translator expect a file as the payload key
				FString NormalizeFilePath = TextureFilePath;
				FPaths::NormalizeFilename(NormalizeFilePath);
				TextureNode->SetPayLoadKey(NormalizeFilePath);

				return TextureNode;
			}

			void FFbxMaterial::ConvertPropertyToShaderNode(UInterchangeBaseNodeContainer& NodeContainer, UInterchangeShaderGraphNode* ShaderGraphNode, FbxProperty& Property, float Factor, FName InputName,
				TVariant<FLinearColor, float> DefaultValue, bool bInverse)
			{
				using namespace Materials::Standard::Nodes;

				UInterchangeShaderNode* NodeToConnectTo = ShaderGraphNode;
				FString InputToConnectTo = InputName.ToString();

				const int32 TextureCount = Property.GetSrcObjectCount<FbxFileTexture>();
				const FbxDataType DataType = Property.GetPropertyDataType();

				if (bInverse)
				{
					const FString OneMinusNodeName = InputName.ToString() + TEXT("OneMinus");
					UInterchangeShaderNode* OneMinusNode = UInterchangeShaderNode::Create(&NodeContainer, OneMinusNodeName, ShaderGraphNode->GetUniqueID());
					OneMinusNode->SetCustomShaderType(OneMinus::Name.ToString());

					UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ShaderGraphNode, InputName.ToString(), OneMinusNode->GetUniqueID());

					NodeToConnectTo = OneMinusNode;
					InputToConnectTo = OneMinus::Inputs::Input.ToString();
				}

				if (!FMath::IsNearlyEqual(Factor, 1.f))
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

					const float InverseFactor = 1.f - Factor; // We lerp from A to B and prefer to put the strongest input in A so we need to flip the lerp factor
					LerpNode->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(Lerp::Inputs::Factor.ToString()), InverseFactor);

					UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(NodeToConnectTo, InputToConnectTo, LerpNode->GetUniqueID());

					NodeToConnectTo = LerpNode;
					InputToConnectTo = Lerp::Inputs::A.ToString();
				}

				// Handles max one texture per property.
				if (TextureCount > 0)
				{
					FbxFileTexture* FbxTexture = Property.GetSrcObject<FbxFileTexture>(0);
					FString TextureFilename = UTF8_TO_TCHAR(FbxTexture->GetFileName());
					//Only import texture that exist on disk
					if(!TextureFilename.IsEmpty() && FPaths::FileExists(TextureFilename))
					{
						FString TextureName = FPaths::GetBaseFilename(TextureFilename);
						UInterchangeShaderNode* TextureSampleShader = UInterchangeShaderNode::Create(&NodeContainer, TextureName, ShaderGraphNode->GetUniqueID());
						TextureSampleShader->SetCustomShaderType(TextureSample::Name.ToString());

						const UInterchangeTexture2DNode* TextureNode = Cast<const UInterchangeTexture2DNode>(NodeContainer.GetNode(UInterchangeTextureNode::MakeNodeUid(TextureName)));

						if (!TextureNode)
						{
							TextureNode = CreateTexture2DNode(NodeContainer, TextureFilename);
						}

						TextureSampleShader->AddStringAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TextureSample::Inputs::Texture.ToString()), TextureNode->GetUniqueID());

						if (!FMath::IsNearlyEqual(FbxTexture->GetScaleU(), 1.0) || !FMath::IsNearlyEqual(FbxTexture->GetScaleV(), 1.0))
						{
							UInterchangeShaderNode* TextureCoordinateShader = UInterchangeShaderNode::Create(&NodeContainer, TextureName + TEXT("_Coordinate"), ShaderGraphNode->GetUniqueID());
							TextureCoordinateShader->SetCustomShaderType(TextureCoordinate::Name.ToString());

							TextureCoordinateShader->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TextureCoordinate::Inputs::UTiling.ToString()), (float)FbxTexture->GetScaleU());
							TextureCoordinateShader->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TextureCoordinate::Inputs::VTiling.ToString()), (float)FbxTexture->GetScaleV());

							UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(TextureSampleShader, TextureSample::Inputs::Coordinates.ToString(), TextureCoordinateShader->GetUniqueID());
						}

						UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(NodeToConnectTo, InputToConnectTo, TextureSampleShader->GetUniqueID());
					}
					else
					{
						UInterchangeResultTextureWarning_TextureFileDoNotExist* Message = Parser.AddMessage<UInterchangeResultTextureWarning_TextureFileDoNotExist>();
						Message->TextureName = TextureFilename;
						Message->MaterialName = ShaderGraphNode->GetDisplayLabel();
					}
				}
				else if (DataType.GetType() == eFbxDouble || DataType.GetType() == eFbxFloat || DataType.GetType() == eFbxInt)
				{
					NodeToConnectTo->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(InputToConnectTo), Property.Get<float>());
				}
				else if (DataType.GetType() == eFbxDouble3)
				{
					const FLinearColor PropertyValue = FFbxConvert::ConvertColor(Property.Get<FbxDouble3>());

					if (DefaultValue.IsType<FLinearColor>())
					{
						NodeToConnectTo->AddLinearColorAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(InputToConnectTo), PropertyValue);
					}
					else if (DefaultValue.IsType<float>())
					{
						// We're connecting a linear color to a float input. Ideally, we'd go through a desaturate, but for now we'll just take the red channel and ignore the rest.
						NodeToConnectTo->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(InputToConnectTo), PropertyValue.R);
					}
				}
			}

			const UInterchangeShaderGraphNode* FFbxMaterial::AddShaderGraphNode(FbxSurfaceMaterial* SurfaceMaterial, UInterchangeBaseNodeContainer& NodeContainer)
			{
				using namespace UE::Interchange::Materials;

				//Create a material node
				FString MaterialName = FFbxHelper::GetFbxObjectName(SurfaceMaterial);
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

				// for each material property, add an input value or a shader node
				FbxProperty CurrentProperty = SurfaceMaterial->GetFirstProperty();
				while (CurrentProperty.IsValid())
				{
					if (CurrentProperty.Modified())
					{
						const FName PropertyName(CurrentProperty.GetNameAsCStr());

						// Convert FBX property names to standard Interchange Phong parameter names when possible
						{
							if (PropertyName == FbxSurfaceMaterial::sDiffuse && SurfaceMaterial->Is<FbxSurfaceLambert>())
							{
								const float Factor = (float)((FbxSurfaceLambert*)SurfaceMaterial)->DiffuseFactor.Get();
								TVariant<FLinearColor, float> DefaultValue;
								DefaultValue.Set<FLinearColor>(FLinearColor::Black);
								ConvertPropertyToShaderNode(NodeContainer, ShaderGraphNode, CurrentProperty, Factor, Phong::Parameters::DiffuseColor, DefaultValue);
							}
							else if (PropertyName == FbxSurfaceMaterial::sSpecular && SurfaceMaterial->Is<FbxSurfacePhong>())
							{
								const float Factor = (float)((FbxSurfacePhong*)SurfaceMaterial)->SpecularFactor.Get();
								TVariant<FLinearColor, float> DefaultValue;
								DefaultValue.Set<FLinearColor>(FLinearColor::Black);
								ConvertPropertyToShaderNode(NodeContainer, ShaderGraphNode, CurrentProperty, Factor, Phong::Parameters::SpecularColor, DefaultValue);
							}
							else if (PropertyName == FbxSurfaceMaterial::sShininess)
							{
								const float Factor = 1.f;
								TVariant<FLinearColor, float> DefaultValue;
								DefaultValue.Set<float>(0.2f);
								ConvertPropertyToShaderNode(NodeContainer, ShaderGraphNode, CurrentProperty, Factor, Phong::Parameters::Shininess, DefaultValue);
							}
							else if (PropertyName == FbxSurfaceMaterial::sEmissive && SurfaceMaterial->Is<FbxSurfaceLambert>())
							{
								const float Factor = (float)((FbxSurfaceLambert*)SurfaceMaterial)->EmissiveFactor.Get();
								TVariant<FLinearColor, float> DefaultValue;
								DefaultValue.Set<FLinearColor>(FLinearColor::Black);
								ConvertPropertyToShaderNode(NodeContainer, ShaderGraphNode, CurrentProperty, Factor, Phong::Parameters::EmissiveColor, DefaultValue);
							}
							else if (PropertyName == FbxSurfaceMaterial::sNormalMap)
							{
								const float Factor = 1.f;
								TVariant<FLinearColor, float> DefaultValue;
								DefaultValue.Set<FLinearColor>(FLinearColor(FVector::UpVector));
								ConvertPropertyToShaderNode(NodeContainer, ShaderGraphNode, CurrentProperty, Factor, Phong::Parameters::Normal, DefaultValue);
							}
							else if (PropertyName == FbxSurfaceMaterial::sTransparentColor && SurfaceMaterial->Is<FbxSurfaceLambert>())
							{
								const float TransparencyFactor = (float)((FbxSurfaceLambert*)SurfaceMaterial)->TransparencyFactor.Get();
								TVariant<FLinearColor, float> DefaultValue;
								DefaultValue.Set<float>(0.f); // Opaque
								const bool bInverse = true;
								ConvertPropertyToShaderNode(NodeContainer, ShaderGraphNode, CurrentProperty, TransparencyFactor, Phong::Parameters::Opacity, DefaultValue, bInverse);
							}
							else if (PropertyName == FbxSurfaceMaterial::sDiffuseFactor || PropertyName == FbxSurfaceMaterial::sSpecularFactor ||
										PropertyName == FbxSurfaceMaterial::sEmissiveFactor || PropertyName == FbxSurfaceMaterial::sTransparencyFactor)
							{
								// Skip factors as they were processed with their corresponding inputs
							}
							else
							{
								if (!UInterchangeShaderPortsAPI::HasInput(ShaderGraphNode, PropertyName))
								{
									const float DummyFactor = 1.f;
									TVariant<FLinearColor, float> DummyDefaultValue;
									DummyDefaultValue.Set<float>(1.f); // Will be ignored because we have a factor of 1
									ConvertPropertyToShaderNode(NodeContainer, ShaderGraphNode, CurrentProperty, DummyFactor, PropertyName, DummyDefaultValue);
								}
							}
						}
					}

					CurrentProperty = SurfaceMaterial->GetNextProperty(CurrentProperty);
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
				for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
				{
					FbxSurfaceMaterial* SurfaceMaterial = ParentFbxNode->GetMaterial(MaterialIndex);
					const UInterchangeShaderGraphNode* ShaderGraphNode = AddShaderGraphNode(SurfaceMaterial, NodeContainer);
					SceneNode->SetSlotMaterialDependencyUid(FFbxHelper::GetFbxObjectName(SurfaceMaterial), ShaderGraphNode->GetUniqueID());
				}
			}

			void FFbxMaterial::AddAllMaterials(FbxScene* SDKScene, UInterchangeBaseNodeContainer& NodeContainer)
			{
				int32 MaterialCount = SDKScene->GetMaterialCount();
				for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
				{
					FbxSurfaceMaterial* SurfaceMaterial = SDKScene->GetMaterial(MaterialIndex);
					AddShaderGraphNode(SurfaceMaterial, NodeContainer);
				}
			}
		} //ns Private
	} //ns Interchange
}//ns UE

#undef LOCTEXT_NAMESPACE
