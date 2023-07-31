// Copyright Epic Games, Inc. All Rights Reserved. 

#include "Gltf/InterchangeGltfTranslator.h"

#include "GLTFAsset.h"
#include "GLTFMeshFactory.h"
#include "GLTFReader.h"

#include "InterchangeAnimationTrackSetNode.h"
#include "InterchangeCameraNode.h"
#include "InterchangeImportLog.h"
#include "InterchangeLightNode.h"
#include "InterchangeManager.h"
#include "InterchangeMaterialDefinitions.h"
#include "InterchangeMeshNode.h"
#include "InterchangeSceneNode.h"
#include "InterchangeShaderGraphNode.h"
#include "InterchangeTexture2DNode.h"
#include "InterchangeVariantSetNode.h"
#include "Nodes/InterchangeSourceNode.h"

#include "Sections/MovieScene3DTransformSection.h"
#include "Texture/InterchangeImageWrapperTranslator.h"

#include "Algo/Find.h"
#include "Async/Async.h"
#include "Misc/App.h"
#include "StaticMeshAttributes.h"
#include "SkeletalMeshAttributes.h"
#include "UObject/GCObjectScopeGuard.h"

#include "StaticMeshOperations.h"
#include "SkeletalMeshOperations.h"

#include "Gltf/InterchangeGltfPrivate.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeGltfTranslator)

#define LOCTEXT_NAMESPACE "InterchangeGltfTranslator"

static bool GInterchangeEnableGLTFImport = true;
static FAutoConsoleVariableRef CCvarInterchangeEnableGLTFImport(
	TEXT("Interchange.FeatureFlags.Import.GLTF"),
	GInterchangeEnableGLTFImport,
	TEXT("Whether glTF support is enabled."),
	ECVF_Default);

static const TArray<FString> SupportedExtensions = {/* Lights */
													TEXT("KHR_lights_punctual"),
													TEXT("KHR_lights"),
													/* Variants */
													TEXT("KHR_materials_variants"), 
													/* Materials */
													TEXT("KHR_materials_unlit"),
													TEXT("KHR_materials_ior"),
													TEXT("KHR_materials_clearcoat"),
													TEXT("KHR_materials_transmission"),
													TEXT("KHR_materials_sheen"),
													TEXT("KHR_materials_specular"),
													TEXT("KHR_materials_pbrSpecularGlossiness"),
													TEXT("MSFT_packing_occlusionRoughnessMetallic"),
													TEXT("MSFT_packing_normalRoughnessMetallic"),
													/* Textures */
													TEXT("KHR_texture_transform")
};

namespace UE::Interchange::Gltf::Private
{
	EInterchangeTextureWrapMode ConvertWrap(GLTF::FSampler::EWrap Wrap)
	{
		switch (Wrap)
		{
		case GLTF::FSampler::EWrap::Repeat:
			return EInterchangeTextureWrapMode::Wrap;
		case GLTF::FSampler::EWrap::MirroredRepeat:
			return EInterchangeTextureWrapMode::Mirror;
		case GLTF::FSampler::EWrap::ClampToEdge:
			return EInterchangeTextureWrapMode::Clamp;

		default:
			return EInterchangeTextureWrapMode::Wrap;
		}
	}

	EInterchangeTextureFilterMode ConvertFilter(GLTF::FSampler::EFilter Filter)
	{
		switch (Filter)
		{
			case GLTF::FSampler::EFilter::Nearest:
				return EInterchangeTextureFilterMode::Nearest;
			case GLTF::FSampler::EFilter::LinearMipmapNearest:
				return EInterchangeTextureFilterMode::Bilinear;
			case GLTF::FSampler::EFilter::LinearMipmapLinear:
				return EInterchangeTextureFilterMode::Trilinear;
				// Other glTF filter values have no direct correlation to Unreal
			default:
				return EInterchangeTextureFilterMode::Default;
		}
	}

	bool CheckForVariants(const GLTF::FMesh& Mesh, int32 VariantCount, int32 MaterialCount)
	{
		for (const GLTF::FPrimitive& Primitive : Mesh.Primitives)
		{
			for (const GLTF::FVariantMapping& VariantMapping : Primitive.VariantMappings)
			{
				if (FMath::IsWithin(VariantMapping.MaterialIndex, 0, MaterialCount))
				{
					for (int32 VariantIndex : VariantMapping.VariantIndices)
					{
						if (FMath::IsWithin(VariantIndex, 0, VariantCount))
						{
							return true;
						}
					}
				}
			}
		}

		return false;
	}

	void ScaleNodeTranslations(TArray<GLTF::FNode>& Nodes, float Scale)
	{
		for (GLTF::FNode& Node : Nodes)
		{
			Node.Transform.SetTranslation(Node.Transform.GetTranslation() * Scale);
		}
	}

	//Temporary Solution specifically for 5.1.1
	// As we cannot modify public headers (because of binary compatibility ) all necessary changes for gltf object uniqueness have been moved into here.
	// Appropriate/fully fledged solution will be addressed for 5.2
	struct GLTFUniqueIdGenerator
	{
		FString Prefix;
		
		// Node To UniqueId maps
		TMap<const GLTF::FAnimation*, FString> AnimationToUniqueId;
		TMap<const GLTF::FScene*, FString> SceneToUniqueId;
		TMap<const GLTF::FMaterial*, FString> MaterialToUniqueId;
		TMap<const GLTF::FMesh*, FString> MeshToUniqueId;
		TMap<const GLTF::FNode*, FString> NodeToUniqueId;
		TMap<const GLTF::FCamera*, FString> CameraToUniqueId;
		TMap<const GLTF::FLight*, FString> LightToUniqueId;
		TMap<const GLTF::FSkinInfo*, FString> SkinToUniqueId;
		TMap<const GLTF::FTexture*, FString> TextureToUniqueId;
		TMap<const GLTF::FImage*, FString> ImageToUniqueId;

		void Clear()
		{
			Prefix = "";

			AnimationToUniqueId.Empty();
			SceneToUniqueId.Empty();
			MaterialToUniqueId.Empty();
			MeshToUniqueId.Empty();
			NodeToUniqueId.Empty();
			CameraToUniqueId.Empty();
			LightToUniqueId.Empty();
			SkinToUniqueId.Empty();
			TextureToUniqueId.Empty();
			ImageToUniqueId.Empty();
		}

		template <typename T>
		FString GetUniqueId(T Object, TMap<T, FString>& ObjectToUniqueId, const FString& ObjectPrefix)
		{
			if (ObjectToUniqueId.Contains(Object))
			{
				return ObjectToUniqueId[Object];
			}
			else
			{
				FString UniqueIdCandidate = Prefix + ObjectPrefix + FString::FromInt(ObjectToUniqueId.Num());
				ObjectToUniqueId.Add(Object, UniqueIdCandidate);
				return UniqueIdCandidate;
			}
		}

		FString GetUniqueId(const GLTF::FAnimation* Object) { return GetUniqueId<const GLTF::FAnimation*>(Object, AnimationToUniqueId, "_animation_"); }
		FString GetUniqueId(const GLTF::FScene* Object) { return GetUniqueId<const GLTF::FScene*>(Object, SceneToUniqueId, "_scene_"); }
		FString GetUniqueId(const GLTF::FMaterial* Object) { return GetUniqueId<const GLTF::FMaterial*>(Object, MaterialToUniqueId, "_material_"); }
		FString GetUniqueId(const GLTF::FMesh* Object) { return GetUniqueId<const GLTF::FMesh*>(Object, MeshToUniqueId, "_mesh_"); }
		FString GetUniqueId(const GLTF::FNode* Object) { return GetUniqueId<const GLTF::FNode*>(Object, NodeToUniqueId, "_node_"); }
		FString GetUniqueId(const GLTF::FCamera* Object) { return GetUniqueId<const GLTF::FCamera*>(Object, CameraToUniqueId, "_camera_"); }
		FString GetUniqueId(const GLTF::FLight* Object) { return GetUniqueId<const GLTF::FLight*>(Object, LightToUniqueId, "_light_"); }
		FString GetUniqueId(const GLTF::FSkinInfo* Object) { return GetUniqueId<const GLTF::FSkinInfo*>(Object, SkinToUniqueId, "_skin_"); }
		FString GetUniqueId(const GLTF::FTexture* Object) { return GetUniqueId<const GLTF::FTexture*>(Object, TextureToUniqueId, "_texture_"); }
		FString GetUniqueId(const GLTF::FImage* Object) { return GetUniqueId<const GLTF::FImage*>(Object, ImageToUniqueId, "_image_"); }
	};

	static TMap<const UInterchangeSourceData*, GLTFUniqueIdGenerator> GltfUniqueIdGenerators;
	static TMap<const UInterchangeSourceData*, int> GltfUniqueIdGeneratorsRefCounter;
}

void UInterchangeGltfTranslator::HandleGltfNode( UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FNode& GltfNode, const FString& ParentNodeUid, const int32 NodeIndex, 
	bool &bHasVariants, TArray<int32>& SkinnedMeshNodes, TSet<int>& UnusedMeshIndices ) const
{
	

	using namespace UE::Interchange::Gltf::Private;

	const FString NodeUid = ParentNodeUid + TEXT("\\") + GltfUniqueIdGenerators[GetSourceData()].GetUniqueId(&GltfNode);

	const UInterchangeSceneNode* ParentSceneNode = Cast< UInterchangeSceneNode >( NodeContainer.GetNode( ParentNodeUid ) );

	UInterchangeSceneNode* InterchangeSceneNode = NewObject< UInterchangeSceneNode >( &NodeContainer );
	InterchangeSceneNode->InitializeNode( NodeUid, GltfNode.Name, EInterchangeNodeContainerType::TranslatedScene );
	NodeContainer.AddNode( InterchangeSceneNode );

	NodeUidMap.Add( &GltfNode, NodeUid );

	FTransform Transform = GltfNode.Transform;

	Transform.SetTranslation( Transform.GetTranslation());

	switch ( GltfNode.Type )
	{
		case GLTF::FNode::EType::MeshSkinned:
		{
			SkinnedMeshNodes.Add(NodeIndex);

			if (!bHasVariants && GltfAsset.Variants.Num() > 0)
			{
				bHasVariants |= CheckForVariants(GltfAsset.Meshes[GltfNode.MeshIndex], GltfAsset.Variants.Num(), GltfAsset.Materials.Num());
			}
			break;
		}

		case GLTF::FNode::EType::Joint:
		{
			InterchangeSceneNode->AddSpecializedType(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString());
			break;
		}

		case GLTF::FNode::EType::Mesh:
		{
			if ( GltfAsset.Meshes.IsValidIndex( GltfNode.MeshIndex ) )
			{
				HandleGltfMesh(NodeContainer, GltfAsset.Meshes[GltfNode.MeshIndex], GltfNode.MeshIndex, UnusedMeshIndices);

				const FString MeshNodeUid = TEXT("\\Mesh\\") + GltfUniqueIdGenerators[GetSourceData()].GetUniqueId(&GltfAsset.Meshes[ GltfNode.MeshIndex ]);
				InterchangeSceneNode->SetCustomAssetInstanceUid( MeshNodeUid );

				if (!bHasVariants && GltfAsset.Variants.Num() > 0)
				{
					bHasVariants |= CheckForVariants(GltfAsset.Meshes[ GltfNode.MeshIndex ], GltfAsset.Variants.Num(), GltfAsset.Materials.Num());
				}
			}
			break;
		}

		case GLTF::FNode::EType::Camera:
		{
			Transform.ConcatenateRotation(FRotator(0, -90, 0).Quaternion());

			if ( GltfAsset.Cameras.IsValidIndex( GltfNode.CameraIndex ) )
			{
				const FString CameraNodeUid = TEXT("\\Camera\\") + GltfUniqueIdGenerators[GetSourceData()].GetUniqueId(&GltfAsset.Cameras[ GltfNode.CameraIndex ]);
				InterchangeSceneNode->SetCustomAssetInstanceUid( CameraNodeUid );
			}
			break;
		}

		case GLTF::FNode::EType::Light:
		{
			Transform.ConcatenateRotation(FRotator(0, -90, 0).Quaternion());

			if ( GltfAsset.Lights.IsValidIndex( GltfNode.LightIndex ) )
			{
				const FString LightNodeUid = TEXT("\\Light\\") + GltfUniqueIdGenerators[GetSourceData()].GetUniqueId(&GltfAsset.Lights[ GltfNode.LightIndex ]);
				InterchangeSceneNode->SetCustomAssetInstanceUid( LightNodeUid );
			}
		}

		case GLTF::FNode::EType::Transform:
		default:
		{
			break;
		}
	}

	constexpr bool bResetCache = false;

	InterchangeSceneNode->SetCustomLocalTransform(&NodeContainer, Transform, bResetCache);

	if ( !ParentNodeUid.IsEmpty() )
	{
		NodeContainer.SetNodeParentUid( NodeUid, ParentNodeUid );
	}

	for ( const int32 ChildIndex : GltfNode.Children )
	{
		if ( GltfAsset.Nodes.IsValidIndex( ChildIndex ) )
		{
			HandleGltfNode( NodeContainer, GltfAsset.Nodes[ ChildIndex ], NodeUid, ChildIndex, bHasVariants, SkinnedMeshNodes, UnusedMeshIndices );
		}
	}
}

void UInterchangeGltfTranslator::HandleGltfMaterialParameter( UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FTextureMap& TextureMap, UInterchangeShaderNode& ShaderNode,
		const FString& MapName, const TVariant< FLinearColor, float >& MapFactor, const FString& OutputChannel, const bool bInverse, const bool bIsNormal ) const
{
	using namespace UE::Interchange::Materials;
	using namespace UE::Interchange::Gltf::Private;

	UInterchangeShaderNode* NodeToConnectTo = &ShaderNode;
	FString InputToConnectTo = MapName;

	if (bInverse)
	{
		const FString OneMinusNodeName = MapName + TEXT("OneMinus");
		UInterchangeShaderNode* OneMinusNode = UInterchangeShaderNode::Create( &NodeContainer, OneMinusNodeName, ShaderNode.GetUniqueID() );

		OneMinusNode->SetCustomShaderType(Standard::Nodes::OneMinus::Name.ToString());

		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(NodeToConnectTo, InputToConnectTo, OneMinusNode->GetUniqueID());

		NodeToConnectTo = OneMinusNode;
		InputToConnectTo = Standard::Nodes::OneMinus::Inputs::Input.ToString();
	}

	bool bTextureHasImportance = true;

	if ( MapFactor.IsType< float >() )
	{
		bTextureHasImportance = !FMath::IsNearlyZero( MapFactor.Get< float >() );
	}
	else if ( MapFactor.IsType< FLinearColor >() )
	{
		bTextureHasImportance = !MapFactor.Get< FLinearColor >().IsAlmostBlack();
	}

	if ( bTextureHasImportance && GltfAsset.Textures.IsValidIndex( TextureMap.TextureIndex ) )
	{
		const FString ColorNodeName = MapName;
		UInterchangeShaderNode* ColorNode = UInterchangeShaderNode::Create( &NodeContainer, ColorNodeName, ShaderNode.GetUniqueID() );
		ColorNode->SetCustomShaderType( Standard::Nodes::TextureSample::Name.ToString() );

		const FString TextureUid = UInterchangeTextureNode::MakeNodeUid(GltfUniqueIdGenerators[GetSourceData()].GetUniqueId(&GltfAsset.Textures[TextureMap.TextureIndex]));

		ColorNode->AddStringAttribute( UInterchangeShaderPortsAPI::MakeInputValueKey( Standard::Nodes::TextureSample::Inputs::Texture.ToString() ), TextureUid );

		if (TextureMap.TexCoord > 0 || TextureMap.bHasTextureTransform)
		{
			UInterchangeShaderNode* TexCoordNode = UInterchangeShaderNode::Create(&NodeContainer, MapName + TEXT("\\TexCoord"), ShaderNode.GetUniqueID());
			TexCoordNode->SetCustomShaderType(Standard::Nodes::TextureCoordinate::Name.ToString());

			TexCoordNode->AddInt32Attribute(UInterchangeShaderPortsAPI::MakeInputValueKey(Standard::Nodes::TextureCoordinate::Inputs::Index.ToString()), TextureMap.TexCoord);

			UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ColorNode, Standard::Nodes::TextureSample::Inputs::Coordinates.ToString(), TexCoordNode->GetUniqueID());

			if (TextureMap.bHasTextureTransform)
			{
				HandleGltfTextureTransform(NodeContainer, TextureMap.TextureTransform, TextureMap.TexCoord, *TexCoordNode);
			}
		}

		bool bNeedsFactorNode = false;

		if ( MapFactor.IsType< float >() )
		{
			bNeedsFactorNode = !FMath::IsNearlyEqual( MapFactor.Get< float >(), 1.f );
		}
		else if ( MapFactor.IsType< FLinearColor >() )
		{
			bNeedsFactorNode = !MapFactor.Get< FLinearColor >().Equals( FLinearColor::White );
		}

		if ( bNeedsFactorNode )
		{
			UInterchangeShaderNode* FactorNode = UInterchangeShaderNode::Create( &NodeContainer, ColorNodeName + TEXT("_Factor"), ShaderNode.GetUniqueID() );

			if ( bIsNormal )
			{
				FactorNode->SetCustomShaderType( Standard::Nodes::FlattenNormal::Name.ToString() );

				UInterchangeShaderNode* FactorOneMinusNode = UInterchangeShaderNode::Create( &NodeContainer, ColorNodeName + TEXT("_Factor_OneMinus"), ShaderNode.GetUniqueID() );
				FactorOneMinusNode->SetCustomShaderType(Standard::Nodes::OneMinus::Name.ToString());

				if ( MapFactor.IsType< float >() )
				{
					FactorOneMinusNode->AddFloatAttribute( UInterchangeShaderPortsAPI::MakeInputValueKey( Standard::Nodes::OneMinus::Inputs::Input.ToString() ), MapFactor.Get< float >() );
				}

				UInterchangeShaderPortsAPI::ConnectOuputToInput( FactorNode, Standard::Nodes::FlattenNormal::Inputs::Normal.ToString(), ColorNode->GetUniqueID(), OutputChannel );
				UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput( FactorNode, Standard::Nodes::FlattenNormal::Inputs::Flatness.ToString(), FactorOneMinusNode->GetUniqueID() );
			}
			else
			{
				FactorNode->SetCustomShaderType( Standard::Nodes::Multiply::Name.ToString() );

				if ( MapFactor.IsType< float >() )
				{
					FactorNode->AddFloatAttribute( UInterchangeShaderPortsAPI::MakeInputValueKey( Standard::Nodes::Multiply::Inputs::B.ToString() ), MapFactor.Get< float >() );
				}
				else if ( MapFactor.IsType< FLinearColor >() )
				{
					FactorNode->AddLinearColorAttribute( UInterchangeShaderPortsAPI::MakeInputValueKey( Standard::Nodes::Multiply::Inputs::B.ToString() ), MapFactor.Get< FLinearColor >() );
				}

				UInterchangeShaderPortsAPI::ConnectOuputToInput( FactorNode, Standard::Nodes::Multiply::Inputs::A.ToString(), ColorNode->GetUniqueID(), OutputChannel );
			}

			UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput( NodeToConnectTo, InputToConnectTo, FactorNode->GetUniqueID() );
		}
		else
		{
			UInterchangeShaderPortsAPI::ConnectOuputToInput( NodeToConnectTo, InputToConnectTo, ColorNode->GetUniqueID(), OutputChannel );
		}
	}
	else
	{
		if ( bIsNormal && !bTextureHasImportance )
		{
			//default normal value is 0,0,1 (blue)
			NodeToConnectTo->AddLinearColorAttribute( UInterchangeShaderPortsAPI::MakeInputValueKey(InputToConnectTo), FLinearColor::Blue );
		}
		else
		{
			if ( MapFactor.IsType< FLinearColor >() )
			{
				NodeToConnectTo->AddLinearColorAttribute( UInterchangeShaderPortsAPI::MakeInputValueKey(InputToConnectTo), MapFactor.Get< FLinearColor >() );
			}
			else if ( MapFactor.IsType< float >() )
			{
				NodeToConnectTo->AddFloatAttribute( UInterchangeShaderPortsAPI::MakeInputValueKey(InputToConnectTo), MapFactor.Get< float >() );
			}
		}
	}
}

void UInterchangeGltfTranslator::HandleGltfMaterial( UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FMaterial& GltfMaterial, UInterchangeShaderGraphNode& ShaderGraphNode ) const
{
	using namespace UE::Interchange::Materials;

	ShaderGraphNode.SetCustomTwoSided( GltfMaterial.bIsDoubleSided );

	//Based on the gltf specification the basecolor and emissive textures have SRGB colors:
	SetTextureSRGB(NodeContainer, GltfMaterial.BaseColor);
	SetTextureSRGB(NodeContainer, GltfMaterial.Emissive);
	//According to GLTF documentation the normal maps are right handed (following OpenGL convention),
	//however UE expects left handed normal maps, this can be resolved by flipping the green channel of the normal textures:
	//(based on https://github.com/KhronosGroup/glTF-Sample-Models/tree/master/2.0/NormalTangentTest#problem-flipped-y-axis-or-flipped-green-channel)
	SetTextureFlipGreenChannel(NodeContainer, GltfMaterial.Normal);
	SetTextureFlipGreenChannel(NodeContainer, GltfMaterial.ClearCoat.NormalMap);

	if (GltfMaterial.bIsUnlitShadingModel)
	{
		// Base Color
		{
			TVariant< FLinearColor, float > BaseColorFactor;
			BaseColorFactor.Set< FLinearColor >(FLinearColor(GltfMaterial.BaseColorFactor));

			HandleGltfMaterialParameter(NodeContainer, GltfMaterial.BaseColor, ShaderGraphNode, Unlit::Parameters::UnlitColor.ToString(),
				BaseColorFactor, Standard::Nodes::TextureSample::Outputs::RGB.ToString());
		}

		// Opacity (use the base color alpha channel)
		if (GltfMaterial.AlphaMode != GLTF::FMaterial::EAlphaMode::Opaque)
		{
			TVariant< FLinearColor, float > OpacityFactor;
			OpacityFactor.Set< float >(GltfMaterial.BaseColorFactor.W);

			HandleGltfMaterialParameter(NodeContainer, GltfMaterial.BaseColor, ShaderGraphNode, PBR::Parameters::Opacity.ToString(),
				OpacityFactor, Standard::Nodes::TextureSample::Outputs::A.ToString());
		}

		return;
	}
	
	//if there is a clearcoatnormal map then we want to swap it with the normals map
	//as the Interchange pipeline will connect the clearcoatnormal map to ClearCoatBottomNormalMap
	//however as per specification the gltf.clearcoatnormal.map should be the top clearcoat and the gltf.normal.map should be the bottom one.
	bool bSwapNormalAndClearCoatNormal = GltfMaterial.bHasClearCoat;

	if ( GltfMaterial.ShadingModel == GLTF::FMaterial::EShadingModel::MetallicRoughness )
	{
		// Base Color
		{
			TVariant< FLinearColor, float > BaseColorFactor;
			BaseColorFactor.Set< FLinearColor >( FLinearColor( GltfMaterial.BaseColorFactor ) );

			HandleGltfMaterialParameter( NodeContainer, GltfMaterial.BaseColor, ShaderGraphNode, PBR::Parameters::BaseColor.ToString(),
				BaseColorFactor, Standard::Nodes::TextureSample::Outputs::RGB.ToString() );
		}

		// Metallic
		{
			TVariant< FLinearColor, float > MetallicFactor;
			MetallicFactor.Set< float >( GltfMaterial.MetallicRoughness.MetallicFactor );

			HandleGltfMaterialParameter( NodeContainer, GltfMaterial.MetallicRoughness.Map, ShaderGraphNode, PBR::Parameters::Metallic.ToString(),
				MetallicFactor, Standard::Nodes::TextureSample::Outputs::B.ToString() );
		}

		// Roughness
		{
			TVariant< FLinearColor, float > RoughnessFactor;
			RoughnessFactor.Set< float >( GltfMaterial.MetallicRoughness.RoughnessFactor );

			HandleGltfMaterialParameter( NodeContainer, GltfMaterial.MetallicRoughness.Map, ShaderGraphNode, PBR::Parameters::Roughness.ToString(),
				RoughnessFactor, Standard::Nodes::TextureSample::Outputs::G.ToString() );
		}

		// Specular
		if (GltfMaterial.bHasSpecular)
		{
			TVariant< FLinearColor, float > SpecularFactor;
			SpecularFactor.Set< float >( GltfMaterial.Specular.SpecularFactor );

			HandleGltfMaterialParameter( NodeContainer, GltfMaterial.Specular.SpecularMap, ShaderGraphNode, PBR::Parameters::Specular.ToString(),
				SpecularFactor, Standard::Nodes::TextureSample::Outputs::RGB.ToString() );
		}
	}
	else if ( GltfMaterial.ShadingModel == GLTF::FMaterial::EShadingModel::SpecularGlossiness )
	{
		// Diffuse Color
		{
			TVariant< FLinearColor, float > DiffuseColorFactor;
			DiffuseColorFactor.Set< FLinearColor >( FLinearColor( GltfMaterial.BaseColorFactor ) );

			HandleGltfMaterialParameter( NodeContainer, GltfMaterial.BaseColor, ShaderGraphNode, Phong::Parameters::DiffuseColor.ToString(),
				DiffuseColorFactor, Standard::Nodes::TextureSample::Outputs::RGB.ToString() );
		}

		// Specular Color
		{
			TVariant< FLinearColor, float > SpecularColorFactor;
			SpecularColorFactor.Set< FLinearColor >( FLinearColor( GltfMaterial.SpecularGlossiness.SpecularFactor ) );

			HandleGltfMaterialParameter( NodeContainer, GltfMaterial.SpecularGlossiness.Map, ShaderGraphNode, Phong::Parameters::SpecularColor.ToString(),
				SpecularColorFactor, Standard::Nodes::TextureSample::Outputs::RGB.ToString() );
		}

		// Glossiness
		{
			TVariant< FLinearColor, float > GlossinessFactor;
			GlossinessFactor.Set< float >( GltfMaterial.SpecularGlossiness.GlossinessFactor );

			const bool bInverse = true;
			HandleGltfMaterialParameter( NodeContainer, GltfMaterial.SpecularGlossiness.Map, ShaderGraphNode, PBR::Parameters::Roughness.ToString(),
				GlossinessFactor, Standard::Nodes::TextureSample::Outputs::A.ToString(), bInverse );
		}
	}

	// Additional maps
	{
		// Normal
		if ( GltfMaterial.Normal.TextureIndex != INDEX_NONE )
		{
			TVariant< FLinearColor, float > NormalFactor;
			NormalFactor.Set< float >( GltfMaterial.NormalScale );

			const bool bInverse = false;
			const bool bIsNormal = true;

			HandleGltfMaterialParameter( NodeContainer, GltfMaterial.Normal, ShaderGraphNode, bSwapNormalAndClearCoatNormal ? ClearCoat::Parameters::ClearCoatNormal.ToString() : Common::Parameters::Normal.ToString(),
				NormalFactor, Standard::Nodes::TextureSample::Outputs::RGB.ToString(), bInverse, bIsNormal );
		}

		// Emissive
		if ( GltfMaterial.Emissive.TextureIndex != INDEX_NONE || !GltfMaterial.EmissiveFactor.IsNearlyZero() )
		{
			TVariant< FLinearColor, float > EmissiveFactor;
			EmissiveFactor.Set< FLinearColor >( FLinearColor( GltfMaterial.EmissiveFactor ) );

			HandleGltfMaterialParameter( NodeContainer, GltfMaterial.Emissive, ShaderGraphNode, Common::Parameters::EmissiveColor.ToString(),
				EmissiveFactor, Standard::Nodes::TextureSample::Outputs::RGB.ToString() );
		}

		// Occlusion
		if ( GltfMaterial.Occlusion .TextureIndex != INDEX_NONE )
		{
			TVariant< FLinearColor, float > OcclusionFactor;
			OcclusionFactor.Set< float >( GltfMaterial.OcclusionStrength );

			HandleGltfMaterialParameter( NodeContainer, GltfMaterial.Occlusion, ShaderGraphNode, PBR::Parameters::Occlusion.ToString(),
				OcclusionFactor, Standard::Nodes::TextureSample::Outputs::R.ToString() );
		}

		// Opacity (use the base color alpha channel)
		if ( GltfMaterial.AlphaMode != GLTF::FMaterial::EAlphaMode::Opaque )
		{
			TVariant< FLinearColor, float > OpacityFactor;
			OpacityFactor.Set< float >( GltfMaterial.BaseColorFactor.W );

			HandleGltfMaterialParameter( NodeContainer, GltfMaterial.BaseColor, ShaderGraphNode, PBR::Parameters::Opacity.ToString(),
				OpacityFactor, Standard::Nodes::TextureSample::Outputs::A.ToString() );
		}

		// Alpha cutoff
		if ( GltfMaterial.AlphaMode == GLTF::FMaterial::EAlphaMode::Mask )
		{
			ShaderGraphNode.SetCustomOpacityMaskClipValue( GltfMaterial.AlphaCutoff );
		}

		// IOR
		if ( GltfMaterial.bHasIOR )
		{
			ShaderGraphNode.AddFloatAttribute( UInterchangeShaderPortsAPI::MakeInputValueKey( PBR::Parameters::IndexOfRefraction.ToString() ), GltfMaterial.IOR );
		}
	}

	if ( GltfMaterial.bHasClearCoat )
	{
		HandleGltfClearCoat( NodeContainer, GltfMaterial, ShaderGraphNode, bSwapNormalAndClearCoatNormal );
	}

	if ( GltfMaterial.bHasSheen )
	{
		HandleGltfSheen( NodeContainer, GltfMaterial, ShaderGraphNode );
	}

	if ( GltfMaterial.bHasTransmission )
	{
		HandleGltfTransmission( NodeContainer, GltfMaterial, ShaderGraphNode );
	}
}

void UInterchangeGltfTranslator::HandleGltfClearCoat( UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FMaterial& GltfMaterial, UInterchangeShaderGraphNode& ShaderGraphNode, const bool bSwapNormalAndClearCoatNormal) const
{
	using namespace UE::Interchange::Materials;

	if ( !GltfMaterial.bHasClearCoat || FMath::IsNearlyZero( GltfMaterial.ClearCoat.ClearCoatFactor ) )
	{
		return;
	}

	// ClearCoat::Parameters::ClearCoat
	{
		TVariant< FLinearColor, float > ClearCoatFactor;
		ClearCoatFactor.Set< float >( GltfMaterial.ClearCoat.ClearCoatFactor );

		HandleGltfMaterialParameter( NodeContainer, GltfMaterial.ClearCoat.ClearCoatMap, ShaderGraphNode, ClearCoat::Parameters::ClearCoat.ToString(),
			ClearCoatFactor, Standard::Nodes::TextureSample::Outputs::R.ToString() );
	}

	//  ClearCoat::Parameters::ClearCoatRoughness
	{
		TVariant< FLinearColor, float > ClearCoatRoughnessFactor;
		ClearCoatRoughnessFactor.Set< float >( GltfMaterial.ClearCoat.Roughness );

		HandleGltfMaterialParameter( NodeContainer, GltfMaterial.ClearCoat.RoughnessMap, ShaderGraphNode, ClearCoat::Parameters::ClearCoatRoughness.ToString(),
			ClearCoatRoughnessFactor, Standard::Nodes::TextureSample::Outputs::G.ToString() );
	}

	// ClearCoat::Parameters::ClearCoatNormal
	{
		TVariant< FLinearColor, float > ClearCoatNormalFactor;
		ClearCoatNormalFactor.Set< float >( GltfMaterial.ClearCoat.NormalMapUVScale );

		const bool bInverse = false;
		const bool bIsNormal = true;

		HandleGltfMaterialParameter( NodeContainer, GltfMaterial.ClearCoat.NormalMap, ShaderGraphNode, bSwapNormalAndClearCoatNormal ? Common::Parameters::Normal.ToString() : ClearCoat::Parameters::ClearCoatNormal.ToString(),
			ClearCoatNormalFactor, Standard::Nodes::TextureSample::Outputs::RGB.ToString(), bInverse, bIsNormal );
	}
}

void UInterchangeGltfTranslator::HandleGltfSheen( UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FMaterial& GltfMaterial, UInterchangeShaderGraphNode& ShaderGraphNode ) const
{
	using namespace UE::Interchange::Materials;

	if ( !GltfMaterial.bHasSheen )
	{
		return;
	}

	// Sheen::Parameters::SheenColor
	{
		TVariant< FLinearColor, float > SheenColorFactor;
		SheenColorFactor.Set< FLinearColor >( FLinearColor( GltfMaterial.Sheen.SheenColorFactor ) );

		HandleGltfMaterialParameter( NodeContainer, GltfMaterial.Sheen.SheenColorMap, ShaderGraphNode, Sheen::Parameters::SheenColor.ToString(),
			SheenColorFactor, Standard::Nodes::TextureSample::Outputs::RGB.ToString() );
	}

	// Sheen::Parameters::SheenRoughness
	{
		TVariant< FLinearColor, float > SheenRoughnessFactor;
		SheenRoughnessFactor.Set< float >( GltfMaterial.Sheen.SheenRoughnessFactor );

		HandleGltfMaterialParameter( NodeContainer, GltfMaterial.Sheen.SheenRoughnessMap, ShaderGraphNode, Sheen::Parameters::SheenRoughness.ToString(),
			SheenRoughnessFactor, Standard::Nodes::TextureSample::Outputs::A.ToString() );
	}
}

/**
 * GLTF transmission is handled a little differently than UE's.
 * GLTF doesn't allow having different reflected and transmitted colors, UE does (base color vs transmittance color).
 * GLTF controls the amount of reflected light vs transmitted light using the transmission factor, UE does that through opacity.
 * GLTF opacity means that the medium is present of not, so it's normal for transmission materials to be considered opaque,
 * meaning that the medium is full present, and the transmission factor determines how much lights is transmitted.
 * When a transmission material isn't fully opaque, we reduce the transmission color by the opacity to mimic GLTF's BTDF.
 * Ideally, this would be better represented by blending a default lit alpha blended material with a thin translucent material based on GLTF's opacity.
 */
void UInterchangeGltfTranslator::HandleGltfTransmission( UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FMaterial& GltfMaterial, UInterchangeShaderGraphNode& ShaderGraphNode ) const
{
	using namespace UE::Interchange::Materials;

	if ( !GltfMaterial.bHasTransmission || FMath::IsNearlyZero( GltfMaterial.Transmission.TransmissionFactor ) )
	{
		return;
	}
 
	FString OpacityNodeUid;
	FString OpacityNodeOutput;

	// Common::Parameters::Opacity
	{
		/**
		 * Per the spec, the red channel of the transmission texture drives how much light is transmitted vs diffused.
		 * So we're setting the inverse of the red channel as the opacity.
		 */
		const FString OneMinusNodeName =  TEXT("OpacityOneMinus");
		UInterchangeShaderNode* OneMinusNode = UInterchangeShaderNode::Create( &NodeContainer, OneMinusNodeName, ShaderGraphNode.GetUniqueID() );

		OneMinusNode->SetCustomShaderType(Standard::Nodes::OneMinus::Name.ToString());

		UInterchangeShaderNode* CurrentNode = OneMinusNode;
		FString CurrentInput = Standard::Nodes::OneMinus::Inputs::Input.ToString();

		TVariant< FLinearColor, float > TransmissionFactor;
		TransmissionFactor.Set< float >( GltfMaterial.Transmission.TransmissionFactor );

		HandleGltfMaterialParameter( NodeContainer, GltfMaterial.Transmission.TransmissionMap, *CurrentNode, CurrentInput,
			TransmissionFactor, Standard::Nodes::TextureSample::Outputs::R.ToString() );

		// The GLTF transmission model specifies that metallic surfaces don't transmit light, so adjust Common::Parameters::Opacity so that metallic surfaces are opaque.
		{
			FString MetallicNodeUid;
			FString MetallicNodeOutput;

			if ( UInterchangeShaderPortsAPI::GetInputConnection( &ShaderGraphNode, PBR::Parameters::Metallic.ToString(), MetallicNodeUid, MetallicNodeOutput ) )
			{
				const FString MetallicLerpNodeName =  TEXT("OpacityMetallicLerp");
				UInterchangeShaderNode* LerpMetallicNode = UInterchangeShaderNode::Create( &NodeContainer, MetallicLerpNodeName, ShaderGraphNode.GetUniqueID() );
				LerpMetallicNode->SetCustomShaderType( Standard::Nodes::Lerp::Name.ToString() );


				LerpMetallicNode->AddFloatAttribute( UInterchangeShaderPortsAPI::MakeInputValueKey( Standard::Nodes::Lerp::Inputs::B.ToString() ), 1.f );
				UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput( LerpMetallicNode, Standard::Nodes::Lerp::Inputs::A.ToString(), CurrentNode->GetUniqueID() );
				UInterchangeShaderPortsAPI::ConnectOuputToInput( LerpMetallicNode, Standard::Nodes::Lerp::Inputs::Factor.ToString(), MetallicNodeUid, MetallicNodeOutput );

				CurrentNode = LerpMetallicNode;
				CurrentInput = TEXT("");
			}
		}

		if ( GltfMaterial.AlphaMode != GLTF::FMaterial::EAlphaMode::Opaque )
		{
			if ( UInterchangeShaderPortsAPI::GetInputConnection( &ShaderGraphNode, PBR::Parameters::Opacity.ToString(), OpacityNodeUid, OpacityNodeOutput ) )
			{
				const FString OpacityLerpNodeName =  TEXT("OpacityLerp");
				UInterchangeShaderNode* OpacityLerpNode = UInterchangeShaderNode::Create( &NodeContainer, OpacityLerpNodeName, ShaderGraphNode.GetUniqueID() );
				OpacityLerpNode->SetCustomShaderType( Standard::Nodes::Lerp::Name.ToString() );

				OpacityLerpNode->AddFloatAttribute( UInterchangeShaderPortsAPI::MakeInputValueKey( Standard::Nodes::Lerp::Inputs::A.ToString() ), 0.f );
				UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput( OpacityLerpNode, Standard::Nodes::Lerp::Inputs::B.ToString(), CurrentNode->GetUniqueID() );
				UInterchangeShaderPortsAPI::ConnectOuputToInput( OpacityLerpNode, Standard::Nodes::Lerp::Inputs::Factor.ToString(), OpacityNodeUid, OpacityNodeOutput );

				CurrentNode = OpacityLerpNode;
				CurrentInput = TEXT("");
			}
		}

		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput( &ShaderGraphNode, Common::Parameters::Opacity.ToString(), CurrentNode->GetUniqueID() );
	}

	// ThinTranslucent::Parameters::Transmissioncolor
	{
		// There's no separation of reflected and transmitted color in this model. So the same color is used for the base color and the transmitted color.
		// Since this extension is only supported with the metallic-roughness model, we can reuse its base color
		const UInterchangeBaseNode* CurrentNode = &ShaderGraphNode;
		FString CurrentOuput = TEXT("");
		FLinearColor CurrentColor = FLinearColor::White;

		FString BaseColorNodeUid;
		FString BaseColorNodeOutput;

		if ( UInterchangeShaderPortsAPI::GetInputConnection( CurrentNode, PBR::Parameters::BaseColor.ToString(), BaseColorNodeUid, BaseColorNodeOutput ) )
		{
			CurrentNode = NodeContainer.GetNode( BaseColorNodeUid );
			CurrentOuput = BaseColorNodeOutput;
		}
		else
		{
			FLinearColor BaseColor;
			if ( ShaderGraphNode.GetLinearColorAttribute( UInterchangeShaderPortsAPI::MakeInputValueKey( PBR::Parameters::BaseColor.ToString() ), BaseColor ) )
			{
				CurrentNode = nullptr;
				CurrentColor = BaseColor;
			}
		}

		if ( GltfMaterial.AlphaMode != GLTF::FMaterial::EAlphaMode::Opaque )
		{
			if ( !OpacityNodeUid.IsEmpty() )
			{
				const FString OpacityLerpNodeName =  TEXT("OpacityTransmissionLerp");
				UInterchangeShaderNode* OpacityLerpNode = UInterchangeShaderNode::Create( &NodeContainer, OpacityLerpNodeName, ShaderGraphNode.GetUniqueID() );
				OpacityLerpNode->SetCustomShaderType( Standard::Nodes::Lerp::Name.ToString() );

				OpacityLerpNode->AddLinearColorAttribute( UInterchangeShaderPortsAPI::MakeInputValueKey( Standard::Nodes::Lerp::Inputs::A.ToString() ), FLinearColor::White );
				UInterchangeShaderPortsAPI::ConnectOuputToInput( OpacityLerpNode, Standard::Nodes::Lerp::Inputs::Factor.ToString(), OpacityNodeUid, OpacityNodeOutput );

				if ( CurrentNode )
				{
					UInterchangeShaderPortsAPI::ConnectOuputToInput( OpacityLerpNode, Standard::Nodes::Lerp::Inputs::B.ToString(), CurrentNode->GetUniqueID(), CurrentOuput );
				}
				else
				{
					OpacityLerpNode->AddLinearColorAttribute( UInterchangeShaderPortsAPI::MakeInputValueKey( Standard::Nodes::Lerp::Inputs::B.ToString() ), CurrentColor );
				}

				CurrentNode = OpacityLerpNode;
				CurrentOuput = TEXT("");
			}
		}

		if ( CurrentNode )
		{
			UInterchangeShaderPortsAPI::ConnectOuputToInput( &ShaderGraphNode, ThinTranslucent::Parameters::TransmissionColor.ToString(), CurrentNode->GetUniqueID(), CurrentOuput );
		}
		else
		{
			ShaderGraphNode.AddLinearColorAttribute( UInterchangeShaderPortsAPI::MakeInputValueKey( ThinTranslucent::Parameters::TransmissionColor.ToString() ), CurrentColor );
		}
	}
}

void UInterchangeGltfTranslator::HandleGltfTextureTransform( UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FTextureTransform& TextureTransform, const int32 TexCoordIndex, UInterchangeShaderNode& ShaderNode ) const
{
	using namespace UE::Interchange::Materials;

	// Scale
	if ( !FMath::IsNearlyEqual( TextureTransform.Scale[0], 1.f ) || 
		 !FMath::IsNearlyEqual( TextureTransform.Scale[1], 1.f ) )
	{
		FVector2f TextureScale;
		TextureScale.X = TextureTransform.Scale[0];
		TextureScale.Y = TextureTransform.Scale[1];

		ShaderNode.SetAttribute< FVector2f >( UInterchangeShaderPortsAPI::MakeInputValueKey( Standard::Nodes::TextureCoordinate::Inputs::Scale.ToString() ), TextureScale );
	}

	// Offset
	if ( !FMath::IsNearlyZero( TextureTransform.Offset[0] ) || 
		 !FMath::IsNearlyZero( TextureTransform.Offset[1] ) )
	{
		FVector2f TextureOffset;
		TextureOffset.X = TextureTransform.Offset[0];
		TextureOffset.Y = TextureTransform.Offset[1];

		ShaderNode.SetAttribute< FVector2f >( UInterchangeShaderPortsAPI::MakeInputValueKey( Standard::Nodes::TextureCoordinate::Inputs::Offset.ToString() ), TextureOffset );
	}

	// Rotate
	if ( !FMath::IsNearlyZero( TextureTransform.Rotation ) )
	{
		float AngleRadians = TextureTransform.Rotation;

		if ( AngleRadians < 0.0f )
		{
			AngleRadians = TWO_PI - AngleRadians;
		}

		AngleRadians = 1.0f - ( AngleRadians / TWO_PI );

		ShaderNode.AddFloatAttribute( UInterchangeShaderPortsAPI::MakeInputValueKey( Standard::Nodes::TextureCoordinate::Inputs::Rotate.ToString() ), AngleRadians );

		FVector2f RotationCenter = FVector2f::Zero();
		ShaderNode.SetAttribute< FVector2f >( UInterchangeShaderPortsAPI::MakeInputValueKey( Standard::Nodes::TextureCoordinate::Inputs::RotationCenter.ToString() ), RotationCenter );
	}

}

EInterchangeTranslatorType UInterchangeGltfTranslator::GetTranslatorType() const
{
	return EInterchangeTranslatorType::Scenes;
}

EInterchangeTranslatorAssetType UInterchangeGltfTranslator::GetSupportedAssetTypes() const
{
	//gltf translator support Meshes and Materials
	return EInterchangeTranslatorAssetType::Materials | EInterchangeTranslatorAssetType::Meshes | EInterchangeTranslatorAssetType::Animations;
}

TArray<FString> UInterchangeGltfTranslator::GetSupportedFormats() const
{
	TArray<FString> GltfExtensions;

	if ( GInterchangeEnableGLTFImport || GIsAutomationTesting )
	{
		GltfExtensions.Reserve(2);
		GltfExtensions.Add(TEXT("gltf;GL Transmission Format"));
		GltfExtensions.Add(TEXT("glb;GL Transmission Format (Binary)"));
	}

	return GltfExtensions;
}

bool UInterchangeGltfTranslator::Translate( UInterchangeBaseNodeContainer& NodeContainer ) const
{
	using namespace UE::Interchange::Gltf::Private;

	FString FilePath = GetSourceData()->GetFilename();
	if ( !FPaths::FileExists( FilePath ) )
	{
		return false;
	}

	GLTF::FFileReader GltfFileReader;

	const bool bLoadImageData = false;
	const bool bLoadMetaData = false;
	GltfFileReader.ReadFile( FilePath, bLoadImageData, bLoadMetaData, const_cast< UInterchangeGltfTranslator* >( this )->GltfAsset );

	ScaleNodeTranslations(const_cast<UInterchangeGltfTranslator*>(this)->GltfAsset.Nodes, GltfUnitConversionMultiplier);

	const FString FileName = FPaths::GetBaseFilename(FilePath);

	if (GltfAsset.RequiredExtensions.Num() != 0)
	{
		FString NotSupportedExtensions;
		for ( const FString& RequiredExtension: GltfAsset.RequiredExtensions )
		{
			if ( SupportedExtensions.Find( RequiredExtension ) == INDEX_NONE )
			{
				if ( NotSupportedExtensions.Len() > 0 )
				{
					NotSupportedExtensions += ", ";
				}
				NotSupportedExtensions += RequiredExtension;
			}
		}
		if ( NotSupportedExtensions.Len() > 0 )
		{
			UInterchangeResultError_Generic* ErrorResult = AddMessage< UInterchangeResultError_Generic >();
			ErrorResult->SourceAssetName = FileName;
			ErrorResult->Text = FText::Format(
				LOCTEXT( "UnsupportedRequiredExtensions", "Not All Required Extensions are supported. (Unsupported extensions: {0})"),
				FText::FromString( NotSupportedExtensions ) );

			return false;
		}
	}

	const_cast< UInterchangeGltfTranslator* >( this )->GltfAsset.GenerateNames(FileName); //uniqueIds are generated on demand
	GltfUniqueIdGeneratorsRefCounter.FindOrAdd(GetSourceData())++;
	GltfUniqueIdGenerators.FindOrAdd(GetSourceData());
	GltfUniqueIdGenerators[GetSourceData()].Prefix = FileName;

	// Textures
	{
		int32 TextureIndex = 0;
		for ( const GLTF::FTexture& GltfTexture : GltfAsset.Textures )
		{
			UInterchangeTexture2DNode* TextureNode = UInterchangeTexture2DNode::Create(&NodeContainer, GltfUniqueIdGenerators[GetSourceData()].GetUniqueId(&GltfTexture));
			TextureNode->SetDisplayLabel(GltfTexture.Name);

			TextureNode->SetCustomFilter(ConvertFilter(GltfTexture.Sampler.MinFilter));
			TextureNode->SetPayLoadKey( LexToString( TextureIndex++ ) );

			TextureNode->SetCustomWrapU( UE::Interchange::Gltf::Private::ConvertWrap( GltfTexture.Sampler.WrapS ) );
			TextureNode->SetCustomWrapV( UE::Interchange::Gltf::Private::ConvertWrap( GltfTexture.Sampler.WrapT ) );
		}
	}

	// Materials
	{
		int32 MaterialIndex = 0;
		for ( const GLTF::FMaterial& GltfMaterial : GltfAsset.Materials )
		{
			UInterchangeShaderGraphNode* ShaderGraphNode = UInterchangeShaderGraphNode::Create(&NodeContainer, GltfUniqueIdGenerators[GetSourceData()].GetUniqueId(&GltfMaterial));
			ShaderGraphNode->SetDisplayLabel(GltfMaterial.Name);

			HandleGltfMaterial( NodeContainer, GltfMaterial, *ShaderGraphNode );
			++MaterialIndex;
		}
	}

	TSet<int> UnusedGltfMeshIndices;
	// Meshes
	{
		int32 MeshIndex = 0;
		for (const GLTF::FMesh& GltfMesh : GltfAsset.Meshes)
		{
			UnusedGltfMeshIndices.Add(MeshIndex++);
		}
	}

	// Cameras
	{
		int32 CameraIndex = 0;
		for ( const GLTF::FCamera& GltfCamera : GltfAsset.Cameras )
		{
			UInterchangeCameraNode* CameraNode = NewObject< UInterchangeCameraNode >( &NodeContainer );
			FString CameraNodeUid = TEXT("\\Camera\\") + GltfUniqueIdGenerators[GetSourceData()].GetUniqueId(&GltfCamera);
			CameraNode->InitializeNode( CameraNodeUid, GltfCamera.Name, EInterchangeNodeContainerType::TranslatedAsset );

			float       AspectRatio;
			float       FocalLength;
			const float SensorWidth = 36.f;  // mm
			CameraNode->SetCustomSensorWidth(SensorWidth);
			if (GltfCamera.bIsPerspective)
			{
				AspectRatio = GltfCamera.Perspective.AspectRatio;
				FocalLength = (SensorWidth / GltfCamera.Perspective.AspectRatio) / (2.0 * tan(GltfCamera.Perspective.Fov / 2.0));
			}
			else
			{
				AspectRatio = GltfCamera.Orthographic.XMagnification / GltfCamera.Orthographic.YMagnification;
				FocalLength = (SensorWidth / AspectRatio) / (AspectRatio * tan(AspectRatio / 4.0));  // can only approximate Fov
			}

			CameraNode->SetCustomSensorHeight(SensorWidth / AspectRatio);
			CameraNode->SetCustomFocalLength(FocalLength);
			CameraNode->SetCustomEnableDepthOfField(false);
			// ignoring znear and zfar

			NodeContainer.AddNode( CameraNode );
			++CameraIndex;
		}
	}

	// Lights
	{
		int32 LightIndex = 0;
		for ( const GLTF::FLight& GltfLight : GltfAsset.Lights )
		{
			FString LightNodeUid = TEXT("\\Light\\") + GltfUniqueIdGenerators[GetSourceData()].GetUniqueId(&GltfLight);

			switch (GltfLight.Type)
			{
			case GLTF::FLight::EType::Directional:
			{
				UInterchangeDirectionalLightNode* LightNode = NewObject< UInterchangeDirectionalLightNode >(&NodeContainer);
				LightNode->InitializeNode(LightNodeUid, GltfLight.Name, EInterchangeNodeContainerType::TranslatedAsset);

				LightNode->SetCustomLightColor(FLinearColor(GltfLight.Color));
				LightNode->SetCustomIntensity(GltfLight.Intensity);

				NodeContainer.AddNode(LightNode);
				++LightIndex;
			}
				break;
			case GLTF::FLight::EType::Point:
			{
				UInterchangePointLightNode* LightNode = NewObject< UInterchangePointLightNode >(&NodeContainer);
				LightNode->InitializeNode(LightNodeUid, GltfLight.Name, EInterchangeNodeContainerType::TranslatedAsset);
				
				LightNode->SetCustomIntensityUnits(EInterchangeLightUnits::Candelas);
				LightNode->SetCustomLightColor(FLinearColor(GltfLight.Color));
				LightNode->SetCustomIntensity(GltfLight.Intensity);
				
				LightNode->SetCustomAttenuationRadius(GltfLight.Range * GltfUnitConversionMultiplier);

				NodeContainer.AddNode(LightNode);
				++LightIndex;
			}
				break;
			case GLTF::FLight::EType::Spot:
			{
				UInterchangeSpotLightNode* LightNode = NewObject< UInterchangeSpotLightNode >(&NodeContainer);
				LightNode->InitializeNode(LightNodeUid, GltfLight.Name, EInterchangeNodeContainerType::TranslatedAsset);

				LightNode->SetCustomIntensityUnits(EInterchangeLightUnits::Candelas);
				LightNode->SetCustomLightColor(FLinearColor(GltfLight.Color));
				LightNode->SetCustomIntensity(GltfLight.Intensity);

				LightNode->SetCustomInnerConeAngle(FMath::RadiansToDegrees(GltfLight.Spot.InnerConeAngle));
				LightNode->SetCustomOuterConeAngle(FMath::RadiansToDegrees(GltfLight.Spot.OuterConeAngle));
				
				NodeContainer.AddNode(LightNode);
				++LightIndex;
			}
				break;
			default:
				break;
			}
		}
	}

	// Cache created scene nodes UIDs to use later for animation binding
	bool bHasVariants = false;

	// Scenes
	{
		int32 SceneIndex = 0;
		for ( const GLTF::FScene& GltfScene : GltfAsset.Scenes )
		{
			UInterchangeSceneNode* SceneNode = NewObject< UInterchangeSceneNode >( &NodeContainer );

			FString SceneName = GltfScene.Name;

			FString SceneNodeUid = TEXT("\\Scene\\") + GltfUniqueIdGenerators[GetSourceData()].GetUniqueId(&GltfScene);
			SceneNode->InitializeNode( SceneNodeUid, SceneName, EInterchangeNodeContainerType::TranslatedScene );
			NodeContainer.AddNode( SceneNode );

			//All scene node should have a valid local transform
			SceneNode->SetCustomLocalTransform(&NodeContainer, FTransform::Identity);

			TArray<int32> SkinnedMeshNodes;
			for ( const int32 NodeIndex : GltfScene.Nodes )
			{
				if ( GltfAsset.Nodes.IsValidIndex( NodeIndex ) )
				{
					HandleGltfNode( NodeContainer, GltfAsset.Nodes[ NodeIndex ], SceneNodeUid, NodeIndex, bHasVariants, SkinnedMeshNodes, UnusedGltfMeshIndices );
				}
			}

			// Skeletons:
			HandleGltfSkeletons( NodeContainer, SceneNodeUid, SkinnedMeshNodes, UnusedGltfMeshIndices );
		}
	}

	// Animations
	{
		for ( int32 AnimationIndex = 0; AnimationIndex < GltfAsset.Animations.Num(); ++AnimationIndex )
		{
			HandleGltfAnimation( NodeContainer, AnimationIndex );
		}
	}

	// Variants
	// Note: Variants are not supported yet in game play mode
	if ( !FApp::IsGame() && bHasVariants )
	{
		HandleGltfVariants( NodeContainer, FileName );
	}

	// Add glTF errors and warnings to the Interchange results
	for ( const GLTF::FLogMessage& LogMessage : GltfFileReader.GetLogMessages() )
	{
		UInterchangeResult* Result = nullptr;

		switch ( LogMessage.Get<0>() )
		{
		case GLTF::EMessageSeverity::Error :
			{
				UInterchangeResultError_Generic* ErrorResult = AddMessage< UInterchangeResultError_Generic >();
				ErrorResult->Text = FText::FromString(LogMessage.Get<1>());
				Result = ErrorResult;
			}
			break;
		case GLTF::EMessageSeverity::Warning:
		default:
			{
				UInterchangeResultWarning_Generic* WarningResult = AddMessage< UInterchangeResultWarning_Generic >();
				WarningResult->Text = FText::FromString(LogMessage.Get<1>());
				Result = WarningResult;
			}

			break;
		}

		if ( Result )
		{
			Result->SourceAssetName = FileName;
		}
	}

	// Create any Mesh Nodes for meshes that have not been used and just in the gltf as an asset:
	TSet<int> UnusedMeshIndices = UnusedGltfMeshIndices;
	for (int UnusedMeshIndex : UnusedMeshIndices)
	{
		HandleGltfMesh(NodeContainer, GltfAsset.Meshes[UnusedMeshIndex], UnusedMeshIndex, UnusedGltfMeshIndices);
	}

	if (UnusedGltfMeshIndices.Num() != 0)
	{
		UE_LOG(LogInterchangeImport, Warning, TEXT("GLTF Mesh Import Warning. Gltf Mesh Usage expectation is not met."));
	}

	GltfUniqueIdGeneratorsRefCounter[GetSourceData()]--;
	if (GltfUniqueIdGeneratorsRefCounter[GetSourceData()] <= 0)
	{
		GltfUniqueIdGenerators[GetSourceData()].Clear();
		GltfUniqueIdGenerators.Remove(GetSourceData());
		GltfUniqueIdGeneratorsRefCounter.Remove(GetSourceData());
	}

	return true;
}

TFuture< TOptional< UE::Interchange::FStaticMeshPayloadData > > UInterchangeGltfTranslator::GetStaticMeshPayloadData( const FString& PayLoadKey ) const
{
	return Async(EAsyncExecution::TaskGraph, [this, PayLoadKey]
		{
			UE::Interchange::FStaticMeshPayloadData StaticMeshPayloadData;
			UE::Interchange::Gltf::Private::GetStaticMeshPayloadDataForPayLoadKey(GltfAsset, PayLoadKey, StaticMeshPayloadData);

			TOptional<UE::Interchange::FStaticMeshPayloadData> Result;
			Result.Emplace(StaticMeshPayloadData);

			return Result;
		});
}

TOptional< UE::Interchange::FImportImage > UInterchangeGltfTranslator::GetTexturePayloadData( const UInterchangeSourceData* InSourceData, const FString& PayLoadKey ) const
{
	int32 TextureIndex = 0;
	LexFromString( TextureIndex, *PayLoadKey );

	if ( !GltfAsset.Textures.IsValidIndex( TextureIndex ) )
	{
		return TOptional< UE::Interchange::FImportImage >();
	}

	const GLTF::FTexture& GltfTexture = GltfAsset.Textures[ TextureIndex ];

	if (GltfTexture.Source.FilePath.IsEmpty())
	{
		// Embedded texture -- try using ImageWrapper to decode it
		TArray64<uint8> ImageData(GltfTexture.Source.Data, GltfTexture.Source.DataByteLength);
		UInterchangeImageWrapperTranslator* ImageWrapperTranslator = NewObject<UInterchangeImageWrapperTranslator>(GetTransientPackage(), NAME_None);
		return ImageWrapperTranslator->GetTexturePayloadDataFromBuffer(ImageData);
	}
	else
	{
		const FString TextureFilePath = FPaths::ConvertRelativePathToFull(GltfTexture.Source.FilePath);

		UInterchangeSourceData* PayloadSourceData = UInterchangeManager::GetInterchangeManager().CreateSourceData(TextureFilePath);
		FGCObjectScopeGuard ScopedSourceData(PayloadSourceData);

		if (!PayloadSourceData)
		{
			return TOptional<UE::Interchange::FImportImage>();
		}

		UInterchangeTranslatorBase* SourceTranslator = UInterchangeManager::GetInterchangeManager().GetTranslatorForSourceData(PayloadSourceData);
		FGCObjectScopeGuard ScopedSourceTranslator(SourceTranslator);
		const IInterchangeTexturePayloadInterface* TextureTranslator = Cast< IInterchangeTexturePayloadInterface >(SourceTranslator);
		if (!ensure(TextureTranslator))
		{
			return TOptional<UE::Interchange::FImportImage>();
		}

		return TextureTranslator->GetTexturePayloadData(PayloadSourceData, TextureFilePath);
	}
}

TFuture<TOptional<UE::Interchange::FAnimationCurvePayloadData>> UInterchangeGltfTranslator::GetAnimationCurvePayloadData(const FString& PayLoadKey) const
{
	return Async(EAsyncExecution::TaskGraph, [this, PayLoadKey]
		{

			TOptional<UE::Interchange::FAnimationCurvePayloadData> Result;
			UE::Interchange::FAnimationCurvePayloadData AnimationCurvePayloadData;

			if (UE::Interchange::Gltf::Private::GetAnimationTransformPayloadData(PayLoadKey, GltfAsset, AnimationCurvePayloadData))
			{
				Result.Emplace(AnimationCurvePayloadData);
			}

			return Result;
		}
	);
}

TFuture<TOptional<UE::Interchange::FAnimationStepCurvePayloadData>> UInterchangeGltfTranslator::GetAnimationStepCurvePayloadData(const FString& PayLoadKey) const
{
	using namespace UE::Interchange;

	TSharedPtr<TPromise<TOptional<FAnimationStepCurvePayloadData>>> Promise = MakeShared<TPromise<TOptional<FAnimationStepCurvePayloadData>>>();
	Promise->SetValue(TOptional<FAnimationStepCurvePayloadData>());
	return Promise->GetFuture();
}

TFuture<TOptional<UE::Interchange::FAnimationBakeTransformPayloadData>> UInterchangeGltfTranslator::GetAnimationBakeTransformPayloadData(const FString& PayLoadKey, const double BakeFrequency, const double RangeStartSecond, const double RangeStopSecond) const
{
	return Async(EAsyncExecution::TaskGraph, [this, PayLoadKey, BakeFrequency, RangeStartSecond, RangeStopSecond]
		{
			TOptional<UE::Interchange::FAnimationBakeTransformPayloadData> Result;

			UE::Interchange::FAnimationBakeTransformPayloadData PayloadData;

			PayloadData.BakeFrequency = BakeFrequency;
			PayloadData.RangeStartTime = RangeStartSecond;
			PayloadData.RangeEndTime = RangeStopSecond;

			if (UE::Interchange::Gltf::Private::GetBakedAnimationTransformPayloadData(PayLoadKey, GltfAsset, PayloadData))
			{
				Result.Emplace(PayloadData);
			}

			return Result;
		});
}

void UInterchangeGltfTranslator::HandleGltfAnimation(UInterchangeBaseNodeContainer& NodeContainer, int32 AnimationIndex) const
{
	const GLTF::FAnimation& GltfAnimation = GltfAsset.Animations[AnimationIndex];

	TMap< const GLTF::FNode*, TArray<int32> > NodeChannelsMap;

	TMap<int32, UInterchangeSkeletalAnimationTrackNode*> RootJointIndexToTrackNodeMap;
	TMap<UInterchangeSkeletalAnimationTrackNode*, TMap<FString, TArray<int32>>> TrackNodeToJointUidWithChannelsUsedMap;

	for (int32 ChannelIndex = 0; ChannelIndex < GltfAnimation.Channels.Num(); ++ChannelIndex)
	{
		const GLTF::FAnimation::FChannel& Channel = GltfAnimation.Channels[ChannelIndex];
		const GLTF::FNode* AnimatedNode = &Channel.Target.Node;

		if (AnimatedNode->Type == GLTF::FNode::EType::Joint && GltfAsset.Nodes.IsValidIndex(AnimatedNode->RootJointIndex))
		{
			const FString* JointNodeUid = NodeUidMap.Find(AnimatedNode);
			if (ensure(JointNodeUid) && GltfAsset.Nodes.IsValidIndex(AnimatedNode->RootJointIndex))
			{
				UInterchangeSkeletalAnimationTrackNode* TrackNode = nullptr;
				if (RootJointIndexToTrackNodeMap.Contains(AnimatedNode->RootJointIndex))
				{
					TrackNode = RootJointIndexToTrackNodeMap[AnimatedNode->RootJointIndex];
				}
				else
				{
					TrackNode = NewObject< UInterchangeSkeletalAnimationTrackNode >(&NodeContainer);
					FString TrackNodeUid = "\\SkeletalAnimation\\" + LexToString(AnimatedNode->RootJointIndex) + "_" + LexToString(AnimationIndex);
					TrackNode->InitializeNode(TrackNodeUid, GltfAnimation.Name, EInterchangeNodeContainerType::TranslatedAsset);
					const GLTF::FNode& RootJointNode = GltfAsset.Nodes[AnimatedNode->RootJointIndex];
					const FString* SkeletonNodeUid = NodeUidMap.Find(&RootJointNode);
					TrackNode->SetCustomSkeletonNodeUid(*SkeletonNodeUid);

					NodeContainer.AddNode(TrackNode);

					RootJointIndexToTrackNodeMap.Add(AnimatedNode->RootJointIndex, TrackNode);
				}
				TMap<FString, TArray<int32>>& JointUidWithChannelsUsedMap = TrackNodeToJointUidWithChannelsUsedMap.FindOrAdd(TrackNode);
				TArray<int32>& ChannelsUsed = JointUidWithChannelsUsedMap.FindOrAdd(*JointNodeUid);
				ChannelsUsed.Add(ChannelIndex);
			}
			
			continue;
		}

		TArray<int32>& NodeChannels = NodeChannelsMap.FindOrAdd(AnimatedNode);
		NodeChannels.Add(ChannelIndex);
	}

	//setup rigged animations:
	{
		for (const TTuple<UInterchangeSkeletalAnimationTrackNode*, TMap<FString, TArray<int32>>>& TrackNodeAndJointNodeChannels : TrackNodeToJointUidWithChannelsUsedMap)
		{
			//@ StartTime = 0;
			//from gltf documentation: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#animations
			//Implementation Note
			//	For example, if the earliest sampler input for an animation is t = 10, a client implementation must begin playback of that animation channel at t = 0 with output clamped to the first available output value.
			double StartTime = 0.0;
			double StopTime = 0.0;
			double FrameRate = 30.0;
			double SingleFrameDuration = 1.0 / FrameRate;
			int32 FrameNumber = 0;
			bool bHasAnimationPayloadSet = false;

			for (const TTuple<FString, TArray<int32>>& JointNodeUidAndChannelsUsedPair : TrackNodeAndJointNodeChannels.Value)
			{
				double PreviousStopTime = StopTime;
				//check channel length and build payload
				FString Payload = LexToString(AnimationIndex);
				for (int32 ChannelIndex : JointNodeUidAndChannelsUsedPair.Value)
				{
					const GLTF::FAnimation::FChannel& Channel = GltfAnimation.Channels[ChannelIndex];
					const GLTF::FAnimation::FSampler& Sampler = GltfAnimation.Samplers[Channel.Sampler];
					TArray<float> Seconds;
					Sampler.Input.GetFloatArray(Seconds);

					if (Sampler.Interpolation == GLTF::FAnimation::EInterpolation::CubicSpline)
					{
						if (Sampler.Input.Count != 3 * Sampler.Output.Count)
						{
							// if any of the channels are corrupt the joint will not receive any of the  animation data
							Payload = "";
							UE_LOG(LogInterchangeImport, Warning, TEXT("GLTF Sampler Corrupt. Input and Output not meeting expectations."));
							break;
						}
					}
					else
					{
						if (Sampler.Input.Count != Sampler.Output.Count)
						{
							// if any of the channels are corrupt the joint will not receive any of the  animation data
							Payload = "";
							UE_LOG(LogInterchangeImport, Warning, TEXT("GLTF Sampler Corrupt. Input and Output not meeting expectations."));
							break;
						}
					}

					float CurrentStopTime = 0;
					int32 CurrentFrameNumber = 0;

					if (Seconds.Num() > 0)
					{
						//calculate FrameNumber and currentStopTime:
						CurrentStopTime = Seconds[Seconds.Num() - 1];

						float CurrentFrameNumberCandidate = CurrentStopTime / SingleFrameDuration;
						CurrentFrameNumber = int(CurrentFrameNumberCandidate);
						if (int(CurrentFrameNumberCandidate) < CurrentFrameNumberCandidate)
						{
							CurrentFrameNumber++;
						}
						CurrentStopTime = CurrentFrameNumber * SingleFrameDuration;
					}

					if (FrameNumber < CurrentFrameNumber)
					{
						FrameNumber = CurrentFrameNumber;
						StopTime = FrameNumber * SingleFrameDuration;
					}
					Payload += ":" + LexToString(ChannelIndex);
				}

				//set payload:
				if (Payload.Len() > 0)
				{
					TrackNodeAndJointNodeChannels.Key->SetAnimationPayloadKeyForSceneNodeUid(JointNodeUidAndChannelsUsedPair.Key, Payload);
					bHasAnimationPayloadSet = true;
				}
				else
				{
					StopTime = PreviousStopTime;
				}
			}

			//set animation length:
			if (bHasAnimationPayloadSet)
			{
				TrackNodeAndJointNodeChannels.Key->SetCustomAnimationSampleRate(FrameRate);
				TrackNodeAndJointNodeChannels.Key->SetCustomAnimationStartTime(StartTime);
				TrackNodeAndJointNodeChannels.Key->SetCustomAnimationStopTime(StopTime);
			}
		}
	}

	if (NodeChannelsMap.IsEmpty())
	{
		return;
	}

	UInterchangeAnimationTrackSetNode* TrackSetNode = NewObject< UInterchangeAnimationTrackSetNode >(&NodeContainer);

	const FString AnimTrackSetNodeUid = TEXT("\\Animation\\") + UE::Interchange::Gltf::Private::GltfUniqueIdGenerators[GetSourceData()].GetUniqueId(&GltfAnimation);
	TrackSetNode->InitializeNode(AnimTrackSetNodeUid, GltfAnimation.Name, EInterchangeNodeContainerType::TranslatedAsset);

	for (const TTuple<const GLTF::FNode*, TArray<int32>>& NodeChannelsEntry : NodeChannelsMap)
	{
		const TArray<int32>& ChannelIndices = NodeChannelsEntry.Value;
		if (ChannelIndices.Num() == 0)
		{
			continue;
		}

		const GLTF::FNode* GltfNode = NodeChannelsEntry.Key;
		const FString* NodeUid = NodeUidMap.Find(GltfNode);
		if (!ensure(NodeUid))
		{
			continue;
		}

		UInterchangeTransformAnimationTrackNode* TransformAnimTrackNode = NewObject< UInterchangeTransformAnimationTrackNode >(&NodeContainer);

		const FString TransformAnimTrackNodeName = FString::Printf(TEXT("%s_%s"), *GltfNode->Name, *GltfAnimation.Name);
		const FString TransformAnimTrackNodeUid = TEXT("\\AnimationTrack\\") + TransformAnimTrackNodeName;

		TransformAnimTrackNode->InitializeNode(TransformAnimTrackNodeUid, TransformAnimTrackNodeName, EInterchangeNodeContainerType::TranslatedAsset);

		TransformAnimTrackNode->SetCustomActorDependencyUid(*NodeUid);

		FString PayloadKey = FString::FromInt(AnimationIndex);

		constexpr int32 TranslationChannel = 0x0001 | 0x0002 | 0x0004;
		constexpr int32 RotationChannel = 0x0008 | 0x0010 | 0x0020;
		constexpr int32 ScaleChannel = 0x0040 | 0x0080 | 0x0100;

		int32 UsedChannels = 0;

		for (int32 ChannelIndex : ChannelIndices)
		{
			PayloadKey += TEXT(":") + FString::FromInt(ChannelIndex);

			const GLTF::FAnimation::FChannel& Channel = GltfAnimation.Channels[ChannelIndex];

			switch (Channel.Target.Path)
			{
				case GLTF::FAnimation::EPath::Translation:
				{
					UsedChannels |= TranslationChannel;
				} break;

				case GLTF::FAnimation::EPath::Rotation:
				{
					UsedChannels |= RotationChannel;
				} break;

				case GLTF::FAnimation::EPath::Scale:
				{
					UsedChannels |= ScaleChannel;
				} break;

				default: break;
			}
		}

		TransformAnimTrackNode->SetCustomAnimationPayloadKey(PayloadKey);
		TransformAnimTrackNode->SetCustomUsedChannels(UsedChannels);

		NodeContainer.AddNode(TransformAnimTrackNode);

		TrackSetNode->AddCustomAnimationTrackUid(TransformAnimTrackNodeUid);
	}

	NodeContainer.AddNode(TrackSetNode);
}

void UInterchangeGltfTranslator::SetTextureSRGB(UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FTextureMap& TextureMap) const
{
	if (GltfAsset.Textures.IsValidIndex(TextureMap.TextureIndex))
	{
		const FString TextureUid = UInterchangeTextureNode::MakeNodeUid(UE::Interchange::Gltf::Private::GltfUniqueIdGenerators[GetSourceData()].GetUniqueId(&GltfAsset.Textures[TextureMap.TextureIndex]));
		if (UInterchangeTextureNode* TextureNode = const_cast<UInterchangeTextureNode*>(Cast<UInterchangeTextureNode>(NodeContainer.GetNode(TextureUid))))
		{
			TextureNode->SetCustomSRGB(true);
		}
	}
}
void UInterchangeGltfTranslator::SetTextureFlipGreenChannel(UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FTextureMap& TextureMap) const
{
	if (GltfAsset.Textures.IsValidIndex(TextureMap.TextureIndex))
	{
		const FString TextureUid = UInterchangeTextureNode::MakeNodeUid(UE::Interchange::Gltf::Private::GltfUniqueIdGenerators[GetSourceData()].GetUniqueId(&GltfAsset.Textures[TextureMap.TextureIndex]));
		if (UInterchangeTextureNode* TextureNode = const_cast<UInterchangeTextureNode*>(Cast<UInterchangeTextureNode>(NodeContainer.GetNode(TextureUid))))
		{
			TextureNode->SetCustombFlipGreenChannel(true);
		}
	}
}

TFuture<TOptional<UE::Interchange::FVariantSetPayloadData>> UInterchangeGltfTranslator::GetVariantSetPayloadData(const FString& PayloadKey) const
{
	using namespace UE::Interchange;

	TPromise<TOptional<FVariantSetPayloadData>> EmptyPromise;
	EmptyPromise.SetValue(TOptional<FVariantSetPayloadData>());

	TArray<FString> PayloadTokens;

	// We need two indices to build the payload: index of LevelVariantSet and index of VariantSetIndex
	if (GltfAsset.Variants.Num() + 1 != PayloadKey.ParseIntoArray(PayloadTokens, TEXT(";")))
	{
		// Invalid payload
		return EmptyPromise.GetFuture();
	}

	//FString PayloadKey = FileName;
	for (int32 Index = 0; Index < GltfAsset.Variants.Num(); ++Index)
	{
		if (PayloadTokens[Index + 1] != GltfAsset.Variants[Index])
		{
			// Invalid payload
			return EmptyPromise.GetFuture();
		}
	}

	return Async(EAsyncExecution::TaskGraph, [this]
			{
				FVariantSetPayloadData PayloadData;
				TOptional<FVariantSetPayloadData> Result;

				if (this->GetVariantSetPayloadData(PayloadData))
				{
					Result.Emplace(MoveTemp(PayloadData));
				}

				return Result;
			}
		);
}

void UInterchangeGltfTranslator::HandleGltfVariants(UInterchangeBaseNodeContainer& NodeContainer, const FString& FileName) const
{
	UInterchangeVariantSetNode* VariantSetNode = nullptr;
	VariantSetNode = NewObject<UInterchangeVariantSetNode>(&NodeContainer);

	FString VariantSetNodeUid = TEXT("\\VariantSet\\") + FileName;
	VariantSetNode->InitializeNode(VariantSetNodeUid, FileName, EInterchangeNodeContainerType::TranslatedScene);
	NodeContainer.AddNode(VariantSetNode);

	VariantSetNode->SetCustomDisplayText(FileName);

	FString PayloadKey = FileName;
	for (int32 Index = 0; Index < GltfAsset.Variants.Num(); ++Index)
	{
		PayloadKey += TEXT(";") + GltfAsset.Variants[Index];
	}
	VariantSetNode->SetCustomVariantsPayloadKey(PayloadKey);

	TFunction<void(const TArray<int32>& Nodes)> CollectDependencies;
	CollectDependencies = [this, VariantSetNode, &CollectDependencies](const TArray<int32>& Nodes) -> void
	{
		const TArray<GLTF::FMaterial>& Materials = this->GltfAsset.Materials;

		for (const int32 NodeIndex : Nodes)
		{
			if (this->GltfAsset.Nodes.IsValidIndex(NodeIndex))
			{
				const GLTF::FNode& GltfNode = this->GltfAsset.Nodes[NodeIndex];

				if (GltfNode.Type == GLTF::FNode::EType::Mesh && this->GltfAsset.Meshes.IsValidIndex(GltfNode.MeshIndex))
				{
					const GLTF::FMesh& Mesh = this->GltfAsset.Meshes[GltfNode.MeshIndex];
					const FString* NodeUidPtr = this->NodeUidMap.Find(&GltfNode);
					if (!ensure(NodeUidPtr))
					{
						continue;
					}

					VariantSetNode->AddCustomDependencyUid(*NodeUidPtr);

					for (const GLTF::FPrimitive& Primitive : Mesh.Primitives)
					{
						if (Primitive.VariantMappings.Num() > 0)
						{
							for (const GLTF::FVariantMapping& VariantMapping : Primitive.VariantMappings)
							{
								if (!ensure(Materials.IsValidIndex(VariantMapping.MaterialIndex)))
								{
									continue;
								}

								const GLTF::FMaterial& GltfMaterial = Materials[VariantMapping.MaterialIndex];
								const FString MaterialUid = UInterchangeShaderGraphNode::MakeNodeUid(UE::Interchange::Gltf::Private::GltfUniqueIdGenerators[GetSourceData()].GetUniqueId(&GltfMaterial));

								VariantSetNode->AddCustomDependencyUid(MaterialUid);

							}
						}
					}
				}

				if (GltfNode.Children.Num() > 0)
				{
					CollectDependencies(GltfNode.Children);
				}
			}
		}
	};

	for (const GLTF::FScene& GltfScene : GltfAsset.Scenes)
	{
		CollectDependencies(GltfScene.Nodes);
	}

	UInterchangeSceneVariantSetsNode* SceneVariantSetsNode = NewObject<UInterchangeSceneVariantSetsNode>(&NodeContainer);

	FString SceneVariantSetsNodeUid = TEXT("\\SceneVariantSets\\") + FileName;
	SceneVariantSetsNode->InitializeNode(SceneVariantSetsNodeUid, FileName, EInterchangeNodeContainerType::TranslatedScene);
	NodeContainer.AddNode(SceneVariantSetsNode);

	SceneVariantSetsNode->AddCustomVariantSetUid(VariantSetNodeUid);
}

bool UInterchangeGltfTranslator::GetVariantSetPayloadData(UE::Interchange::FVariantSetPayloadData& PayloadData) const
{
	using namespace UE;

	PayloadData.Variants.SetNum(GltfAsset.Variants.Num());

	TMap<const TCHAR*, Interchange::FVariant*> VariantMap;
	VariantMap.Reserve(GltfAsset.Variants.Num());

	for (int32 VariantIndex = 0; VariantIndex < GltfAsset.Variants.Num(); ++VariantIndex)
	{
		const FString& VariantName = GltfAsset.Variants[VariantIndex];
		PayloadData.Variants[VariantIndex].DisplayText = VariantName;
		VariantMap.Add(*VariantName, &PayloadData.Variants[VariantIndex]);
	}

	TFunction<void(const TArray<int32>& Nodes)> BuildPayloadData;
	BuildPayloadData = [this, VariantMap = MoveTemp(VariantMap), &BuildPayloadData](const TArray<int32>& Nodes) -> void
	{
		const TArray<FString>& VariantNames = this->GltfAsset.Variants;
		const TArray<GLTF::FMaterial>& Materials = this->GltfAsset.Materials;

		for (const int32 NodeIndex : Nodes)
		{
			if (!ensure(this->GltfAsset.Nodes.IsValidIndex(NodeIndex)))
			{
				continue;
			}

			const GLTF::FNode& GltfNode = this->GltfAsset.Nodes[NodeIndex];

			if (GltfNode.Type == GLTF::FNode::EType::Mesh && this->GltfAsset.Meshes.IsValidIndex(GltfNode.MeshIndex))
			{
				const GLTF::FMesh& Mesh = this->GltfAsset.Meshes[GltfNode.MeshIndex];
				const FString* NodeUidPtr = this->NodeUidMap.Find(&GltfNode);
				if (!ensure(NodeUidPtr))
				{
					continue;
				}

				for (const GLTF::FPrimitive& Primitive : Mesh.Primitives)
				{
					for (const GLTF::FVariantMapping& VariantMapping : Primitive.VariantMappings)
					{
						if (!ensure(Materials.IsValidIndex(VariantMapping.MaterialIndex)))
						{
							continue;
						}

						const GLTF::FMaterial& GltfMaterial = Materials[VariantMapping.MaterialIndex];
						const FString MaterialUid = UInterchangeShaderGraphNode::MakeNodeUid(UE::Interchange::Gltf::Private::GltfUniqueIdGenerators[GetSourceData()].GetUniqueId(&GltfMaterial));

						for (int32 VariantIndex : VariantMapping.VariantIndices)
						{
							if (!ensure(VariantMap.Contains(*VariantNames[VariantIndex])))
							{
								continue;
							}

							// This is on par with the Datasmith GLTF translator but might be wrong.
							// Each primitive should be a section of the static mesh and 
							// TODO: Revisit creation of static mesh and handling of variants: UE-159945.
							Interchange::FVariantPropertyCaptureData PropertyCaptureData;

							PropertyCaptureData.Category = Interchange::EVariantPropertyCaptureCategory::Material;
							PropertyCaptureData.ObjectUid = MaterialUid;

							Interchange::FVariant& VariantData = *(VariantMap[*VariantNames[VariantIndex]]);

							Interchange::FVariantBinding& Binding = VariantData.Bindings.AddDefaulted_GetRef();

							Binding.TargetUid = *NodeUidPtr;
							Binding.Captures.Add(PropertyCaptureData);
						}

					}
				}
			}

			if (GltfNode.Children.Num() > 0)
			{
				BuildPayloadData(GltfNode.Children);
			}
		}
	};

	for (const GLTF::FScene& GltfScene : GltfAsset.Scenes)
	{
		BuildPayloadData(GltfScene.Nodes);
	}

	return true;
}

TFuture<TOptional<UE::Interchange::FSkeletalMeshLodPayloadData>> UInterchangeGltfTranslator::GetSkeletalMeshLodPayloadData(const FString& PayLoadKey) const
{
	return Async(EAsyncExecution::TaskGraph, [this, PayLoadKey]
		{
			TOptional<UE::Interchange::FSkeletalMeshLodPayloadData> Result;
			UE::Interchange::FSkeletalMeshLodPayloadData SkeletalMeshPayloadData;
			
			if (UE::Interchange::Gltf::Private::GetSkeletalMeshDescriptionForPayLoadKey(GltfAsset, PayLoadKey, SkeletalMeshPayloadData.LodMeshDescription, &SkeletalMeshPayloadData.JointNames))
			{
				Result.Emplace(SkeletalMeshPayloadData);
			}

			return Result;
		});
}

TFuture<TOptional<UE::Interchange::FSkeletalMeshMorphTargetPayloadData>> UInterchangeGltfTranslator::GetSkeletalMeshMorphTargetPayloadData(const FString& PayLoadKey) const
{
	TSharedPtr<TPromise<TOptional<UE::Interchange::FSkeletalMeshMorphTargetPayloadData>>> Promise = MakeShared<TPromise<TOptional<UE::Interchange::FSkeletalMeshMorphTargetPayloadData>>>();
	Promise->SetValue(TOptional<UE::Interchange::FSkeletalMeshMorphTargetPayloadData>{});
	return Promise->GetFuture();
}

void UInterchangeGltfTranslator::HandleGltfSkeletons(UInterchangeBaseNodeContainer& NodeContainer, const FString& SceneNodeUid, const TArray<int32>& SkinnedMeshNodes, TSet<int>& UnusedMeshIndices) const
{
	TMap<int32, TMap<int32, TArray<int32>>> MeshIndexToRootJointGroupedSkinnedMeshNodesMap;

	//group SkinnedMeshNodes based on Joint Root Parents and Mesh indices
	//this is needed in order to figure out how many duplications do we need for a given mesh
	for (int32 SkinnedMeshNodeIndex : SkinnedMeshNodes)
	{
		const GLTF::FNode& SkinnedMeshNode = GltfAsset.Nodes[SkinnedMeshNodeIndex];

		TMap<int32, TArray<int32>>& RootJointGroupedSkinnedMeshNodes = MeshIndexToRootJointGroupedSkinnedMeshNodesMap.FindOrAdd(SkinnedMeshNode.MeshIndex);

		//get the SkinnedMeshNode's skin's first joint as the starting ground and find the top most root joint for it:
		if (!GltfAsset.Skins.IsValidIndex(SkinnedMeshNode.Skindex)
			|| !GltfAsset.Skins[SkinnedMeshNode.Skindex].Joints.IsValidIndex(0)
			|| !GltfAsset.Nodes.IsValidIndex(GltfAsset.Skins[SkinnedMeshNode.Skindex].Joints[0]))
		{
			continue;
		}
		
		int32 RootJointIndex = GltfAsset.Nodes[GltfAsset.Skins[SkinnedMeshNode.Skindex].Joints[0]].RootJointIndex;
		if (!GltfAsset.Nodes.IsValidIndex(RootJointIndex))
		{
			continue;
		}

		//basedon that root joint group the SkinnedMeshNodes:
		TArray<int32>& GroupedSkinnedMeshNodes = RootJointGroupedSkinnedMeshNodes.FindOrAdd(RootJointIndex);
		GroupedSkinnedMeshNodes.Add(SkinnedMeshNodeIndex);
	}

	for (const TTuple<int32, TMap<int32, TArray<int32>>>& MeshIndexRootJointGroups : MeshIndexToRootJointGroupedSkinnedMeshNodesMap)
	{
		const TMap<int32, TArray<int32>>& RootJointGroupedSkinnedMeshNodes = MeshIndexRootJointGroups.Value;

		int MeshIndex = MeshIndexRootJointGroups.Key;

		//iterate through the groups:
		//rootjoint , array<skinnedMeshNodes>
		for (const TTuple<int32, TArray<int32>>& RootJointToSkinnedMeshNodes : RootJointGroupedSkinnedMeshNodes)
		{
			//Duplicate MeshNode for each group:
			int32 RootJointIndex = RootJointToSkinnedMeshNodes.Key;

			//Skeletal Mesh's naming policy: (Mesh.Name)_(RootJointNode.Name) naming policy:
			FString SkeletalName = GltfAsset.Meshes[MeshIndex].Name + TEXT("_") + GltfAsset.Nodes[RootJointIndex].Name;
			FString SkeletalId = UE::Interchange::Gltf::Private::GltfUniqueIdGenerators[GetSourceData()].GetUniqueId(&GltfAsset.Meshes[MeshIndex]) + TEXT("_") + UE::Interchange::Gltf::Private::GltfUniqueIdGenerators[GetSourceData()].GetUniqueId(&GltfAsset.Nodes[RootJointIndex]);

			UInterchangeMeshNode* SkeletalMeshNode = HandleGltfMesh(NodeContainer, GltfAsset.Meshes[MeshIndex], MeshIndex, UnusedMeshIndices, SkeletalId);

			SkeletalMeshNode->SetSkinnedMesh(true);

			//generate payload key:
			//of template:
			//"LexToString(SkinnedMeshNode.MeshIndex | (SkinnedMeshNode.Skindex << 16))":"LexToString(SkinnedMeshNode.MeshIndex | (SkinnedMeshNode.Skindex << 16))".....
			FString Payload = "";
			for (int32 SkinnedMeshIndex : RootJointToSkinnedMeshNodes.Value)
			{
				const GLTF::FNode& SkinnedMeshNode = GltfAsset.Nodes[SkinnedMeshIndex];
				if (Payload.Len() > 0)
				{
					Payload += ":";
				}
				Payload += LexToString(SkinnedMeshNode.MeshIndex | (SkinnedMeshNode.Skindex << 16));
			}
			SkeletalMeshNode->SetPayLoadKey(Payload);

			//set the root joint node as the skeletonDependency:
			int32 RootJointNodeIndex = RootJointToSkinnedMeshNodes.Key;
			const GLTF::FNode& RootJointNode = GltfAsset.Nodes[RootJointNodeIndex];
			const FString* SkeletonNodeUid = NodeUidMap.Find(&RootJointNode);
			if (ensure(SkeletonNodeUid))
			{
				SkeletalMeshNode->SetSkeletonDependencyUid(*SkeletonNodeUid);
			}
				

			//set the mesh actor node's custom asset instance uid to the new duplicated mesh
			//if there are more than one skins, then choose the topmost (root node of the collection, top most in a hierarchical tree term) occurance of SkinnedMeshIndex
			int32 MeshActorNodeIndex = UE::Interchange::Gltf::Private::GetRootNodeIndex(GltfAsset, RootJointToSkinnedMeshNodes.Value);
			const GLTF::FNode& MeshActorNode = GltfAsset.Nodes[MeshActorNodeIndex];
			const FString* SceneMeshActorNodeUid = NodeUidMap.Find(&MeshActorNode);
			if (const UInterchangeSceneNode* ConstSceneMeshActorNode = Cast< UInterchangeSceneNode >(NodeContainer.GetNode(*SceneMeshActorNodeUid)))
			{
				UInterchangeSceneNode* SceneMeshNode = const_cast<UInterchangeSceneNode*>(ConstSceneMeshActorNode);
				if (ensure(SceneMeshNode))
				{
					SceneMeshNode->SetCustomAssetInstanceUid(SkeletalMeshNode->GetUniqueID());
				}
			}

			NodeContainer.AddNode(SkeletalMeshNode);
		}
	}
}

UInterchangeMeshNode* UInterchangeGltfTranslator::HandleGltfMesh(UInterchangeBaseNodeContainer& NodeContainer, 
	const GLTF::FMesh& GltfMesh, int MeshIndex, 
	TSet<int>& UnusedMeshIndices, 
	const FString& SkeletalName/*If set it creates the mesh even if it was already created (for Skeletals)*/) const
{
	if (!UnusedMeshIndices.Contains(MeshIndex) && SkeletalName.Len() == 0)
	{
		return nullptr;
	}
	FString MeshName = SkeletalName.Len() ? SkeletalName : GltfMesh.Name;
	UnusedMeshIndices.Remove(MeshIndex);

	UInterchangeMeshNode* MeshNode = NewObject< UInterchangeMeshNode >(&NodeContainer);
	FString MeshNodeUid = TEXT("\\Mesh\\") + (SkeletalName.Len() ? SkeletalName : UE::Interchange::Gltf::Private::GltfUniqueIdGenerators[GetSourceData()].GetUniqueId(&GltfMesh));

	MeshNode->InitializeNode(MeshNodeUid, MeshName, EInterchangeNodeContainerType::TranslatedAsset);
	MeshNode->SetPayLoadKey(LexToString(MeshIndex));

	NodeContainer.AddNode(MeshNode);

	for (int32 PrimitiveCounter = 0; PrimitiveCounter < GltfMesh.Primitives.Num(); PrimitiveCounter++)
	{
		const GLTF::FPrimitive& Primitive = GltfMesh.Primitives[PrimitiveCounter];

		// Assign materials
		if (GltfAsset.Materials.IsValidIndex(Primitive.MaterialIndex))
		{
			const FString MaterialName = GltfAsset.Materials[Primitive.MaterialIndex].Name;
			const FString ShaderGraphNodeUid = UInterchangeShaderGraphNode::MakeNodeUid(UE::Interchange::Gltf::Private::GltfUniqueIdGenerators[GetSourceData()].GetUniqueId(&GltfAsset.Materials[Primitive.MaterialIndex]));
			MeshNode->SetSlotMaterialDependencyUid(MaterialName, ShaderGraphNodeUid);
		}
	}

	return MeshNode;
}

#undef LOCTEXT_NAMESPACE