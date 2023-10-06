// Copyright Epic Games, Inc. All Rights Reserved. 

#include "Gltf/InterchangeGltfTranslator.h"

#include "GLTFAccessor.h"
#include "GLTFAnimation.h"
#include "GLTFAsset.h"
#include "GLTFMesh.h"
#include "GLTFMeshFactory.h"
#include "GLTFNode.h"
#include "GLTFReader.h"
#include "GLTFTexture.h"

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
#include "Gltf/InterchangeGLTFMaterial.h"

#include "EngineAnalytics.h"
#include "Engine/RendererSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeGltfTranslator)

#define LOCTEXT_NAMESPACE "InterchangeGltfTranslator"

static const TArray<FString> ImporterSupportedExtensions = {
	/* Lights */
	GLTF::ToString(GLTF::EExtension::KHR_LightsPunctual),
	GLTF::ToString(GLTF::EExtension::KHR_Lights),
	/* Variants */
	GLTF::ToString(GLTF::EExtension::KHR_MaterialsVariants),
	/* Materials */
	GLTF::ToString(GLTF::EExtension::KHR_MaterialsUnlit),
	GLTF::ToString(GLTF::EExtension::KHR_MaterialsIOR),
	GLTF::ToString(GLTF::EExtension::KHR_MaterialsClearCoat),
	GLTF::ToString(GLTF::EExtension::KHR_MaterialsTransmission),
	GLTF::ToString(GLTF::EExtension::KHR_MaterialsSheen),
	GLTF::ToString(GLTF::EExtension::KHR_MaterialsSpecular),
	GLTF::ToString(GLTF::EExtension::KHR_MaterialsPbrSpecularGlossiness),
	GLTF::ToString(GLTF::EExtension::KHR_MaterialsEmissiveStrength),
	GLTF::ToString(GLTF::EExtension::MSFT_PackingOcclusionRoughnessMetallic),
	GLTF::ToString(GLTF::EExtension::MSFT_PackingNormalRoughnessMetallic),
	/* Textures */
	GLTF::ToString(GLTF::EExtension::KHR_TextureTransform),

	/* Mesh */
	GLTF::ToString(GLTF::EExtension::KHR_MeshQuantization)
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
			Node.LocalBindPose.SetTranslation(Node.LocalBindPose.GetTranslation() * Scale);
		}
	}

	enum TranslationResult : int32
	{
		SUCCESSFULL = 0,
		INPUT_FILE_NOTFOUND,
		GLTFREADER_FAILED,
		NOTSUPPORTED_EXTENSION_FOUND
	};
	void SendAnalytics(const TranslationResult& TranslationResult,
		const TArray<FString>& ExtensionsUsed = TArray<FString>(),
		const TArray<FString>& ExtensionsRequired = TArray<FString>(),
		GLTF::FMetadata Metadata = GLTF::FMetadata(),
		const FString& GLTFReaderLogMessage = "")
	{
		if (FEngineAnalytics::IsAvailable())
		{
			TMap<FString, FString> MetadataExtras;
			for (GLTF::FMetadata::FExtraData ExtraData : Metadata.Extras)
			{
				MetadataExtras.Add(ExtraData.Name, ExtraData.Value);
			}

			TSet<FString> AllExtensions;
			AllExtensions.Append(ExtensionsUsed);
			AllExtensions.Append(ExtensionsRequired);

			TArray<FString> ExtensionsSupported;
			TArray<FString> ExtensionsUnsupported;

			for (const FString& Extension : AllExtensions)
			{
				if (ImporterSupportedExtensions.Find(Extension) == INDEX_NONE)
				{
					ExtensionsUnsupported.Add(Extension);
				}
				else
				{
					ExtensionsSupported.Add(Extension);
				}
			}

			TArray<FAnalyticsEventAttribute> GLTFAnalytics;
			if (ExtensionsUsed.Num() > 0)					GLTFAnalytics.Add(FAnalyticsEventAttribute(TEXT("ExtensionsUsed"), ExtensionsUsed));
			if (ExtensionsRequired.Num() > 0)				GLTFAnalytics.Add(FAnalyticsEventAttribute(TEXT("ExtensionsRequired"), ExtensionsRequired));
			if (ExtensionsSupported.Num() > 0)				GLTFAnalytics.Add(FAnalyticsEventAttribute(TEXT("ExtensionsSupported"), ExtensionsSupported));
			if (ExtensionsUnsupported.Num() > 0)			GLTFAnalytics.Add(FAnalyticsEventAttribute(TEXT("ExtensionsUnsupported"), ExtensionsUnsupported));
			if (Metadata.GeneratorName.Len() > 0)			GLTFAnalytics.Add(FAnalyticsEventAttribute(TEXT("MetaData.GeneratorName"), Metadata.GeneratorName));
			if (MetadataExtras.Num() > 0)					GLTFAnalytics.Add(FAnalyticsEventAttribute(TEXT("MetaData.Extras"), MetadataExtras));
			/*Version is always set at this point.*/		GLTFAnalytics.Add(FAnalyticsEventAttribute(TEXT("MetaData.Version"), Metadata.Version));

			switch (TranslationResult)
			{
			case SUCCESSFULL:
				GLTFAnalytics.Add(FAnalyticsEventAttribute(TEXT("TranslationStatus"), "Successful."));
				break;
			case INPUT_FILE_NOTFOUND:
				GLTFAnalytics.Add(FAnalyticsEventAttribute(TEXT("TranslationStatus"), "[Failed] Input File Not Found."));
				break;
			case GLTFREADER_FAILED:
				GLTFAnalytics.Add(FAnalyticsEventAttribute(TEXT("TranslationStatus"), "[Failed] Parsing error."));
				break;
			case NOTSUPPORTED_EXTENSION_FOUND:
				GLTFAnalytics.Add(FAnalyticsEventAttribute(TEXT("TranslationStatus"), "[Failed] Unsupported Extension Found."));
				break;
			default:
				break;
			}

			//Send Analytics
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Interchange.Usage.Import.GLTF"), GLTFAnalytics);
		}
	};

	void AcquireJointsWithBindPose(const TArray<GLTF::FNode>& GLTFNodes,int32 CurrentIndex, TSet<int32>& Joints)
	{
		if (GLTFNodes.IsValidIndex(CurrentIndex))
		{
			const GLTF::FNode& CurrentNode = GLTFNodes[CurrentIndex];
			if (CurrentNode.Type == GLTF::FNode::EType::Joint)
			{
				if (CurrentNode.bHasLocalBindPose)
				{
					Joints.Add(CurrentIndex);
				}

				for (int32 ChildIndex : CurrentNode.Children)
				{
					AcquireJointsWithBindPose(GLTFNodes, ChildIndex, Joints);
				}
			}
		}
	}
}

void UInterchangeGLTFTranslator::HandleGltfNode( UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FNode& GltfNode, const FString& ParentNodeUid, const int32 NodeIndex, 
	bool &bHasVariants, TArray<int32>& SkinnedMeshNodes, TSet<int>& UnusedMeshIndices ) const
{
	using namespace UE::Interchange::Gltf::Private;

	const FString NodeUid = ParentNodeUid + TEXT("\\") + GltfNode.UniqueId;

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

			{//Set Morph Target Curve Weights
				const TArray<FString>& MorphTargetNames = GltfAsset.Meshes[GltfNode.MeshIndex].MorphTargetNames;
				int32 MorphTargetNamesCount = MorphTargetNames.Num();
				const TArray<float>& MorphTargetWeights = (GltfNode.MorphTargetWeights.Num() > 0) ? GltfNode.MorphTargetWeights : GltfAsset.Meshes[GltfNode.MeshIndex].MorphTargetWeights;

				if (MorphTargetWeights.Num() == MorphTargetNamesCount)
				{
					for (int32 MorphTargetIndex = 0; MorphTargetIndex < MorphTargetNamesCount; MorphTargetIndex++)
					{
						InterchangeSceneNode->SetMorphTargetCurveWeight(MorphTargetNames[MorphTargetIndex], MorphTargetWeights[MorphTargetIndex]);
					}
				}
				else
				{
					UE_LOG(LogInterchangeImport, Warning, TEXT("GLTF Node [%d] Import Warning. Gltf Node's MorphTargetNames count is missmatched against MorphTargetWeights count."), *GltfNode.UniqueId);
				}
			}

			break;
		}

		case GLTF::FNode::EType::Joint:
		{
			InterchangeSceneNode->AddSpecializedType(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString());
			if (GltfNode.bHasLocalBindPose)
			{
				InterchangeSceneNode->SetCustomBindPoseLocalTransform(&NodeContainer, GltfNode.LocalBindPose);
			}
			break;
		}

		case GLTF::FNode::EType::Mesh:
		{
			if ( GltfAsset.Meshes.IsValidIndex( GltfNode.MeshIndex ) )
			{
				UInterchangeMeshNode* MeshNode = HandleGltfMesh(NodeContainer, GltfAsset.Meshes[GltfNode.MeshIndex], GltfNode.MeshIndex, UnusedMeshIndices);

				InterchangeSceneNode->SetCustomAssetInstanceUid( MeshNode->GetUniqueID() );
				if (GltfAsset.Meshes[GltfNode.MeshIndex].MorphTargetNames.Num() > 0)
				{
					const TArray<FString>& MorphTargetNames = GltfAsset.Meshes[GltfNode.MeshIndex].MorphTargetNames;
					int32 MorphTargetNamesCount = MorphTargetNames.Num();
					const TArray<float>& MorphTargetWeights = (GltfNode.MorphTargetWeights.Num() > 0) ? GltfNode.MorphTargetWeights : GltfAsset.Meshes[GltfNode.MeshIndex].MorphTargetWeights;
					
					if (MorphTargetWeights.Num() == MorphTargetNamesCount)
					{
						for (int32 MorphTargetIndex = 0; MorphTargetIndex < MorphTargetNamesCount; MorphTargetIndex++)
						{
							InterchangeSceneNode->SetMorphTargetCurveWeight(MorphTargetNames[MorphTargetIndex], MorphTargetWeights[MorphTargetIndex]);
						}
					}
					else
					{
						UE_LOG(LogInterchangeImport, Warning, TEXT("GLTF Node [%d] Import Warning. Gltf Node's MorphTargetNames count is missmatched against MorphTargetWeights count."), *GltfNode.UniqueId);
					}
				}

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
				const FString CameraNodeUid = TEXT("\\Camera\\") + GltfAsset.Cameras[ GltfNode.CameraIndex ].UniqueId;
				InterchangeSceneNode->SetCustomAssetInstanceUid( CameraNodeUid );
			}
			break;
		}

		case GLTF::FNode::EType::Light:
		{
			Transform.ConcatenateRotation(FRotator(0, -90, 0).Quaternion());

			if ( GltfAsset.Lights.IsValidIndex( GltfNode.LightIndex ) )
			{
				const FString LightNodeUid = TEXT("\\Light\\") + GltfAsset.Lights[ GltfNode.LightIndex ].UniqueId;
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

EInterchangeTranslatorType UInterchangeGLTFTranslator::GetTranslatorType() const
{
	return EInterchangeTranslatorType::Scenes;
}

EInterchangeTranslatorAssetType UInterchangeGLTFTranslator::GetSupportedAssetTypes() const
{
	//gltf translator support Meshes and Materials
	return EInterchangeTranslatorAssetType::Materials | EInterchangeTranslatorAssetType::Meshes | EInterchangeTranslatorAssetType::Animations;
}

TArray<FString> UInterchangeGLTFTranslator::GetSupportedFormats() const
{
	TArray<FString> GltfExtensions;

	GltfExtensions.Reserve(2);
	GltfExtensions.Add(TEXT("gltf;GL Transmission Format"));
	GltfExtensions.Add(TEXT("glb;GL Transmission Format (Binary)"));

	return GltfExtensions;
}

bool UInterchangeGLTFTranslator::Translate( UInterchangeBaseNodeContainer& NodeContainer ) const
{
	using namespace UE::Interchange::Gltf::Private;

	FString FilePath = GetSourceData()->GetFilename();
	if ( !FPaths::FileExists( FilePath ) )
	{
		SendAnalytics(TranslationResult::INPUT_FILE_NOTFOUND);
		return false;
	}
	
	GLTF::FFileReader GltfFileReader;

	const bool bLoadImageData = false;
	const bool bLoadMetaData = false;
	GltfFileReader.ReadFile( FilePath, bLoadImageData, bLoadMetaData, const_cast< UInterchangeGLTFTranslator* >( this )->GltfAsset );

	const FString FileName = GltfAsset.Name;

	//Required Extension Check:
	TArray<FString> NotSupportedRequiredExtensions;
	if (GltfAsset.ExtensionsRequired.Num() != 0)
	{
		for (const FString& RequiredExtension : GltfAsset.ExtensionsRequired)
		{
			if (ImporterSupportedExtensions.Find(RequiredExtension) == INDEX_NONE)
			{
				NotSupportedRequiredExtensions.Add(RequiredExtension);
			}
		}
	}

	//Check if ReadFile failed:
	TArray<GLTF::FLogMessage> GLTFReadFileLogMessages = GltfFileReader.GetLogMessages();
	for (GLTF::FLogMessage& LogMessage : GLTFReadFileLogMessages)
	{
		if (LogMessage.Key == GLTF::EMessageSeverity::Error)
		{
			UInterchangeResultError_Generic* ErrorResult = AddMessage< UInterchangeResultError_Generic >();
			ErrorResult->SourceAssetName = FileName;
			ErrorResult->Text = FText::Format(LOCTEXT("GLTF::FFileReader::ReadFile Failed.", "LogMessage: {0}"), FText::FromString(LogMessage.Value));

			SendAnalytics(TranslationResult::GLTFREADER_FAILED, GltfAsset.ExtensionsUsed,GltfAsset.ExtensionsRequired, GltfAsset.Metadata, LogMessage.Value);
			return false;
		}
	}

	//In case of non supported extensions fail out:
	if (NotSupportedRequiredExtensions.Num() > 0)
	{
		FString NotSupportedRequiredExtensionsStringified;
		for (const FString& NotSupportedExtension : NotSupportedRequiredExtensions)
		{
			if (NotSupportedRequiredExtensionsStringified.Len() > 0)
			{
				NotSupportedRequiredExtensionsStringified += ", ";
			}
			NotSupportedRequiredExtensionsStringified += NotSupportedExtension;
		}

		UInterchangeResultError_Generic* ErrorResult = AddMessage< UInterchangeResultError_Generic >();
		ErrorResult->SourceAssetName = FileName;
		ErrorResult->Text = FText::Format(
			LOCTEXT("UnsupportedRequiredExtensions", "Not All Required Extensions are supported. (Unsupported extensions: {0})"),
			FText::FromString(NotSupportedRequiredExtensionsStringified));

		SendAnalytics(TranslationResult::NOTSUPPORTED_EXTENSION_FOUND, GltfAsset.ExtensionsUsed, GltfAsset.ExtensionsRequired, GltfAsset.Metadata);
		return false;
	}

	ScaleNodeTranslations(const_cast<UInterchangeGLTFTranslator*>(this)->GltfAsset.Nodes, GltfUnitConversionMultiplier);

	//Check Normal Textures:
	TSet<int32> NormalTextureIndices;
	{
		auto AddTextureIndex = [&](int32 TextureIndex)
		{
			if (GltfAsset.Textures.IsValidIndex(TextureIndex))
			{
				NormalTextureIndices.Add(TextureIndex);
			}
		};

		for (const GLTF::FMaterial& GltfMaterial : GltfAsset.Materials)
		{
			AddTextureIndex(GltfMaterial.Normal.TextureIndex);
			AddTextureIndex(GltfMaterial.ClearCoat.NormalMap.TextureIndex);
		}
	}

	// Textures
	{
		int32 TextureIndex = 0;
		for ( const GLTF::FTexture& GltfTexture : GltfAsset.Textures )
		{
			// The glTF reader enforces the spec on the image format for buffers, URIs and file paths
			// Skip the texture is the glTF reader has not recognized the format
			if (GltfTexture.Source.Format == GLTF::FImage::EFormat::Unknown)
			{
				UInterchangeResultError_Generic* Message = AddMessage<UInterchangeResultError_Generic>();

				FText TextMessage;
				if (GltfTexture.Source.FilePath.IsEmpty())
				{
					Message->Text = FText::Format(LOCTEXT("TextureCreationFailed", "The image format of the buffer for texture {0} is not supported."), FText::FromString(GltfTexture.Name));
				}
				else
				{
					Message->SourceAssetName = GetSourceData()->GetFilename();
					Message->Text = FText::Format(
						LOCTEXT("TextureCreationFailedFromFile", "The extension of the image file, {0}, for texture {1} is not supported."),
						FText::FromString(GltfTexture.Source.FilePath), FText::FromString(GltfTexture.Name));
				}

				continue;
			}

			UInterchangeTexture2DNode* TextureNode = UInterchangeTexture2DNode::Create(&NodeContainer, GltfTexture.UniqueId);
			TextureNode->SetDisplayLabel(GltfTexture.Name);

			TextureNode->SetCustomFilter(ConvertFilter(GltfTexture.Sampler.MinFilter));

			bool TextureUsedAsNormal = NormalTextureIndices.Contains(TextureIndex);

			if (TextureUsedAsNormal)
			{
				//According to GLTF documentation the normal maps are right handed (following OpenGL convention),
				//however UE expects left handed normal maps, this can be resolved by flipping the green channel of the normal textures:
				//(based on https://github.com/KhronosGroup/glTF-Sample-Models/tree/master/2.0/NormalTangentTest#problem-flipped-y-axis-or-flipped-green-channel)
				TextureNode->SetCustombFlipGreenChannel(true);
			}

			FString Payloadkey = LexToString(TextureIndex++) + TEXT(":") + LexToString(TextureUsedAsNormal);
			TextureNode->SetPayLoadKey(Payloadkey);

			TextureNode->SetCustomWrapU( UE::Interchange::Gltf::Private::ConvertWrap( GltfTexture.Sampler.WrapS ) );
			TextureNode->SetCustomWrapV( UE::Interchange::Gltf::Private::ConvertWrap( GltfTexture.Sampler.WrapT ) );
		}
	}

	// Meshes
	TSet<FString> MaterialsUsedOnMeshesWithVertexColor;
	TSet<int> UnusedGltfMeshIndices;
	{
		int32 MeshIndex = 0;
		for (const GLTF::FMesh& GltfMesh : GltfAsset.Meshes)
		{
			UnusedGltfMeshIndices.Add(MeshIndex++);

			if (GltfMesh.HasColors())
			{
				for (int32 PrimitiveCounter = 0; PrimitiveCounter < GltfMesh.Primitives.Num(); PrimitiveCounter++)
				{
					const GLTF::FPrimitive& Primitive = GltfMesh.Primitives[PrimitiveCounter];

					if (GltfAsset.Materials.IsValidIndex(Primitive.MaterialIndex))
					{
						const FString ShaderGraphNodeUid = UInterchangeShaderGraphNode::MakeNodeUid(GltfAsset.Materials[Primitive.MaterialIndex].UniqueId);
						MaterialsUsedOnMeshesWithVertexColor.Add(ShaderGraphNodeUid);
					}
				}
			}
		}
	}

	// Materials
	{
		int32 MaterialIndex = 0;
		for ( const GLTF::FMaterial& GltfMaterial : GltfAsset.Materials )
		{
			//Based on the gltf specification the basecolor and emissive textures have SRGB colors:
			SetTextureSRGB(NodeContainer, GltfMaterial.BaseColor, true);
			SetTextureSRGB(NodeContainer, GltfMaterial.Emissive, true);
			//Textures that are expected to use Scalar outputs we want to set them as SRGB false explicitly, based on UInterchangeGenericMaterialPipeline::HandleTextureNode
			SetTextureSRGB(NodeContainer, GltfMaterial.MetallicRoughness.Map, false);
			SetTextureSRGB(NodeContainer, GltfMaterial.Occlusion, false);
			SetTextureSRGB(NodeContainer, GltfMaterial.ClearCoat.ClearCoatMap, false);
			SetTextureSRGB(NodeContainer, GltfMaterial.ClearCoat.RoughnessMap, false);
			SetTextureSRGB(NodeContainer, GltfMaterial.Transmission.TransmissionMap, false);

			const FString ShaderGraphNodeUid = UInterchangeShaderGraphNode::MakeNodeUid(GltfMaterial.UniqueId);
			bool bUseVertexColor = MaterialsUsedOnMeshesWithVertexColor.Contains(ShaderGraphNodeUid);

			UInterchangeShaderGraphNode* ShaderGraphNode = UInterchangeShaderGraphNode::Create(&NodeContainer, GltfMaterial.UniqueId);
			ShaderGraphNode->SetDisplayLabel(GltfMaterial.Name);

			UE::Interchange::GLTFMaterials::HandleGltfMaterial(NodeContainer, GltfMaterial, GltfAsset.Textures, ShaderGraphNode);
			
			++MaterialIndex;
		}
	}

	// Cameras
	{
		int32 CameraIndex = 0;
		for ( const GLTF::FCamera& GltfCamera : GltfAsset.Cameras )
		{
			UInterchangeStandardCameraNode* CameraNode = NewObject< UInterchangeStandardCameraNode >(&NodeContainer);
			FString CameraNodeUid = TEXT("\\Camera\\") + GltfCamera.UniqueId;
			CameraNode->InitializeNode(CameraNodeUid, GltfCamera.Name, EInterchangeNodeContainerType::TranslatedAsset);

			if (GltfCamera.bIsPerspective)
			{
				CameraNode->SetCustomProjectionMode(EInterchangeCameraProjectionType::Perspective);

				CameraNode->SetCustomFieldOfView(FMath::RadiansToDegrees(GltfCamera.Perspective.Fov));
				CameraNode->SetCustomAspectRatio(GltfCamera.Perspective.AspectRatio);
			}
			else
			{
				CameraNode->SetCustomProjectionMode(EInterchangeCameraProjectionType::Orthographic);

				CameraNode->SetCustomWidth(GltfCamera.Orthographic.XMagnification * GltfUnitConversionMultiplier);
				CameraNode->SetCustomNearClipPlane(GltfCamera.ZNear * GltfUnitConversionMultiplier);
				CameraNode->SetCustomFarClipPlane(GltfCamera.ZFar * GltfUnitConversionMultiplier);

				CameraNode->SetCustomAspectRatio(GltfCamera.Orthographic.XMagnification / GltfCamera.Orthographic.YMagnification);
			}

			NodeContainer.AddNode(CameraNode);
			++CameraIndex;
		}
	}

	// Lights
	{
		int32 LightIndex = 0;
		for ( const GLTF::FLight& GltfLight : GltfAsset.Lights )
		{
			FString LightNodeUid = TEXT("\\Light\\") + GltfLight.UniqueId;

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

			FString SceneNodeUid = TEXT("\\Scene\\") + GltfScene.UniqueId;
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
			{
				UInterchangeResultWarning_Generic* WarningResult = AddMessage< UInterchangeResultWarning_Generic >();
				WarningResult->Text = FText::FromString(LogMessage.Get<1>());
				Result = WarningResult;
			}
			break;
		case GLTF::EMessageSeverity::Display:
			{
				UInterchangeResultDisplay_Generic* DisplayResult = AddMessage< UInterchangeResultDisplay_Generic >();
				DisplayResult->Text = FText::FromString(LogMessage.Get<1>());
				Result = DisplayResult;
			}
			break;
		default:
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

	SendAnalytics(TranslationResult::SUCCESSFULL, GltfAsset.ExtensionsUsed, GltfAsset.ExtensionsRequired, GltfAsset.Metadata);
	return true;
}

TOptional< UE::Interchange::FImportImage > UInterchangeGLTFTranslator::GetTexturePayloadData(const FString& PayloadKey, TOptional<FString>& AlternateTexturePath) const
{
	TArray<FString> PayloadKeys;
	PayloadKey.ParseIntoArray(PayloadKeys, TEXT(":"));

	if (PayloadKeys.Num() == 0)
	{
		return TOptional< UE::Interchange::FImportImage >();
	}

	int32 TextureIndex = 0;
	LexFromString( TextureIndex, *PayloadKeys[0]);

	if ( !GltfAsset.Textures.IsValidIndex( TextureIndex ) )
	{
		return TOptional< UE::Interchange::FImportImage >();
	}

	const GLTF::FTexture& GltfTexture = GltfAsset.Textures[ TextureIndex ];

	TOptional<UE::Interchange::FImportImage> TexturePayloadData = TOptional<UE::Interchange::FImportImage>();

	if (GltfTexture.Source.FilePath.IsEmpty())
	{
		// Embedded texture -- try using ImageWrapper to decode it
		TArray64<uint8> ImageData(GltfTexture.Source.Data, GltfTexture.Source.DataByteLength);
		UInterchangeImageWrapperTranslator* ImageWrapperTranslator = NewObject<UInterchangeImageWrapperTranslator>(GetTransientPackage(), NAME_None);
		ImageWrapperTranslator->SetResultsContainer(Results);
		TexturePayloadData = ImageWrapperTranslator->GetTexturePayloadDataFromBuffer(ImageData);
	}
	else
	{
		const FString TextureFilePath = FPaths::ConvertRelativePathToFull(GltfTexture.Source.FilePath);

		UInterchangeSourceData* PayloadSourceData = UInterchangeManager::GetInterchangeManager().CreateSourceData(TextureFilePath);
		FGCObjectScopeGuard ScopedSourceData(PayloadSourceData);

		if (!PayloadSourceData)
		{
			return TexturePayloadData;
		}

		UInterchangeTranslatorBase* SourceTranslator = UInterchangeManager::GetInterchangeManager().GetTranslatorForSourceData(PayloadSourceData);
		FGCObjectScopeGuard ScopedSourceTranslator(SourceTranslator);
		const IInterchangeTexturePayloadInterface* TextureTranslator = Cast< IInterchangeTexturePayloadInterface >(SourceTranslator);
		if (!ensure(TextureTranslator))
		{
			return TexturePayloadData;
		}
		SourceTranslator->SetResultsContainer(Results);

		AlternateTexturePath = TextureFilePath;

		TexturePayloadData = TextureTranslator->GetTexturePayloadData(PayloadKey, AlternateTexturePath);
	}

	if (PayloadKeys.Num() == 2 && TexturePayloadData.IsSet())
	{
		bool TextureUsedAsNormal = false;
		LexFromString(TextureUsedAsNormal, *PayloadKeys[1]);

		TexturePayloadData.GetValue().CompressionSettings = TextureUsedAsNormal ? TC_Normalmap : TC_Default;
	}

	return TexturePayloadData;
}

TFuture<TOptional<UE::Interchange::FAnimationPayloadData>> UInterchangeGLTFTranslator::GetAnimationPayloadData(const FInterchangeAnimationPayLoadKey& PayLoadKey, const double BakeFrequency, const double RangeStartSecond, const double RangeStopSecond) const
{
	return Async(EAsyncExecution::TaskGraph, [this, PayLoadKey, BakeFrequency, RangeStartSecond, RangeStopSecond]
		{

			TOptional<UE::Interchange::FAnimationPayloadData> Result;
			UE::Interchange::FAnimationPayloadData AnimationPayLoadData(PayLoadKey.Type);

			switch (PayLoadKey.Type)
			{
			case EInterchangeAnimationPayLoadType::CURVE:
				if (UE::Interchange::Gltf::Private::GetTransformAnimationPayloadData(PayLoadKey.UniqueId, GltfAsset, AnimationPayLoadData))
				{
					Result.Emplace(AnimationPayLoadData);
				}
				break;
			case EInterchangeAnimationPayLoadType::MORPHTARGETCURVE:
				if (UE::Interchange::Gltf::Private::GetMorphTargetAnimationPayloadData(PayLoadKey.UniqueId, GltfAsset, AnimationPayLoadData))
				{
					Result.Emplace(AnimationPayLoadData);
				}
				break;
			case EInterchangeAnimationPayLoadType::BAKED:
				AnimationPayLoadData.BakeFrequency = BakeFrequency;
				AnimationPayLoadData.RangeStartTime = RangeStartSecond;
				AnimationPayLoadData.RangeEndTime = RangeStopSecond;
				if (UE::Interchange::Gltf::Private::GetBakedAnimationTransformPayloadData(PayLoadKey.UniqueId, GltfAsset, AnimationPayLoadData))
				{
					Result.Emplace(AnimationPayLoadData);
				}
				break;
			case EInterchangeAnimationPayLoadType::STEPCURVE:
			case EInterchangeAnimationPayLoadType::NONE:
			default:
				break;
			}

			return Result;
		}
	);
}

void UInterchangeGLTFTranslator::HandleGltfAnimation(UInterchangeBaseNodeContainer& NodeContainer, int32 AnimationIndex) const
{
	const GLTF::FAnimation& GltfAnimation = GltfAsset.Animations[AnimationIndex];

	TMap< const GLTF::FNode*, TArray<int32> > NodeChannelsMap;

	TMap<FString, UInterchangeSkeletalAnimationTrackNode*> RootJointIndexToTrackNodeMap;
	TMap<UInterchangeSkeletalAnimationTrackNode*, TMap<FString, TArray<int32>>> TrackNodeToJointUidWithChannelsUsedMap;

	TMap<int32, TSet<int32>> SkeletonRootToAnimatedJointNodeIndicesMap;	// (SkeletonRootUID, [AnimatedNodeIndices])

	for (int32 ChannelIndex = 0; ChannelIndex < GltfAnimation.Channels.Num(); ++ChannelIndex)
	{
		const GLTF::FAnimation::FChannel& Channel = GltfAnimation.Channels[ChannelIndex];
		const GLTF::FNode* AnimatedNode = &Channel.Target.Node;

		const FString* AnimatedNodeUidPtr = NodeUidMap.Find(AnimatedNode);
		if (!ensure(AnimatedNodeUidPtr))
		{
			continue;
		}

		FString AnimatedNodeUid = *AnimatedNodeUidPtr;

		auto CreateSkeletalAnimationTrackNode = [&NodeContainer, &RootJointIndexToTrackNodeMap, &GltfAnimation, &AnimationIndex, &TrackNodeToJointUidWithChannelsUsedMap, &ChannelIndex, &AnimatedNodeUid](const FString& SkeletonNodeUid, const TMap<FString, FString>& AnimationPayloadKeyForMorphTargetNodeUids)
		{
			if (SkeletonNodeUid.Len() > 0)
			{
				UInterchangeSkeletalAnimationTrackNode* TrackNode = nullptr;
				if (RootJointIndexToTrackNodeMap.Contains(SkeletonNodeUid))
				{
					TrackNode = RootJointIndexToTrackNodeMap[SkeletonNodeUid];
				}
				else
				{
					TrackNode = NewObject< UInterchangeSkeletalAnimationTrackNode >(&NodeContainer);
					FString TrackNodeUid = "\\SkeletalAnimation\\" + SkeletonNodeUid + "_" + LexToString(AnimationIndex);
					TrackNode->InitializeNode(TrackNodeUid, GltfAnimation.Name, EInterchangeNodeContainerType::TranslatedAsset);
					TrackNode->SetCustomSkeletonNodeUid(SkeletonNodeUid);

					NodeContainer.AddNode(TrackNode);

					RootJointIndexToTrackNodeMap.Add(SkeletonNodeUid, TrackNode);
				}

				for (const TPair<FString, FString>& AnimationPayloadKeyForMorphTargetNodeUid : AnimationPayloadKeyForMorphTargetNodeUids)
				{
					TrackNode->SetAnimationPayloadKeyForMorphTargetNodeUid(AnimationPayloadKeyForMorphTargetNodeUid.Key, AnimationPayloadKeyForMorphTargetNodeUid.Value, EInterchangeAnimationPayLoadType::MORPHTARGETCURVE);
				}

				TMap<FString, TArray<int32>>& JointUidWithChannelsUsedMap = TrackNodeToJointUidWithChannelsUsedMap.FindOrAdd(TrackNode);
				TArray<int32>& ChannelsUsed = JointUidWithChannelsUsedMap.FindOrAdd(AnimatedNodeUid);
				ChannelsUsed.Add(ChannelIndex);
			}
		};


		bool bAnimationChannelProcessed = false;

		bool bSkeletalAnimation = AnimatedNode->Type == GLTF::FNode::EType::Joint && GltfAsset.Nodes.IsValidIndex(AnimatedNode->RootJointIndex);
		if (bSkeletalAnimation)
		{
			const FString* SkeletonUidPtr = NodeUidMap.Find(&GltfAsset.Nodes[AnimatedNode->RootJointIndex]);
			FString SkeletonNodeUid = *SkeletonUidPtr;

			CreateSkeletalAnimationTrackNode(SkeletonNodeUid, TMap<FString, FString>());

			TSet<int32> AnimatedNodeIndices = SkeletonRootToAnimatedJointNodeIndicesMap.FindOrAdd(AnimatedNode->RootJointIndex);
			for (int32 NodeIndex = 0; NodeIndex < GltfAsset.Nodes.Num(); NodeIndex++)
			{
				if (&GltfAsset.Nodes[NodeIndex] == AnimatedNode)
				{
					AnimatedNodeIndices.Add(NodeIndex);
					break;
				}
			}

			bAnimationChannelProcessed = true;
		}

		bool bMorphTargetAnimation = Channel.Target.Path == GLTF::FAnimation::EPath::Weights;
		if (bMorphTargetAnimation)
		{
			TMap<FString, FString> AnimationPayloadKeyForMorphTargetNodeUids;
			//Find SceneNode that references the MeshNode:
			if (const UInterchangeSceneNode* ConstSceneMeshActorNode = Cast< UInterchangeSceneNode >(NodeContainer.GetNode(*AnimatedNodeUid)))
			{
				FString SkeletalMeshUid;
				if (ConstSceneMeshActorNode->GetCustomAssetInstanceUid(SkeletalMeshUid))
				{
					if (const UInterchangeMeshNode* MeshNode = Cast< UInterchangeMeshNode >(NodeContainer.GetNode(SkeletalMeshUid)))
					{
						TArray<FString> MorphTargetDependencies;
						MeshNode->GetMorphTargetDependencies(MorphTargetDependencies);
						for (const FString& MorphTargetDependencyUid : MorphTargetDependencies)
						{
							if (const UInterchangeMeshNode* MorphTargetNodeConst = Cast< UInterchangeMeshNode >(NodeContainer.GetNode(MorphTargetDependencyUid)))
							{
								if (MorphTargetNodeConst->GetPayLoadKey().IsSet())
								{
									FInterchangeMeshPayLoadKey PayLoadKey = MorphTargetNodeConst->GetPayLoadKey().GetValue();
									FString PayLoadKeyUniqueId = LexToString(AnimationIndex) + TEXT(":") + LexToString(ChannelIndex) + TEXT(":") + PayLoadKey.UniqueId;

									AnimationPayloadKeyForMorphTargetNodeUids.Add(MorphTargetDependencyUid, PayLoadKeyUniqueId);
								}
							}
						}
					}
				}
			}

			if (AnimationPayloadKeyForMorphTargetNodeUids.Num() > 0)
			{
				FString SkeletonNodeUid = AnimatedNodeUid;
				CreateSkeletalAnimationTrackNode(AnimatedNodeUid, AnimationPayloadKeyForMorphTargetNodeUids);
			}

			bAnimationChannelProcessed = true;
		}

		if (bAnimationChannelProcessed)
		{
			continue;
		}

		TArray<int32>& NodeChannels = NodeChannelsMap.FindOrAdd(AnimatedNode);
		NodeChannels.Add(ChannelIndex);
	}

	{//Fix Skeletal Animations
		for (const TTuple<int32, TSet<int32>>& AnimatedJointNodeIndices : SkeletonRootToAnimatedJointNodeIndicesMap)
		{
			int32 SkeletonRootIndex = AnimatedJointNodeIndices.Key;

			const FString* SkeletonUid = NodeUidMap.Find(&GltfAsset.Nodes[SkeletonRootIndex]);
			if (!ensure(SkeletonUid))
			{
				continue;
			}

			if (!RootJointIndexToTrackNodeMap.Contains(*SkeletonUid))
			{
				continue;
			}
			UInterchangeSkeletalAnimationTrackNode* TrackNode = RootJointIndexToTrackNodeMap[*SkeletonUid];

			TSet<int32> JointsWithBindPose;
			UE::Interchange::Gltf::Private::AcquireJointsWithBindPose(GltfAsset.Nodes, SkeletonRootIndex, JointsWithBindPose);

			TSet<int32> NodesToAddToAnimation = JointsWithBindPose.Difference(AnimatedJointNodeIndices.Value);
			for (int32 NodeToAddIndex : NodesToAddToAnimation)
			{
				const FString* NodeToAddUidPtr = NodeUidMap.Find(&GltfAsset.Nodes[NodeToAddIndex]);
				if (!ensure(NodeToAddUidPtr))
				{
					continue;
				}
				TrackNode->SetAnimationPayloadKeyForSceneNodeUid(*NodeToAddUidPtr, TEXT("BIND_POSE_FIX<->") + LexToString(NodeToAddIndex), EInterchangeAnimationPayLoadType::BAKED);
			}
		}
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
				bool bHasNonWeightAnimationChannel = false;
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
						if (Channel.Target.Path != GLTF::FAnimation::EPath::Weights && (Sampler.Input.Count != Sampler.Output.Count))
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

					if (Channel.Target.Path != GLTF::FAnimation::EPath::Weights)
					{
						bHasNonWeightAnimationChannel = true;
						Payload += ":" + LexToString(ChannelIndex);
					}
				}

				//set payload:
				if (Payload.Len() > 0)
				{
					if (bHasNonWeightAnimationChannel)
					{
						TrackNodeAndJointNodeChannels.Key->SetAnimationPayloadKeyForSceneNodeUid(JointNodeUidAndChannelsUsedPair.Key, Payload, EInterchangeAnimationPayLoadType::BAKED);
					}
					
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

	const FString AnimTrackSetNodeUid = TEXT("\\Animation\\") + GltfAnimation.UniqueId;
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

		TransformAnimTrackNode->SetCustomAnimationPayloadKey(PayloadKey, EInterchangeAnimationPayLoadType::CURVE);
		TransformAnimTrackNode->SetCustomUsedChannels(UsedChannels);

		NodeContainer.AddNode(TransformAnimTrackNode);

		TrackSetNode->AddCustomAnimationTrackUid(TransformAnimTrackNodeUid);
	}

	NodeContainer.AddNode(TrackSetNode);
}

void UInterchangeGLTFTranslator::SetTextureSRGB(UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FTextureMap& TextureMap, bool bSRGB) const
{
	if (GltfAsset.Textures.IsValidIndex(TextureMap.TextureIndex))
	{
		const FString TextureUid = UInterchangeTextureNode::MakeNodeUid(GltfAsset.Textures[TextureMap.TextureIndex].UniqueId);
		if (UInterchangeTextureNode* TextureNode = const_cast<UInterchangeTextureNode*>(Cast<UInterchangeTextureNode>(NodeContainer.GetNode(TextureUid))))
		{
			TextureNode->SetCustomSRGB(bSRGB);
		}
	}
}

TFuture<TOptional<UE::Interchange::FVariantSetPayloadData>> UInterchangeGLTFTranslator::GetVariantSetPayloadData(const FString& PayloadKey) const
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

void UInterchangeGLTFTranslator::HandleGltfVariants(UInterchangeBaseNodeContainer& NodeContainer, const FString& FileName) const
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
								const FString MaterialUid = UInterchangeShaderGraphNode::MakeNodeUid(GltfMaterial.UniqueId);

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

bool UInterchangeGLTFTranslator::GetVariantSetPayloadData(UE::Interchange::FVariantSetPayloadData& PayloadData) const
{
	using namespace UE;

	PayloadData.Variants.SetNum(GltfAsset.Variants.Num());

	TMap<const TCHAR*, Interchange::FVariant*, FDefaultSetAllocator, TStringPointerMapKeyFuncs_DEPRECATED<const TCHAR*, Interchange::FVariant*>> VariantMap;
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
						const FString MaterialNodeUid = UInterchangeShaderGraphNode::MakeNodeUid(GltfMaterial.UniqueId);

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
							PropertyCaptureData.ObjectUid = MaterialNodeUid;

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

TFuture< TOptional< UE::Interchange::FMeshPayloadData > > UInterchangeGLTFTranslator::GetMeshPayloadData(const FInterchangeMeshPayLoadKey& PayLoadKey, const FTransform& MeshGlobalTransform) const
{
	return Async(EAsyncExecution::TaskGraph, [this, PayLoadKey, MeshGlobalTransform]
		{
			UE::Interchange::FMeshPayloadData MeshPayLoadData;
			bool bSuccessfullAcquisition = false;

			switch (PayLoadKey.Type)
			{
			case EInterchangeMeshPayLoadType::STATIC:
				bSuccessfullAcquisition = UE::Interchange::Gltf::Private::GetStaticMeshPayloadDataForPayLoadKey(GltfAsset, PayLoadKey.UniqueId, MeshGlobalTransform, MeshPayLoadData.MeshDescription);
				break;
			case EInterchangeMeshPayLoadType::SKELETAL:
				bSuccessfullAcquisition = UE::Interchange::Gltf::Private::GetSkeletalMeshDescriptionForPayLoadKey(GltfAsset, PayLoadKey.UniqueId, MeshGlobalTransform, MeshPayLoadData.MeshDescription, &MeshPayLoadData.JointNames);
				break;
			case EInterchangeMeshPayLoadType::MORPHTARGET:
				//GLTF handles morph targets as simple Meshes
				bSuccessfullAcquisition = UE::Interchange::Gltf::Private::GetStaticMeshPayloadDataForPayLoadKey(GltfAsset, PayLoadKey.UniqueId, MeshGlobalTransform, MeshPayLoadData.MeshDescription);
				break;
			case EInterchangeMeshPayLoadType::NONE:
			default:
				break;
			}

			TOptional<UE::Interchange::FMeshPayloadData> Result;
			if (bSuccessfullAcquisition)
			{
				Result.Emplace(MeshPayLoadData);
			}

			return Result;
		});
}

void UInterchangeGLTFTranslator::HandleGltfSkeletons(UInterchangeBaseNodeContainer& NodeContainer, const FString& SceneNodeUid, const TArray<int32>& SkinnedMeshNodes, TSet<int>& UnusedMeshIndices) const
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
		
		int32 RootSkinJointIndex = UE::Interchange::Gltf::Private::GetRootNodeIndex(GltfAsset, GltfAsset.Skins[SkinnedMeshNode.Skindex].Joints);
		if (!GltfAsset.Nodes.IsValidIndex(RootSkinJointIndex))
		{
			continue;
		}

		//basedon that root joint group the SkinnedMeshNodes:
		TArray<int32>& GroupedSkinnedMeshNodes = RootJointGroupedSkinnedMeshNodes.FindOrAdd(RootSkinJointIndex);
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
			int32 RootSkinJointNodeIndex = RootJointToSkinnedMeshNodes.Key;
			int32 RootJointNodeIndex = GltfAsset.Nodes[RootSkinJointNodeIndex].RootJointIndex;

			//Skeletal Mesh's naming policy: (Mesh.Name)_(RootJointNode.Name) naming policy:
			FString SkeletalName = GltfAsset.Meshes[MeshIndex].Name + TEXT("_") + GltfAsset.Nodes[RootSkinJointNodeIndex].Name;
			FString SkeletalId = GltfAsset.Meshes[MeshIndex].UniqueId + TEXT("_") + GltfAsset.Nodes[RootSkinJointNodeIndex].UniqueId;

			UInterchangeMeshNode* SkeletalMeshNode = HandleGltfMesh(NodeContainer, GltfAsset.Meshes[MeshIndex], MeshIndex, UnusedMeshIndices, SkeletalName, SkeletalId);
			SkeletalMeshNode->SetSkinnedMesh(true);

			//set the root joint node as the skeletonDependency:
			const GLTF::FNode& RootJointNode = GltfAsset.Nodes[RootJointNodeIndex];
			const FString* SkeletonNodeUid = NodeUidMap.Find(&RootJointNode);
			if (ensure(SkeletonNodeUid))
			{
				SkeletalMeshNode->SetSkeletonDependencyUid(*SkeletonNodeUid);
			}

			bool bBakeSkinJointTransform = RootSkinJointNodeIndex != RootJointNodeIndex || RootJointNode.ParentIndex == -1;

			//generate payload key:
			//of template:
			//"LexToString(bBakeSkinJointTransform)";"LexToString(SkinnedMeshNode.MeshIndex | (SkinnedMeshNode.Skindex << 16))":"LexToString(SkinnedMeshNode.MeshIndex | (SkinnedMeshNode.Skindex << 16))".....
			FString Payload = TEXT("");
			for (int32 SkinnedMeshIndex : RootJointToSkinnedMeshNodes.Value)
			{
				const GLTF::FNode& SkinnedMeshNode = GltfAsset.Nodes[SkinnedMeshIndex];
				if (Payload.Len() > 0)
				{
					Payload += TEXT(":");
				}
				
				Payload += LexToString(SkinnedMeshNode.MeshIndex | (SkinnedMeshNode.Skindex << 16));
			}
			Payload = LexToString(bBakeSkinJointTransform) + TEXT(";") + Payload;
			SkeletalMeshNode->SetPayLoadKey(Payload, EInterchangeMeshPayLoadType::SKELETAL);

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

UInterchangeMeshNode* UInterchangeGLTFTranslator::HandleGltfMesh(UInterchangeBaseNodeContainer& NodeContainer,
	const GLTF::FMesh& GltfMesh, int MeshIndex,
	TSet<int>& UnusedMeshIndices,
	const FString& SkeletalName/*If set it creates the mesh even if it was already created (for Skeletals)*/,
	const FString& SkeletalId) const
{
	FString MeshName = SkeletalName.Len() ? SkeletalName : GltfMesh.Name;
	FString MeshNodeUid = TEXT("\\Mesh\\") + (SkeletalId.Len() ? SkeletalId : GltfMesh.UniqueId);

	//check if Node already exist with MeshNodeUid:
	if (const UInterchangeMeshNode* Node = Cast< UInterchangeMeshNode >(NodeContainer.GetNode(MeshNodeUid)))
	{
		UInterchangeMeshNode* MeshNode = const_cast<UInterchangeMeshNode*>(Node);
		if (ensure(MeshNode))
		{
			return MeshNode;
		}
	}

	//to track which meshes we have to generate a mesh node for at the end of Translate:
	UnusedMeshIndices.Remove(MeshIndex);

	//Create Mesh Node:
	UInterchangeMeshNode* MeshNode = NewObject< UInterchangeMeshNode >(&NodeContainer);
	MeshNode->InitializeNode(MeshNodeUid, MeshName, EInterchangeNodeContainerType::TranslatedAsset);

	//Generate Mesh Payload:
	FString PayloadKey = LexToString(MeshIndex);
	MeshNode->SetPayLoadKey(PayloadKey, EInterchangeMeshPayLoadType::STATIC);

	NodeContainer.AddNode(MeshNode);

	//Set Slot Material Dependencies:
	for (int32 PrimitiveCounter = 0; PrimitiveCounter < GltfMesh.Primitives.Num(); PrimitiveCounter++)
	{
		const GLTF::FPrimitive& Primitive = GltfMesh.Primitives[PrimitiveCounter];

		// Assign materials
		if (GltfAsset.Materials.IsValidIndex(Primitive.MaterialIndex))
		{
			const FString MaterialName = GltfAsset.Materials[Primitive.MaterialIndex].Name;
			const FString ShaderGraphNodeUid = UInterchangeShaderGraphNode::MakeNodeUid(GltfAsset.Materials[Primitive.MaterialIndex].UniqueId);
			MeshNode->SetSlotMaterialDependencyUid(MaterialName, ShaderGraphNodeUid);
		}
	}

	//Generate Morph Target Meshes:
	if (GltfMesh.MorphTargetNames.Num() > 0)
	{
		for (int32 MorphTargetIndex = 0; MorphTargetIndex < GltfMesh.MorphTargetNames.Num(); MorphTargetIndex++)
		{
			//check if MorphTarget mesh was already created or not:
			FString MorphTargetName = GltfMesh.MorphTargetNames[MorphTargetIndex]; //Morph Target Names are validated to be unique (GLTFAsset::GenerateNames)

			//Add the MorphTargetName as a dependency to original mesh:
			MeshNode->SetMorphTargetDependencyUid(MorphTargetName);

			//check if Node already exist with MorphTargetName(uid):
			if (const UInterchangeMeshNode* Node = Cast< UInterchangeMeshNode >(NodeContainer.GetNode(MorphTargetName)))
			{
				continue;
			}

			//create MorphTargetMeshNode:
			UInterchangeMeshNode* MorphTargetMeshNode = NewObject< UInterchangeMeshNode >(&NodeContainer);
			MorphTargetMeshNode->InitializeNode(MorphTargetName, MorphTargetName, EInterchangeNodeContainerType::TranslatedAsset);

			//Generate Payload:
			FString MorphTargetPayLoadKey = LexToString(MeshIndex) + TEXT(":") + LexToString(MorphTargetIndex);
			MorphTargetMeshNode->SetPayLoadKey(MorphTargetPayLoadKey, EInterchangeMeshPayLoadType::MORPHTARGET);

			//set mesh as a morph target:
			MorphTargetMeshNode->SetMorphTarget(true);
			MorphTargetMeshNode->SetMorphTargetName(MorphTargetName);

			NodeContainer.AddNode(MorphTargetMeshNode);

			//Set Slot Material Dependencies:
			for (int32 PrimitiveCounter = 0; PrimitiveCounter < GltfMesh.Primitives.Num(); PrimitiveCounter++)
			{
				const GLTF::FPrimitive& Primitive = GltfMesh.Primitives[PrimitiveCounter];

				// Assign materials
				if (GltfAsset.Materials.IsValidIndex(Primitive.MaterialIndex))
				{
					const FString MaterialName = GltfAsset.Materials[Primitive.MaterialIndex].Name;
					const FString ShaderGraphNodeUid = UInterchangeShaderGraphNode::MakeNodeUid(GltfAsset.Materials[Primitive.MaterialIndex].UniqueId);
					MorphTargetMeshNode->SetSlotMaterialDependencyUid(MaterialName, ShaderGraphNodeUid);
				}
			}
		}
	}

	return MeshNode;
}

UInterchangeGLTFTranslator::UInterchangeGLTFTranslator()
{
	if (!HasAllFlags(RF_ClassDefaultObject))
	{
		bRenderSettingsClearCoatEnableSecondNormal = GetDefault<URendererSettings>()->bClearCoatEnableSecondNormal != 0;
	}

	if (!UE::Interchange::GLTFMaterials::AreRequiredPackagesLoaded())
	{
		UE_LOG(LogInterchangeImport, Warning, TEXT("UInterchangeGLTFPipeline: Some required packages are missing. Material import might be wrong"));
	}
}

#undef LOCTEXT_NAMESPACE
