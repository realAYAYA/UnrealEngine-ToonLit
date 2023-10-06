// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSourceSurface.h"

#include "Engine/SkinnedAssetCommon.h"
#include "Engine/StaticMesh.h"
#include "GPUSkinPublicDefs.h"
#include "Interfaces/ITargetPlatform.h"
#include "Materials/MaterialInstance.h"
#include "GPUSkinVertexFactory.h"

#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/MutableMeshBufferUtils.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceColor.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceFloat.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceGroupProjector.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceImage.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceMesh.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceTable.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/MutableUtils.h"
#include "MuCOE/Nodes/CustomizableObjectNodeCopyMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeEditMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeExtendMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeFloatConstant.h"
#include "MuCOE/Nodes/CustomizableObjectNodeFloatParameter.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterialVariation.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMorphMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeRemoveMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeRemoveMeshBlocks.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuT/NodeImageFormat.h"
#include "MuT/NodeImageMipmap.h"
#include "MuT/NodeImageNormalComposite.h"
#include "MuT/NodeImageResize.h"
#include "MuT/NodeMeshConstant.h"
#include "MuT/NodeMeshFormat.h"
#include "MuT/NodeMeshFragment.h"
#include "MuT/NodePatchImage.h"
#include "MuT/NodePatchMesh.h"
#include "MuT/NodeScalarConstant.h"
#include "MuT/NodeSurfaceEdit.h"
#include "MuT/NodeSurfaceVariation.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


/**
 * Show a compilation warning when any of the meshes behind the pin InMeshPin has a UV that is not normalized.
 * 
 * @param InMeshPin Meshes connected to this pin (directly or indirectly through switch/variation nodes).
 * @param OperationNode Node which is performing the block operation. 
 */
void LayoutOperationUVNormalizedWarning(FMutableGraphGenerationContext& GenerationContext, const UEdGraphPin* InMeshPin, const UCustomizableObjectNode* OperationNode, const int32 UVIndex)
{
	if (const UEdGraphPin* ConnectedPin = FollowInputPin(*InMeshPin))
	{
		FPinDataValue* PinData = GenerationContext.PinData.Find(ConnectedPin);
		if (PinData)
		{
			for (const FMeshData& MeshData : PinData->MeshesData)
			{
				if (const UCustomizableObjectNodeMesh* Node = Cast<UCustomizableObjectNodeMesh>(MeshData.Node))
				{
					bool bNormalized = true;
					if (const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(MeshData.Mesh))
					{
						bNormalized = IsUVNormalized(*SkeletalMesh, MeshData.LOD, MeshData.MaterialIndex, UVIndex);
					}
					else if (const UStaticMesh* StaticMesh = Cast<UStaticMesh>(MeshData.Mesh))
					{
						bNormalized = IsUVNormalized(*StaticMesh, MeshData.LOD, MeshData.MaterialIndex, UVIndex);
					}

					if (!bNormalized)
					{
						FText Text = FText::Format(LOCTEXT("UVNotNormalized", "UV from mesh {0} not normalized. Required to perform texture layout operations."), FText::FromString(*MeshData.Mesh->GetName()));
						GenerationContext.Compiler->CompilerLog(Text, OperationNode, EMessageSeverity::Type::Info);
					}
				}
			}
		}
	}
}


void SetSurfaceFormat( FMutableGraphGenerationContext& GenerationContext,
					   mu::FMeshBufferSet& OutVertexBufferFormat, mu::FMeshBufferSet& OutIndexBufferFormat, const FMutableGraphMeshGenerationData& MeshData, 
					   ECustomizableObjectNumBoneInfluences ECustomizableObjectNumBoneInfluences, bool bWithRealTimeMorphs, 
					   bool bWithClothing, bool bWith16BitWeights )
{
	// Limit skinning weights if necessary
	// \todo: make it more flexible to support 3 or 5 or 1 weight, since there is support for this in 4.25
	const int32 MutableBonesPerVertex = FGPUBaseSkinVertexFactory::UseUnlimitedBoneInfluences(MeshData.MaxNumBonesPerVertex, GenerationContext.Options.TargetPlatform) &&
										MeshData.MaxNumBonesPerVertex < (int32)ECustomizableObjectNumBoneInfluences ?
											MeshData.MaxNumBonesPerVertex :
											(int32)ECustomizableObjectNumBoneInfluences;

	ensure(MutableBonesPerVertex <= MAX_TOTAL_INFLUENCES);
	
	if (MutableBonesPerVertex != MeshData.MaxNumBonesPerVertex)
	{
		UE_LOG(LogMutable, Verbose, TEXT("In object [%s] Mesh bone number adjusted from %d to %d."), *GenerationContext.Object->GetName(), MeshData.MaxNumBonesPerVertex, MutableBonesPerVertex);
	}

	int MutableBufferCount = MUTABLE_VERTEXBUFFER_TEXCOORDS + 1;
	if (MeshData.bHasVertexColors)
	{
		++MutableBufferCount;
	}

	if (MeshData.MaxNumBonesPerVertex > 0 && MeshData.MaxBoneIndexTypeSizeBytes > 0)
	{
		++MutableBufferCount;
	}

	if (bWithRealTimeMorphs)
	{
		MutableBufferCount += 2;
	}

	if (bWithClothing)
	{
		++MutableBufferCount;
	}

	MutableBufferCount += MeshData.SkinWeightProfilesSemanticIndices.Num();

	OutVertexBufferFormat.SetBufferCount(MutableBufferCount);

	int32 CurrentVertexBuffer = 0;

	// Vertex buffer
	MutableMeshBufferUtils::SetupVertexPositionsBuffer(CurrentVertexBuffer, OutVertexBufferFormat);
	++CurrentVertexBuffer;

	// Tangent buffer
	MutableMeshBufferUtils::SetupTangentBuffer(CurrentVertexBuffer, OutVertexBufferFormat);
	++CurrentVertexBuffer;

	// Texture coords buffer
	MutableMeshBufferUtils::SetupTexCoordinatesBuffer(CurrentVertexBuffer, MeshData.NumTexCoordChannels, OutVertexBufferFormat);
	++CurrentVertexBuffer;

	// Skin buffer
	if (MeshData.MaxNumBonesPerVertex > 0 && MeshData.MaxBoneIndexTypeSizeBytes > 0)
	{
		const int32 MaxBoneWeightTypeSizeBytes = bWith16BitWeights ? 2 : 1;
		MutableMeshBufferUtils::SetupSkinBuffer(CurrentVertexBuffer, MeshData.MaxBoneIndexTypeSizeBytes, MaxBoneWeightTypeSizeBytes, MutableBonesPerVertex, OutVertexBufferFormat);
		++CurrentVertexBuffer;
	}

	// Colour buffer
	if (MeshData.bHasVertexColors)
	{
		MutableMeshBufferUtils::SetupVertexColorBuffer(CurrentVertexBuffer, OutVertexBufferFormat);
		++CurrentVertexBuffer;
	}

	// MorphTarget vertex tracking info buffers
	if (bWithRealTimeMorphs)
	{
		{
			using namespace mu;
			const int ElementSize = sizeof(int32);
			constexpr int ChannelCount = 1;
			const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = { MBS_OTHER };
			const int SemanticIndices[ChannelCount] = { 0 };
			const MESH_BUFFER_FORMAT Formats[ChannelCount] = { MBF_INT32 };
			const int Components[ChannelCount] = { 1 };
			const int Offsets[ChannelCount] = { 0 };

			OutVertexBufferFormat.SetBuffer(CurrentVertexBuffer, ElementSize, ChannelCount, Semantics, SemanticIndices, Formats, Components, Offsets);
			++CurrentVertexBuffer;
		}

		{
			using namespace mu;
			const int ElementSize = sizeof(int32);
			constexpr int ChannelCount = 1;
			const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = { MBS_OTHER };
			const int SemanticIndices[ChannelCount] = { 1 };
			const MESH_BUFFER_FORMAT Formats[ChannelCount] = { MBF_INT32 };
			const int Components[ChannelCount] = { 1 };
			const int Offsets[ChannelCount] = { 0 };

			OutVertexBufferFormat.SetBuffer(CurrentVertexBuffer, ElementSize, ChannelCount, Semantics, SemanticIndices, Formats, Components, Offsets);
			++CurrentVertexBuffer;
		}
	}

	//Clothing Data Buffer.
	if (bWithClothing)
	{
		using namespace mu;
		const int ElementSize = sizeof(int32);
		constexpr int ChannelCount = 1;
		const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = { MBS_OTHER };
		const int SemanticIndices[ChannelCount] = { bWithRealTimeMorphs ? 2 : 0 };
		const MESH_BUFFER_FORMAT Formats[ChannelCount] = { MBF_INT32 };
		const int Components[ChannelCount] = { 1 };
		const int Offsets[ChannelCount] = { 0 };

		OutVertexBufferFormat.SetBuffer(CurrentVertexBuffer, ElementSize, ChannelCount, Semantics, SemanticIndices, Formats, Components, Offsets);
		++CurrentVertexBuffer;
	}

	for (int32 ProfileSemanticIndex : MeshData.SkinWeightProfilesSemanticIndices)
	{
		MutableMeshBufferUtils::SetupSkinWeightProfileBuffer(CurrentVertexBuffer, MeshData.MaxBoneIndexTypeSizeBytes, 1, MutableBonesPerVertex, ProfileSemanticIndex, OutVertexBufferFormat);
		++CurrentVertexBuffer;
	}

	// Index buffer
	MutableMeshBufferUtils::SetupIndexBuffer(OutIndexBufferFormat);
}


void UpdateSharedSurfaceId(FMutableGraphGenerationContext& GenerationContext, UCustomizableObjectNodeMaterial* NodeMaterial, mu::NodeSurfaceNewPtr InNodeSurface)
{
	// \TODO: Reusability is actually per-texture: if a texture is set to not use layouts (setting LayoutIndex to -1) then it can always be reused.
	// Reuse materials between LODs when using automatics LODs, if texture layout management is disabled or if bReuseMaterials is enabled in the node material.
	const bool bCanShareSurface = GenerationContext.CurrentAutoLODStrategy == ECustomizableObjectAutomaticLODStrategy::AutomaticFromMesh
		&& NodeMaterial->bReuseMaterialBetweenLODs;

	// Set shared surface Id to reuse materials and textures between LODs if automatic LODs from mesh is being used
	if (bCanShareSurface && InNodeSurface)
	{
		FMutableGraphGenerationContext::FSharedSurfaces& SharedSurface = GenerationContext.SharedSurfaceIds.FindOrAdd(NodeMaterial, { GenerationContext.SharedSurfaceIds.Num(), InNodeSurface });
		SharedSurface.NodeSurface = InNodeSurface;

		InNodeSurface->SetSharedSurfaceId(SharedSurface.SharedSurfaceId);
	}

	// Invalidate the shared surface Id. 
	else if (!bCanShareSurface && !InNodeSurface)
	{
		FMutableGraphGenerationContext::FSharedSurfaces* SharedSurface = GenerationContext.SharedSurfaceIds.Find(NodeMaterial);
		if (SharedSurface && SharedSurface->NodeSurface)
		{
			SharedSurface->NodeSurface->SetSharedSurfaceId(INDEX_NONE);
		}
	}
}


mu::NodeSurfacePtr GenerateMutableSourceSurface(const UEdGraphPin * Pin, FMutableGraphGenerationContext & GenerationContext, FMutableGraphSurfaceGenerationData& SurfaceData)
{
	check(Pin)
	RETURN_ON_CYCLE(*Pin, GenerationContext)

	CheckNumOutputs(*Pin, GenerationContext);

	UCustomizableObjectNode* Node = CastChecked<UCustomizableObjectNode>(Pin->GetOwningNode());

	const FGeneratedKey Key(reinterpret_cast<void*>(&GenerateMutableSourceSurface), *Pin, *Node, GenerationContext, true);
	if (const FGeneratedData* Generated = GenerationContext.Generated.Find(Key))
	{
		SurfaceData.NodeMaterial = Cast<UCustomizableObjectNodeMaterial>(Generated->Source);
		return static_cast<mu::NodeSurface*>(Generated->Node.get());
	}
	
	mu::NodeSurfacePtr Result;

	const int32 LOD = Node->IsAffectedByLOD() ? GenerationContext.CurrentLOD : 0;
	
	if (UCustomizableObjectNode* CustomObjNode = Cast<UCustomizableObjectNode>(Node))
	{
		if (CustomObjNode->IsNodeOutDatedAndNeedsRefresh())
		{
			CustomObjNode->SetRefreshNodeWarning();
		}
	}

	if (UCustomizableObjectNodeMaterial* TypedNodeMat = Cast<UCustomizableObjectNodeMaterial>(Node))
	{
		if (TypedNodeMat->MeshComponentIndex != GenerationContext.CurrentMeshComponent)
		{
			return Result;
		}

		SurfaceData.NodeMaterial = TypedNodeMat; // Save the NodeMaterial for the calling recursive calls

		// NodeCopyMaterial. Special case when the TypedNodeMat is a NodeCopyMaterial. The TypedNodeMat pointer now points to the parent NodeMaterial except when reading the mesh pin, which comes from the NodeCopyMaterial.
		UCustomizableObjectNodeMaterial* TypedNodeMaterial = TypedNodeMat;
		UCustomizableObjectNodeMaterial* TypedNodeCopyMaterial = TypedNodeMat;

		if (UCustomizableObjectNodeCopyMaterial* TypedDerivedNodeCopyMaterial = Cast<UCustomizableObjectNodeCopyMaterial>(TypedNodeMat))
		{
			UEdGraphPin* MaterialPin = TypedDerivedNodeCopyMaterial->GetMaterialPin();
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*MaterialPin))
			{
				FMutableGraphSurfaceGenerationData ParentSurfaceData;
				GenerateMutableSourceSurface(ConnectedPin, GenerationContext, ParentSurfaceData);

				if (ParentSurfaceData.NodeMaterial)
				{
					UEdGraphNode* NodeMaterial = const_cast<UCustomizableObjectNodeMaterial*>(ParentSurfaceData.NodeMaterial);

					if (NodeMaterial->IsA(UCustomizableObjectNodeMaterial::StaticClass()) && !NodeMaterial->IsA(UCustomizableObjectNodeCopyMaterial::StaticClass()))
					{
						TypedNodeMaterial = static_cast<UCustomizableObjectNodeMaterial*>(NodeMaterial);
						TypedNodeMat = TypedNodeMaterial;
					}
					else
					{
						GenerationContext.Compiler->CompilerLog(LOCTEXT("CopyMaterialInput", "Copy Material Node can only have a Material Node as an input."), TypedDerivedNodeCopyMaterial);
					}
				}
			}
		}

		const UEdGraphPin* ConnectedMaterialPin = FollowInputPin(*TypedNodeMat->GetMeshPin());
		// Warn when texture connections are  improperly used by connecting them directly to material inputs when no layout is used
		// TODO: delete the if clause and the warning when static meshes are operational again
		if (ConnectedMaterialPin)
		{
			if (const UEdGraphPin* StaticMeshPin = FindMeshBaseSource(*ConnectedMaterialPin, true))
			{
				const UCustomizableObjectNode* StaticMeshNode = CastChecked<UCustomizableObjectNode>(StaticMeshPin->GetOwningNode());
				GenerationContext.Compiler->CompilerLog(LOCTEXT("UnsupportedStaticMeshes", "Static meshes are currently not supported as material meshes"), StaticMeshNode);
			}
		}

		mu::NodeSurfaceNewPtr SurfNode = new mu::NodeSurfaceNew();
		Result = SurfNode;

		int32 ReferencedMaterialsIndex = -1;
		if (TypedNodeMat->Material)
		{
			const int32 lastMaterialAmount = GenerationContext.ReferencedMaterials.Num();
			ReferencedMaterialsIndex = GenerationContext.ReferencedMaterials.AddUnique(TypedNodeMat->Material);
			// Used ReferencedMaterialsIndex instead of TypedNodeMat->Material->GetName() to prevent material name collisions

			// Set shared surface Id to reuse materials and textures between LODs if automatic LODs from mesh is being used
			UpdateSharedSurfaceId(GenerationContext, TypedNodeMat, SurfNode);

			// Take slot name from skeletal mesh if one can be found, else leave empty.
			// Keep Referenced Materials and Materail Slot Names synchronized even if no material name can be found.
			const bool IsNewSlotName = GenerationContext.ReferencedMaterialSlotNames.Num() == ReferencedMaterialsIndex;
			check(IsNewSlotName == lastMaterialAmount < GenerationContext.ReferencedMaterials.Num());
			bool SlotNameFound = false;
			check(GenerationContext.ReferencedMaterialSlotNames.Num() >= ReferencedMaterialsIndex);
			if (IsNewSlotName || GenerationContext.ReferencedMaterialSlotNames[ReferencedMaterialsIndex].IsNone())
			{
				if (ConnectedMaterialPin)
				{
					if (const UEdGraphPin* SkeletalMeshPin = FindMeshBaseSource(*ConnectedMaterialPin, false))
					{
						if (const UCustomizableObjectNodeSkeletalMesh* SkeletalMeshNode = Cast<UCustomizableObjectNodeSkeletalMesh>(SkeletalMeshPin->GetOwningNode()))
						{
							if (const FSkeletalMaterial* SkeletalMaterial = SkeletalMeshNode->GetSkeletalMaterialFor(*SkeletalMeshPin))
							{
								if (IsNewSlotName)
								{
									GenerationContext.ReferencedMaterialSlotNames.Add(SkeletalMaterial->MaterialSlotName);
									SlotNameFound = true;
								}
								else if (GenerationContext.ReferencedMaterialSlotNames[ReferencedMaterialsIndex].IsNone())
								{
									GenerationContext.ReferencedMaterialSlotNames[ReferencedMaterialsIndex] = SkeletalMaterial->MaterialSlotName;
									SlotNameFound = true;
								}
							}

							//TODO(max): Add support for table nodes. Convert this to a function as it will be also used in the GenerateMutableSourceTable file
						}
					}
				}

				// No name was found, we need to keep index parity, so we add empty value. We may find the value later.
				if (IsNewSlotName && !SlotNameFound)
				{
					GenerationContext.ReferencedMaterialSlotNames.Add(FName(NAME_None));
				}
			}
		}

		mu::NodeMeshPtr MeshNode;

		TypedNodeMat = TypedNodeCopyMaterial; // NodeCopyMaterial. Start reading mesh pin. Set TypedNodeMat pointer to NodeCopyMaterial

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeMat->GetMeshPin()))
		{
			FMutableGraphMeshGenerationData MeshData;
			MeshNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, MeshData, false, false);

			if (MeshNode)
			{
				SurfNode->SetMeshCount(1);

				mu::NodeMeshFormatPtr MeshFormatNode = new mu::NodeMeshFormat();
				MeshFormatNode->SetSource(MeshNode.get());
				SetSurfaceFormat( GenerationContext,
						MeshFormatNode->GetVertexBuffers(), MeshFormatNode->GetIndexBuffers(), MeshData,
						GenerationContext.Options.CustomizableObjectNumBoneInfluences,
						GenerationContext.Options.bRealTimeMorphTargetsEnabled, 
						GenerationContext.Options.bClothingEnabled,
						GenerationContext.Options.b16BitBoneWeightsEnabled);
				MeshFormatNode->SetMessageContext(Node);

				SurfNode->SetMesh(0, MeshFormatNode);
			}
			else
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("MeshFailed", "Mesh generation failed."), Node);
			}
		}

		TypedNodeMat = TypedNodeMaterial; // NodeCopyMaterial. End reading mesh pin. Set TypedNodeMat pointer back to the parent NodeMaterial

		TMap<FString, float> TextureNameToProjectionResFactor;
		FString AlternateResStateName;

		bool bTableMaterialPinLinked = TypedNodeMat->GetMaterialAssetPin() && FollowInputPin(*TypedNodeMat->GetMaterialAssetPin()) != nullptr;
		FString TableColumnName;

		// Checking if we should not use the material of the table node even if it is linked to the material node
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeMat->GetMaterialAssetPin()))
		{
			if (const UCustomizableObjectNodeTable* TypedNodeTable = Cast< UCustomizableObjectNodeTable >(ConnectedPin->GetOwningNode()))
			{
				TableColumnName = ConnectedPin->PinFriendlyName.ToString();

				if (UMaterialInstance * TableMaterial = TypedNodeTable->GetColumnDefaultAssetByType<UMaterialInstance>(ConnectedPin))
				{
					// Checking if the reference material of the Table Node has the same parent as the material of the Material Node 
					if (!TypedNodeMat->Material || TableMaterial->GetMaterial() != TypedNodeMat->Material->GetMaterial())
					{
						bTableMaterialPinLinked = false;

						GenerationContext.Compiler->CompilerLog(LOCTEXT("DifferentParentMaterial","The Deafult Material Instance of the Data Table must have the same Parent Material."), TypedNodeMat);
					}
				}
				else
				{
					FText Msg = FText::Format(LOCTEXT("DefaultValueNotFound", "Couldn't find a default value in the data table's struct for the column {0}. The default value is null or not a Material Instance."), FText::FromString(TableColumnName));
					GenerationContext.Compiler->CompilerLog(Msg, Node);

					bTableMaterialPinLinked = false;
				}
			}
		}

		const int32 NumImages = TypedNodeMat->GetNumParameters(EMaterialParameterType::Texture);
		SurfNode->SetImageCount(NumImages);

		for (int32 ImageIndex = 0; ImageIndex < NumImages; ++ImageIndex)
		{
			const UEdGraphPin* ImagePin = TypedNodeMat->GetParameterPin(EMaterialParameterType::Texture, ImageIndex);

			const bool bIsImagePinLinked = ImagePin && FollowInputPin(*ImagePin);

			if (bIsImagePinLinked && !TypedNodeMaterial->IsImageMutableMode(ImageIndex))
			{
				if (const UEdGraphPin* ConnectedPin = FollowInputPin(*ImagePin))
				{
					// This is a connected pass-through texture that simply has to be passed to the core
					mu::Ptr<mu::NodeImage> PassThroughImagePtr = GenerateMutableSourceImage(ConnectedPin, GenerationContext, 0);
					SurfNode->SetImage(ImageIndex, PassThroughImagePtr);

					const FString ImageName = TypedNodeMat->GetParameterName(EMaterialParameterType::Texture, ImageIndex).ToString();
					FString SurfNodeImageName = FString::Printf(TEXT("%d"), GenerationContext.ImageProperties.Num());
					SurfNode->SetImageName(ImageIndex, StringCast<ANSICHAR>(*SurfNodeImageName).Get());
					int32 UVLayout = TypedNodeMat->GetImageUVLayout(ImageIndex);

					if (UVLayout >= 0)
					{
						FString msg = FString::Printf(TEXT("Passthrough texture [%s] will ignore layout despite set to use layout [%d]"), *ImageName, UVLayout);
						GenerationContext.Compiler->CompilerLog(FText::FromString(msg), Node, EMessageSeverity::Warning);

					}

					SurfNode->SetImageLayoutIndex(ImageIndex, -1);
					SurfNode->SetImageAdditionalNames(ImageIndex, StringCast<ANSICHAR>(*TypedNodeMat->Material->GetName()).Get(), StringCast<ANSICHAR>(*ImageName).Get());

					// We don't need a reference texture or props here, but we do need the parameter name.
					FGeneratedImageProperties Props;
					Props.TextureParameterName = ImageName;
					GenerationContext.ImageProperties.Add(Props);
					SurfaceData.ImageProperties = Props;
				}
			}
			else
			{
				mu::NodeImagePtr GroupProjectionImg;
				UTexture2D* GroupProjectionReferenceTexture = nullptr;
				const FString ImageName = TypedNodeMat->GetParameterName(EMaterialParameterType::Texture, ImageIndex).ToString();
				const FGuid ImageId = TypedNodeMat->GetParameterId(EMaterialParameterType::Texture, ImageIndex);

				FString MaterialImageId = FGroupProjectorImageInfo::GenerateId(TypedNodeMat, ImageIndex);
				bool bShareProjectionTexturesBetweenLODs = false;
				FGroupProjectorImageInfo* ProjectorInfo = GenerationContext.GroupProjectorLODCache.Find(MaterialImageId);

				if (!ProjectorInfo) // No previous LOD of this material generated the image.
				{
					bool bIsGroupProjectorImage = false;

					GroupProjectionImg = GenerateMutableGroupProjection(LOD, ImageIndex, MeshNode, GenerationContext,
						TypedNodeMat, bShareProjectionTexturesBetweenLODs, bIsGroupProjectorImage,
						GroupProjectionReferenceTexture, TextureNameToProjectionResFactor, AlternateResStateName,
						nullptr);

					if (GroupProjectionImg.get() || TypedNodeMaterial->IsImageMutableMode(ImageIndex))
					{
						// Get the reference texture
						UTexture2D* ReferenceTexture = nullptr;

						GenerationContext.CurrentMaterialTableParameter = ImageName;
						GenerationContext.CurrentMaterialTableParameterId = ImageId.ToString();

						if (GroupProjectionImg.get() && GroupProjectionReferenceTexture)
						{
							ReferenceTexture = GroupProjectionReferenceTexture;
						}
						else
						{
							ReferenceTexture = TypedNodeMat->GetImageReferenceTexture(ImageIndex);

							if (!ReferenceTexture && !GroupProjectionImg.get())
							{
								if (ImagePin)
								{
									if (const UEdGraphPin* ConnectedPin = FollowInputPin(*ImagePin))
									{
										ReferenceTexture = FindReferenceImage(ConnectedPin, GenerationContext);
									}
								}
								if (!ReferenceTexture && bTableMaterialPinLinked)
								{
									if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeMat->GetMaterialAssetPin()))
									{
										ReferenceTexture = FindReferenceImage(ConnectedPin, GenerationContext);
									}
								}
								if (!ReferenceTexture)
								{
									ReferenceTexture = TypedNodeMat->GetImageValue(ImageIndex);
								}
							}
						}

						FGeneratedImageProperties Props;
						if (ReferenceTexture)
						{
							// Store properties for the generated images
							Props.TextureParameterName = ImageName;
							Props.CompressionSettings = ReferenceTexture->CompressionSettings;
							Props.Filter = ReferenceTexture->Filter;
							Props.SRGB = ReferenceTexture->SRGB;
							Props.LODBias = 0;
							Props.MipGenSettings = ReferenceTexture->MipGenSettings;
							Props.MaxTextureSize = GetMaxTextureSize(ReferenceTexture, GenerationContext);
							Props.LODGroup = ReferenceTexture->LODGroup;
							Props.AddressX = ReferenceTexture->AddressX;
							Props.AddressY = ReferenceTexture->AddressY;
							Props.bFlipGreenChannel = ReferenceTexture->bFlipGreenChannel;

							// TODO: MTBL-1081
							// TextureGroup::TEXTUREGROUP_UI does not support streaming. If we generate a texture that requires streaming and set this group, it will crash when initializing the resource. 
							// If LODGroup == TEXTUREGROUP_UI, UTexture::IsPossibleToStream() will return false and UE will assume all mips are loaded, when they're not, and crash.
							if (Props.LODGroup == TEXTUREGROUP_UI)
							{
								Props.LODGroup = TextureGroup::TEXTUREGROUP_Character;

								FString msg = FString::Printf(TEXT("The Reference texture [%s] is using TEXTUREGROUP_UI which does not support streaming. Please set a different TEXTURE group."),
									*ReferenceTexture->GetName(), *ImageName);
								GenerationContext.Compiler->CompilerLog(FText::FromString(msg), Node, EMessageSeverity::Info);
							}
						}
						else if (!GroupProjectionImg.get())
						{
							// warning!
							FString msg = FString::Printf(TEXT("The Reference texture for material image [%s] is not set and it couldn't be found automatically."), *ImageName);
							GenerationContext.Compiler->CompilerLog(FText::FromString(msg), Node);
						}

						GenerationContext.ImageProperties.Add(Props);
						SurfaceData.ImageProperties = Props;

						// Calculate the LODBias for this texture
						int32 LODBias = ComputeLODBias(GenerationContext, ReferenceTexture, Props.MaxTextureSize, TypedNodeMat, ImageIndex);

						GenerationContext.CurrentTextureLODBias = LODBias;

						// Generate the texture nodes
						mu::NodeImagePtr ImageNode = [&]()
						{
							if (TypedNodeMaterial->IsImageMutableMode(ImageIndex))
							{
								if (ImagePin)
								{
									if (const UEdGraphPin* ConnectedPin = FollowInputPin(*ImagePin))
									{
										return GenerateMutableSourceImage(ConnectedPin, GenerationContext, Props.MaxTextureSize);
									}
								}

								if (bTableMaterialPinLinked)
								{
									if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeMat->GetMaterialAssetPin()))
									{
										return GenerateMutableSourceImage(ConnectedPin, GenerationContext, Props.MaxTextureSize);
									}
								}

								// Else
								{
									UTexture2D* Texture2D = TypedNodeMat->GetImageValue(ImageIndex);

									const mu::NodeImageConstantPtr ConstImageNode = new mu::NodeImageConstant();
									GenerationContext.ArrayTextureUnrealToMutableTask.Add(FTextureUnrealToMutableTask(ConstImageNode, Texture2D, Node));

									return ResizeToMaxTextureSize(Props.MaxTextureSize, Texture2D, ConstImageNode);
								}
							}
							else
							{
								return mu::NodeImagePtr();
							}
						}();

						if (GroupProjectionImg.get())
						{
							ImageNode = GroupProjectionImg;
						}

						if (GenerationContext.Options.TargetPlatform && ReferenceTexture)
						{
							int LayerIndex = 0;

							TArray< TArray<FName> > PlatformFormats;
							GenerationContext.Options.TargetPlatform->GetTextureFormats(ReferenceTexture, PlatformFormats);

							bool bHaveFormatPlatformFormats = PlatformFormats.Num() > 0;

							mu::NodeImagePtr LastImage = ImageNode;

							// To apply LOD bias						
							if (LODBias > 0)
							{
								mu::NodeImageResizePtr ResizeImage = new mu::NodeImageResize();
								ResizeImage->SetBase(LastImage.get());
								ResizeImage->SetRelative(true);
								float factor = FMath::Pow(0.5f, LODBias);
								ResizeImage->SetSize(factor, factor);
								ResizeImage->SetMessageContext(Node);
								LastImage = ResizeImage;
							}

							mu::NodeImageMipmapPtr MipmapImage = new mu::NodeImageMipmap();
							MipmapImage->SetSource(LastImage.get());
							MipmapImage->SetMipmapGenerationSettings(mu::EMipmapFilterType::MFT_SimpleAverage, mu::EAddressMode::None, 1.0f, false);

							MipmapImage->SetMessageContext(Node);
							LastImage = MipmapImage;

							// Apply composite image. This needs to be computed after mipmaps generation. 	
							if (ReferenceTexture && ReferenceTexture->GetCompositeTexture() && ReferenceTexture->CompositeTextureMode != CTM_Disabled)
							{
								mu::NodeImageNormalCompositePtr CompositedImage = new mu::NodeImageNormalComposite();
								CompositedImage->SetBase(LastImage.get());
								CompositedImage->SetPower(ReferenceTexture->CompositePower);

								mu::ECompositeImageMode CompositeImageMode = [CompositeTextureMode = ReferenceTexture->CompositeTextureMode]() -> mu::ECompositeImageMode
								{
									switch (CompositeTextureMode)
									{
									case CTM_NormalRoughnessToRed: return mu::ECompositeImageMode::CIM_NormalRoughnessToRed;
									case CTM_NormalRoughnessToGreen: return mu::ECompositeImageMode::CIM_NormalRoughnessToGreen;
									case CTM_NormalRoughnessToBlue: return mu::ECompositeImageMode::CIM_NormalRoughnessToBlue;
									case CTM_NormalRoughnessToAlpha: return mu::ECompositeImageMode::CIM_NormalRoughnessToAlpha;

									default: return mu::ECompositeImageMode::CIM_Disabled;
									}
								}();

								CompositedImage->SetMode(CompositeImageMode);

								mu::NodeImageConstantPtr CompositeNormalImage = new mu::NodeImageConstant();

								UTexture2D* ReferenceCompositeNormalTexture = Cast<UTexture2D>(ReferenceTexture->GetCompositeTexture());
								if (ReferenceCompositeNormalTexture)
								{
									GenerationContext.ArrayTextureUnrealToMutableTask.Add(FTextureUnrealToMutableTask(CompositeNormalImage, ReferenceCompositeNormalTexture, Node, true));

									mu::NodeImageMipmapPtr NormalCompositeMipmapImage = new mu::NodeImageMipmap();
									NormalCompositeMipmapImage->SetSource(CompositeNormalImage);
									NormalCompositeMipmapImage->SetMipmapGenerationSettings(mu::EMipmapFilterType::MFT_SimpleAverage, mu::EAddressMode::None, 1.0f, true);

									CompositedImage->SetNormal(NormalCompositeMipmapImage);
								}

								LastImage = CompositedImage;
							}

							if (bHaveFormatPlatformFormats)
							{
								mu::NodeImageFormatPtr FormatImage = new mu::NodeImageFormat();
								FormatImage->SetSource(LastImage.get());
								FormatImage->SetFormat(mu::EImageFormat::IF_RGBA_UBYTE);
								FormatImage->SetMessageContext(Node);
								LastImage = FormatImage;

								if (GenerationContext.Options.bTextureCompression)
								{
									check(PlatformFormats[0].Num() > LayerIndex);

									const FString PlatformFormat = PlatformFormats[0][LayerIndex].ToString();

									// Remove platform prefix
									FString FormatWithoutPrefix = PlatformFormat;
									PlatformFormat.Split(TEXT("_"), nullptr, &FormatWithoutPrefix, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

									mu::EImageFormat mutableFormat = mu::EImageFormat::IF_NONE;
									mu::EImageFormat mutableFormatIfAlpha = mu::EImageFormat::IF_NONE;

									if (FormatWithoutPrefix == TEXT("AutoDXT"))
									{
										mutableFormat = mu::EImageFormat::IF_BC1;
										mutableFormatIfAlpha = mu::EImageFormat::IF_BC3;
									}
									else if (FormatWithoutPrefix == TEXT("DXT1")) mutableFormat = mu::EImageFormat::IF_BC1;
									else if (FormatWithoutPrefix == TEXT("DXT3")) mutableFormat = mu::EImageFormat::IF_BC2;
									else if (FormatWithoutPrefix == TEXT("DXT5")) mutableFormat = mu::EImageFormat::IF_BC3;
									else if (FormatWithoutPrefix == TEXT("BC1")) mutableFormat = mu::EImageFormat::IF_BC1;
									else if (FormatWithoutPrefix == TEXT("BC2")) mutableFormat = mu::EImageFormat::IF_BC2;
									else if (FormatWithoutPrefix == TEXT("BC3")) mutableFormat = mu::EImageFormat::IF_BC3;
									else if (FormatWithoutPrefix == TEXT("BC4")) mutableFormat = mu::EImageFormat::IF_BC4;
									else if (FormatWithoutPrefix == TEXT("BC5")) mutableFormat = mu::EImageFormat::IF_BC5;
									else if (FormatWithoutPrefix == TEXT("G8")) mutableFormat = mu::EImageFormat::IF_L_UBYTE;
									else if (FormatWithoutPrefix == TEXT("BGRA8")) mutableFormat = mu::EImageFormat::IF_RGBA_UBYTE;
									else if (PlatformFormat.Contains(TEXT("ASTC")))
									{
										if ((FormatWithoutPrefix == TEXT("AutoASTC")) || (FormatWithoutPrefix == TEXT("RGBAuto")))
										{
											mutableFormat = mu::EImageFormat::IF_ASTC_4x4_RGB_LDR;
											mutableFormatIfAlpha = mu::EImageFormat::IF_ASTC_4x4_RGBA_LDR;
										}
										else if (FormatWithoutPrefix == TEXT("RGB")) mutableFormat = mu::EImageFormat::IF_ASTC_4x4_RGB_LDR;
										else if (FormatWithoutPrefix == TEXT("RGBA")) mutableFormat = mu::EImageFormat::IF_ASTC_4x4_RGBA_LDR;
										else if (FormatWithoutPrefix == TEXT("NormalRG")) mutableFormat = mu::EImageFormat::IF_ASTC_4x4_RG_LDR;
										else if (PlatformFormat.Contains(TEXT("ASTC_NormalRG_Precise")))
										{
											// TODO: This is just a workaround to prevent the "Unexpected image format" warning below. ASTC_NormalRG_Precise is
											// not supported yet by Mutable so it forces IF_ASTC_4x4_RG_LDR as a replacement. It should be changed with a more
											// appropriate format or directly implement it
											mutableFormat = mu::EImageFormat::IF_ASTC_4x4_RG_LDR;

											const FString ReplacedImageFormatMsg = FString::Printf(TEXT("In object [%s] the unsupported ASTC_NormalRG_Precise image format is used, ASTC_4x4_RG_LDR will be used instead."), *GenerationContext.Object->GetName());
											const FText ReplacedImageFormatText = FText::FromString(ReplacedImageFormatMsg);
											GenerationContext.Compiler->CompilerLog(ReplacedImageFormatText, Node, EMessageSeverity::Info);
											UE_LOG(LogMutable, Log, TEXT("%s"), *ReplacedImageFormatMsg);
										}
										else if (PlatformFormat.Contains(TEXT("ASTC_NormalLA")))
										{
											// TODO: This is just a workaround to prevent the "Unexpected image format" warning below. ASTC_NormalLA is
											// not supported yet by Mutable so it forces IF_ASTC_4x4_RG_LDR as a replacement. It should be changed with a more
											// appropriate format or directly implement it
											mutableFormat = mu::EImageFormat::IF_ASTC_4x4_RG_LDR;

											const FString ReplacedImageFormatMsg2 = FString::Printf(TEXT("In object [%s] the unsupported ASTC_NormalLA image format is used, ASTC_4x4_RG_LDR will be used instead."), *GenerationContext.Object->GetName());
											const FText ReplacedImageFormatText2 = FText::FromString(ReplacedImageFormatMsg2);
											GenerationContext.Compiler->CompilerLog(ReplacedImageFormatText2, Node, EMessageSeverity::Info);
											UE_LOG(LogMutable, Log, TEXT("%s"), *ReplacedImageFormatMsg2);
										}
									}

									if (mutableFormat == mu::EImageFormat::IF_NONE)
									{
										// Format not supported by Mutable, use RBGA_UBYTE as default.
										mutableFormat = mu::EImageFormat::IF_RGBA_UBYTE;

										const FString UnexpectedImageFormatMsg = FString::Printf(TEXT("In object [%s] Unexpected image format [%s], RGBA_UBYTE will be used instead."), *GenerationContext.Object->GetName(), *PlatformFormat);
										const FText UnexpectedImageFormatText = FText::FromString(UnexpectedImageFormatMsg);
										GenerationContext.Compiler->CompilerLog(UnexpectedImageFormatText, Node);
										UE_LOG(LogMutable, Warning, TEXT("%s"), *UnexpectedImageFormatMsg);
									}

									FormatImage->SetFormat(mutableFormat, mutableFormatIfAlpha);
								}
							}

							ImageNode = LastImage;
						}

						mu::NodeImagePtr ImageNodePtr = ImageNode;
						SurfNode->SetImage(ImageIndex, ImageNodePtr);

						FString SurfNodeImageName = FString::Printf(TEXT("%d"), GenerationContext.ImageProperties.Num() - 1);

						// Encoding material layer in mutable name
						const int32 LayerIndex = TypedNodeMat->GetParameterLayerIndex(EMaterialParameterType::Texture, ImageIndex);
						if (LayerIndex != -1)
						{
							SurfNodeImageName += "-MutableLayerParam:" + FString::FromInt(LayerIndex);
						}

						SurfNode->SetImageName(ImageIndex, StringCast<ANSICHAR>(*SurfNodeImageName).Get());
						int32 UVLayout = TypedNodeMat->GetImageUVLayout(ImageIndex);
						SurfNode->SetImageLayoutIndex(ImageIndex, UVLayout);
						SurfNode->SetImageAdditionalNames(ImageIndex, StringCast<ANSICHAR>(*TypedNodeMat->Material->GetName()).Get(), StringCast<ANSICHAR>(*ImageName).Get());

						if (bShareProjectionTexturesBetweenLODs && bIsGroupProjectorImage)
						{
							// Add to the GroupProjectorLODCache to potentially reuse this projection texture in higher LODs
							ensure(LOD == GenerationContext.FirstLODAvailable);
							float* AlternateProjectionResFactor = TextureNameToProjectionResFactor.Find(ImageName);
							GenerationContext.GroupProjectorLODCache.Add(MaterialImageId,
								FGroupProjectorImageInfo(ImageNodePtr, ImageName, ImageName, TypedNodeMat,
									AlternateProjectionResFactor ? *AlternateProjectionResFactor : 0.f, AlternateResStateName, SurfNode, UVLayout));
						}
					}
				}
				else
				{
					ensure(LOD > GenerationContext.FirstLODAvailable);
					check(ProjectorInfo->SurfNode->GetImage(ImageIndex) == ProjectorInfo->ImageNode);
					SurfNode->SetImage(ImageIndex, ProjectorInfo->ImageNode);
					SurfNode->SetImageName(ImageIndex, StringCast<ANSICHAR>(*ProjectorInfo->TextureName).Get());
					SurfNode->SetImageLayoutIndex(ImageIndex, ProjectorInfo->UVLayout);

					TextureNameToProjectionResFactor.Add(ProjectorInfo->RealTextureName, ProjectorInfo->AlternateProjectionResolutionFactor);
					AlternateResStateName = ProjectorInfo->AlternateResStateName;
				}
			}
		}

		const int32 NumVectors = TypedNodeMat->GetNumParameters(EMaterialParameterType::Vector);
		SurfNode->SetVectorCount(NumVectors);
		for (int32 VectorIndex = 0; VectorIndex < NumVectors; ++VectorIndex)
		{
			const UEdGraphPin* VectorPin = TypedNodeMat->GetParameterPin(EMaterialParameterType::Vector, VectorIndex);
			bool bVectorPinConnected = VectorPin && FollowInputPin(*VectorPin);

			FString VectorName = TypedNodeMat->GetParameterName(EMaterialParameterType::Vector, VectorIndex).ToString();
			FGuid VectorId = TypedNodeMat->GetParameterId(EMaterialParameterType::Vector, VectorIndex);

			if (bVectorPinConnected)
			{				
				if (const UEdGraphPin* ConnectedPin = FollowInputPin(*VectorPin))
				{
					mu::NodeColourPtr ColorNode = GenerateMutableSourceColor(ConnectedPin, GenerationContext);

					// Encoding material layer in mutable name
					if (const int32 LayerIndex = TypedNodeMat->GetParameterLayerIndex(EMaterialParameterType::Vector, VectorIndex); LayerIndex != INDEX_NONE)
					{
						VectorName += "-MutableLayerParam:" + FString::FromInt(LayerIndex);
					}

					SurfNode->SetVector(VectorIndex, ColorNode);
					SurfNode->SetVectorName(VectorIndex, StringCast<ANSICHAR>(*VectorName).Get());
				}
			}
		}

		const int32 NumScalar = TypedNodeMat->GetNumParameters(EMaterialParameterType::Scalar);
		SurfNode->SetScalarCount(NumScalar);
		for (int32 ScalarIndex = 0; ScalarIndex < NumScalar; ++ScalarIndex)
		{
			const UEdGraphPin* ScalarPin = TypedNodeMat->GetParameterPin(EMaterialParameterType::Scalar, ScalarIndex);
			bool bScalarPinConnected = ScalarPin && FollowInputPin(*ScalarPin);

			FString ScalarName = TypedNodeMat->GetParameterName(EMaterialParameterType::Scalar, ScalarIndex).ToString();
			FGuid ScalarId = TypedNodeMat->GetParameterId(EMaterialParameterType::Scalar, ScalarIndex);

			if (bScalarPinConnected)
			{
				if (const UEdGraphPin* ConnectedPin = FollowInputPin(*ScalarPin))
				{
					mu::NodeScalarPtr ScalarNode = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);

					// Encoding material layer in mutable name
					if (const int32 LayerIndex = TypedNodeMat->GetParameterLayerIndex(EMaterialParameterType::Scalar, ScalarIndex); LayerIndex != INDEX_NONE)
					{
						ScalarName += "-MutableLayerParam:" + FString::FromInt(LayerIndex);
					}

					SurfNode->SetScalar(ScalarIndex, ScalarNode);
					SurfNode->SetScalarName(ScalarIndex, StringCast<ANSICHAR>(*ScalarName).Get());
				}
			}
		}

		// New method to pass the surface id as a scalar parameter
		{
			int32 MaterialIndex = NumScalar;
			SurfNode->SetScalarCount(NumScalar + 1);

			const UEdGraphPin* MaterialPin = TypedNodeMat->GetMaterialAssetPin();

			//Encoding name for material material id parameter
			FString MaterialName = "__MutableMaterialId";

			if (bTableMaterialPinLinked)
			{
				if (const UEdGraphPin* ConnectedPin = FollowInputPin(*MaterialPin))
				{
					GenerationContext.CurrentMaterialTableParameterId = MaterialName;
					mu::NodeScalarPtr ScalarNode = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);

					SurfNode->SetScalar(MaterialIndex, ScalarNode);
					SurfNode->SetScalarName(MaterialIndex, StringCast<ANSICHAR>(*MaterialName).Get());
				}
			}
			else
			{
				mu::NodeScalarConstantPtr ScalarNode = new mu::NodeScalarConstant();
				ScalarNode->SetValue(ReferencedMaterialsIndex);

				SurfNode->SetScalar(MaterialIndex, ScalarNode);
				SurfNode->SetScalarName(MaterialIndex, StringCast<ANSICHAR>(*MaterialName).Get());
			}
		}
		

		for (const FString& Tag : TypedNodeMat->Tags)
		{
			SurfNode->AddTag(StringCast<ANSICHAR>(*Tag).Get());
		}

		TArray<mu::NodeSurfaceNewPtr>* ArraySurfaceNodePtr = GenerationContext.MapMaterialNodeToMutableSurfaceNodeArray.Find(TypedNodeMat);
		if (ArraySurfaceNodePtr == nullptr)
		{
			TArray<mu::NodeSurfaceNewPtr> ArraySurfaceNode;
			ArraySurfaceNode.Add(SurfNode);
			ArraySurfaceNodePtr = &GenerationContext.MapMaterialNodeToMutableSurfaceNodeArray.Add(TypedNodeMat, ArraySurfaceNode);
		}
		else
		{
			ArraySurfaceNodePtr->AddUnique(SurfNode);
		}

		// If an alternate resolution for a particular state is present, clone the surface node, add the image resizing and inject the surface variation node
		if (TextureNameToProjectionResFactor.Num() > 0 && !AlternateResStateName.IsEmpty())
		{
			mu::NodeSurfaceNewPtr SurfNode2 = new mu::NodeSurfaceNew;

			SurfNode2->SetCustomID(ReferencedMaterialsIndex);

			SurfNode2->SetMeshCount(SurfNode->GetMeshCount());
			SurfNode2->SetMesh(0, SurfNode->GetMesh(0));

			SurfNode2->SetVectorCount(SurfNode->GetVectorCount());

			for (int32 VectorParamIndex = 0; VectorParamIndex < SurfNode->GetVectorCount(); ++VectorParamIndex)
			{
				SurfNode2->SetVector(VectorParamIndex, SurfNode->GetVector(VectorParamIndex));
				SurfNode2->SetVectorName(VectorParamIndex, SurfNode->GetVectorName(VectorParamIndex));
			}

			SurfNode2->SetScalarCount(SurfNode->GetScalarCount());

			for (int32 ScalarParamIndex = 0; ScalarParamIndex < SurfNode->GetScalarCount(); ++ScalarParamIndex)
			{
				SurfNode2->SetScalar(ScalarParamIndex, SurfNode->GetScalar(ScalarParamIndex));
				SurfNode2->SetScalarName(ScalarParamIndex, SurfNode->GetScalarName(ScalarParamIndex));
			}

			for (const FString& Tag : TypedNodeMat->Tags)
			{
				SurfNode2->AddTag(StringCast<ANSICHAR>(*Tag).Get());
			}

			SurfNode2->SetImageCount(SurfNode->GetImageCount());

			for (int32 ImageIndex = 0; ImageIndex < SurfNode->GetImageCount(); ++ImageIndex)
			{
				const FString ImageName = TypedNodeMat->GetParameterName(EMaterialParameterType::Texture, ImageIndex).ToString();
				if (float* ResolutionFactor = TextureNameToProjectionResFactor.Find(ImageName))
				{
					FString MaterialImageId = FGroupProjectorImageInfo::GenerateId(TypedNodeMat, ImageIndex);
					FGroupProjectorImageInfo* ProjectorInfo = GenerationContext.GroupProjectorLODCache.Find(MaterialImageId);

					if (!ProjectorInfo || !ProjectorInfo->bIsAlternateResolutionResized)
					{
						mu::NodeImageResizePtr NodeImageResize = new mu::NodeImageResize;
						NodeImageResize->SetRelative(true);
						NodeImageResize->SetSize(*ResolutionFactor, *ResolutionFactor);
						NodeImageResize->SetBase(SurfNode->GetImage(ImageIndex));

						SurfNode2->SetImage(ImageIndex, NodeImageResize);

						if (ProjectorInfo)
						{
							ensure(LOD == GenerationContext.FirstLODAvailable);
							ProjectorInfo->ImageResizeNode = NodeImageResize;
							ProjectorInfo->bIsAlternateResolutionResized = true;
						}
					}
					else
					{
						ensure(LOD > GenerationContext.FirstLODAvailable);
						check(ProjectorInfo->bIsAlternateResolutionResized);
						SurfNode2->SetImage(ImageIndex, ProjectorInfo->ImageResizeNode);
					}
				}
				else
				{
					SurfNode2->SetImage(ImageIndex, SurfNode->GetImage(ImageIndex));
				}

				SurfNode2->SetImageName(ImageIndex, SurfNode->GetImageName(ImageIndex));
				SurfNode2->SetImageLayoutIndex(ImageIndex, SurfNode->GetImageLayoutIndex(ImageIndex));
			}

			ArraySurfaceNodePtr->AddUnique(SurfNode2);

			mu::NodeSurfaceVariationPtr SurfaceVariation = new mu::NodeSurfaceVariation;
			SurfaceVariation->SetVariationType(mu::NodeSurfaceVariation::VariationType::State);
			SurfaceVariation->SetVariationCount(1);
			SurfaceVariation->SetVariationTag(0, StringCast<ANSICHAR>(*AlternateResStateName).Get());

			SurfaceVariation->AddDefaultSurface(&*SurfNode);
			SurfaceVariation->AddVariationSurface(0, &*SurfNode2);

			Result = SurfaceVariation;
		}
	}

	else if (UCustomizableObjectNodeExtendMaterial* TypedNodeExt = Cast<UCustomizableObjectNodeExtendMaterial>(Node))
	{
		mu::NodeSurfaceEditPtr SurfNode = new mu::NodeSurfaceEdit();
		Result = SurfNode;

		[&] // Using a lambda so control flow is easier to manage.
		{
			UCustomizableObjectNodeMaterial* OriginalParentMaterialNode = TypedNodeExt->GetParentMaterialNode();
			UCustomizableObjectNodeMaterial* ParentMaterialNode = OriginalParentMaterialNode;

			// Copy material
			UCustomizableObjectNodeMaterial* CopyMaterialParentMaterialNode = nullptr;
			const UCustomizableObjectNodeCopyMaterial* ParentMaterialCopyNodeCast = Cast<UCustomizableObjectNodeCopyMaterial>(OriginalParentMaterialNode);
			if (ParentMaterialCopyNodeCast)
			{
				CopyMaterialParentMaterialNode = ParentMaterialCopyNodeCast->GetMaterialNode();
				if (!CopyMaterialParentMaterialNode)
				{
					GenerationContext.Compiler->CompilerLog(LOCTEXT("BaseMissing", "Base Material not set (or not found)."), ParentMaterialCopyNodeCast);
					return;
				}
			}
		
			if (!ParentMaterialNode)
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("ParentMissing", "Parent node not set (or not found)."), Node);
				return;
			}

			if (TypedNodeExt->IsNodeOutDatedAndNeedsRefresh())
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("ParentMissing", "Parent node not set (or not found)."), Node);
				return;
			}

			// Parent, probably generated, will be retrieved from the cache
			FMutableGraphSurfaceGenerationData DummySurfaceData;
			mu::NodeSurfacePtr ParentNode = GenerateMutableSourceSurface(ParentMaterialNode->OutputPin(), GenerationContext, DummySurfaceData);
			SurfNode->SetParent(ParentNode.get());

			mu::NodeMeshPtr AddMeshNode;
			FMutableGraphMeshGenerationData MeshData;
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeExt->AddMeshPin()))
			{
				AddMeshNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, MeshData, true, false);
			}

			if (AddMeshNode)
			{
				mu::NodeMeshPtr MeshPtr = AddMeshNode;

				const TArray<UCustomizableObjectLayout*> Layouts = TypedNodeExt->GetLayouts();
				
				if (Layouts.Num())
				{
					//TODO: Implement support for multiple UV channels (e.g. Add warning for vertices which have a block in a layout but not in the other)
					mu::NodeMeshFragmentPtr MeshFrag = new mu::NodeMeshFragment();
				
					MeshFrag->SetMesh(MeshPtr);
					MeshFrag->SetLayoutOrGroup(0);
					MeshFrag->SetFragmentType(mu::NodeMeshFragment::FT_LAYOUT_BLOCKS);
					MeshFrag->SetBlockCount(Layouts[0]->Blocks.Num());
				
					for (int i = 0; i < Layouts[0]->Blocks.Num(); ++i)
					{
						MeshFrag->SetBlock(i, i);
					}
				
					MeshPtr = MeshFrag;
				}
				else
				{
					GenerationContext.Compiler->CompilerLog(LOCTEXT("ExtendMaterialLayoutMissing","Skeletal Mesh without Layout Node linked to an Extend Material. A 4x4 layout will be added as default layout."), Node);
				}

				mu::NodeMeshFormatPtr MeshFormat = new mu::NodeMeshFormat();
				SetSurfaceFormat( GenerationContext,
						MeshFormat->GetVertexBuffers(), MeshFormat->GetIndexBuffers(), MeshData,
						GenerationContext.Options.CustomizableObjectNumBoneInfluences,
						GenerationContext.Options.bRealTimeMorphTargetsEnabled, 
						GenerationContext.Options.bClothingEnabled,
						GenerationContext.Options.b16BitBoneWeightsEnabled);

				MeshFormat->SetSource(MeshPtr.get());

				mu::NodePatchMeshPtr MeshPatch = new mu::NodePatchMesh();
				MeshPatch->SetAdd(MeshFormat.get());

				SurfNode->SetMesh(MeshPatch.get());
				MeshPatch->SetMessageContext(Node);
			}

			if (ParentMaterialCopyNodeCast) // CopyMaterial swap.
			{
				ParentMaterialNode = CopyMaterialParentMaterialNode; 
			}
		
			const int32 NumImages = ParentMaterialNode->GetNumParameters(EMaterialParameterType::Texture);
			SurfNode->SetImageCount(NumImages);
			for (int32 ImageIndex = 0; ImageIndex < NumImages; ++ImageIndex)
			{
				mu::NodeImagePtr ImageNode;
				
				if (!ImageNode) // If
				{
					const FString MaterialImageId = FGroupProjectorImageInfo::GenerateId(TypedNodeExt, ImageIndex);
					const FGroupProjectorImageInfo* ProjectorInfo = GenerationContext.GroupProjectorLODCache.Find(MaterialImageId);

					if (ProjectorInfo)
					{
						ensure(LOD > GenerationContext.FirstLODAvailable);
						check(ProjectorInfo->SurfNode->GetImage(ImageIndex) == ProjectorInfo->ImageNode);
						ImageNode = ProjectorInfo->ImageNode;

						//TextureNameToProjectionResFactor.Add(ProjectorInfo->RealTextureName, ProjectorInfo->AlternateProjectionResolutionFactor);
						//AlternateResStateName = ProjectorInfo->AlternateResStateName;
					}
				}

				if (!ImageNode) // Else if
				{
					bool bShareProjectionTexturesBetweenLODs = false;
					bool bIsGroupProjectorImage = false;
					UTexture2D * GroupProjectionReferenceTexture = nullptr;
					TMap<FString, float> TextureNameToProjectionResFactor;
					FString AlternateResStateName;
					
					ImageNode = GenerateMutableGroupProjection(LOD, ImageIndex, AddMeshNode, GenerationContext,
						TypedNodeExt, bShareProjectionTexturesBetweenLODs, bIsGroupProjectorImage,
						GroupProjectionReferenceTexture, TextureNameToProjectionResFactor, AlternateResStateName,
						ParentMaterialNode);
				}
				
				if (!ImageNode) // Else if
				{
					const FGuid ImageId = ParentMaterialNode->GetParameterId(EMaterialParameterType::Texture, ImageIndex);
					
					if (TypedNodeExt->UsesImage(ImageId))
					{
						check(ParentMaterialNode->IsImageMutableMode(ImageIndex)); // Ensured at graph time. If it fails, something is wrong.
						
						float MaxTextureSize = 0.f;

						// The static_cast is correct because we now it's a material node due to the ParentMaterialNodeCast
						if (const mu::NodeSurfaceNewPtr ParentNode2 = static_cast<mu::NodeSurfaceNew*>(ParentNode.get()))
						{
							FString Aux(ParentNode2->GetImageName(ImageIndex));

							if (!Aux.IsEmpty())
							{
								check(Aux.IsNumeric());
								const int32 ImagePropertiesIndex = FCString::Atoi(*Aux);
								check(GenerationContext.ImageProperties.IsValidIndex(ImagePropertiesIndex));
								MaxTextureSize = GenerationContext.ImageProperties[ImagePropertiesIndex].MaxTextureSize;
							}
						}

						if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeExt->GetUsedImagePin(ImageId)))
						{
							ImageNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, DummySurfaceData.ImageProperties.MaxTextureSize);
						}
					}
				}

				if (ImageNode)
				{
					SurfNode->SetImage(ImageIndex, ImageNode);
				}

				const int32 UVIndex = ParentMaterialNode->GetImageUVLayout(ImageIndex);
				LayoutOperationUVNormalizedWarning(GenerationContext, ParentMaterialNode->GetMeshPin(), Node, UVIndex);
				LayoutOperationUVNormalizedWarning(GenerationContext, TypedNodeExt->AddMeshPin(), Node, UVIndex);

				// Validate if the ParentMaterialNode can be shared between LODs.
				UpdateSharedSurfaceId(GenerationContext, ParentMaterialNode, nullptr);
			}
		
			for (const FString& Tag : TypedNodeExt->Tags)
			{
				SurfNode->AddTag(StringCast<ANSICHAR>(*Tag).Get());
			}
		}();
	}

	else if (const UCustomizableObjectNodeRemoveMesh* TypedNodeRem = Cast<UCustomizableObjectNodeRemoveMesh>(Node))
	{
		mu::NodeSurfaceEditPtr SurfNode = new mu::NodeSurfaceEdit();
		Result = SurfNode;

		UCustomizableObjectNodeMaterial* ParentMaterialNode = TypedNodeRem->GetParentMaterialNode();
		if (!ParentMaterialNode)
		{
			GenerationContext.Compiler->CompilerLog(LOCTEXT("ParentMissing", "Parent node not set (or not found)."), Node);
		}
		else
		{
			// Parent, probably generated, will be retrieved from the cache
			FMutableGraphSurfaceGenerationData DummySurfaceData;
			mu::NodeSurfacePtr ParentNode = GenerateMutableSourceSurface(ParentMaterialNode->OutputPin(), GenerationContext, DummySurfaceData);
			SurfNode->SetParent(ParentNode.get());

			mu::NodeMeshPtr RemoveMeshNode;

			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeRem->RemoveMeshPin()))
			{
				FMutableGraphMeshGenerationData DummyMeshData;
				RemoveMeshNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, DummyMeshData, false, true);
			}

			if (RemoveMeshNode)
			{
				mu::NodePatchMeshPtr MeshPatch = new mu::NodePatchMesh();
				MeshPatch->SetRemove(RemoveMeshNode.get());
				SurfNode->SetMesh(MeshPatch.get());
				MeshPatch->SetMessageContext(Node);
			}

			// Validate if the ParentMaterialNode can be shared between LODs.
			UpdateSharedSurfaceId(GenerationContext, ParentMaterialNode, nullptr);
		}
	}

	else if (const UCustomizableObjectNodeRemoveMeshBlocks* TypedNodeRemBlocks = Cast<UCustomizableObjectNodeRemoveMeshBlocks>(Node))
	{
		mu::NodeSurfaceEditPtr SurfNode = new mu::NodeSurfaceEdit();
		Result = SurfNode;

		UCustomizableObjectNodeMaterial* ParentMaterialNode = TypedNodeRemBlocks->GetParentMaterialNode();

		if (!ParentMaterialNode)
		{
			GenerationContext.Compiler->CompilerLog(LOCTEXT("ParentMissing", "Parent node not set (or not found)."), Node);
		}
		else
		{
			// Parent, probably generated, will be retrieved from the cache
			FMutableGraphSurfaceGenerationData DummySurfaceData;
			mu::NodeSurfacePtr ParentNode = GenerateMutableSourceSurface(ParentMaterialNode->OutputPin(), GenerationContext, DummySurfaceData);
			SurfNode->SetParent(ParentNode.get());

			const UEdGraphPin* BaseSourcePin = FindMeshBaseSource(*ParentMaterialNode->OutputPin(), false);

			if (!BaseSourcePin)
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("ParentMissing", "Parent node not set (or not found)."), Node);
			}
			else
			{
				FMutableGraphMeshGenerationData DummyMeshData;
				mu::NodeMeshPtr BaseSourceMesh = GenerateMutableSourceMesh(BaseSourcePin, GenerationContext, DummyMeshData, false, false);

				mu::NodeMeshFragmentPtr MeshFrag = new mu::NodeMeshFragment();
				MeshFrag->SetMesh(BaseSourceMesh);
				MeshFrag->SetLayoutOrGroup(TypedNodeRemBlocks->ParentLayoutIndex);
				MeshFrag->SetFragmentType(mu::NodeMeshFragment::FT_LAYOUT_BLOCKS);
				MeshFrag->SetMessageContext(TypedNodeRemBlocks);

				// Add the block indices
				TArray<int> FoundBlockIndices;
				const TArray<UCustomizableObjectLayout*> Layouts = ParentMaterialNode->GetLayouts();

				if (!Layouts.IsValidIndex(TypedNodeRemBlocks->ParentLayoutIndex))
				{
					GenerationContext.Compiler->CompilerLog(LOCTEXT("RemoveMeshNode Invalid layout", "Node refers to an invalid texture layout. Add a layout to the selected parent material, then select some block(s)."), Node);
					UE_LOG(LogMutable, Warning, TEXT("In object [%s] UCustomizableObjectNodeRemoveMeshBlocks refers to an invalid texture layout index %d. Parent node has %d layouts."),
						*GenerationContext.Object->GetName(), TypedNodeRemBlocks->ParentLayoutIndex, Layouts.Num());
				}
				else
				{
					const UCustomizableObjectLayout* Layout = Layouts[TypedNodeRemBlocks->ParentLayoutIndex];

					if (Cast<UCustomizableObjectNodeMaterial>(ParentMaterialNode))
					{
						for (const FGuid& Id : TypedNodeRemBlocks->BlockIds)
						{
							int BlockIndex = Layout->FindBlock(Id);
							if (Layout->Blocks.IsValidIndex(BlockIndex))
							{
								FoundBlockIndices.Add(BlockIndex);
							}
							else
							{
								GenerationContext.Compiler->CompilerLog(LOCTEXT("RemoveMeshNode InvalidLayoutBlock", "Node refers to an invalid layout block."), Node);
								UE_LOG(LogMutable, Warning, TEXT("In object [%s] UCustomizableObjectNodeRemoveMeshBlocks refers to an invalid layout block id %s. Parent node has %d blocks."),
									*GenerationContext.Object->GetName(), *Id.ToString(), Layout->Blocks.Num());
							}
						}
					}
				}

				MeshFrag->SetBlockCount(FoundBlockIndices.Num());
				for (int i = 0; i < FoundBlockIndices.Num(); ++i)
				{
					MeshFrag->SetBlock(i, FoundBlockIndices[i]);
				}

				mu::NodePatchMeshPtr MeshPatch = new mu::NodePatchMesh();
				MeshPatch->SetRemove(MeshFrag.get());
				SurfNode->SetMesh(MeshPatch.get());
				MeshPatch->SetMessageContext(Node);
			}

			LayoutOperationUVNormalizedWarning(GenerationContext, ParentMaterialNode->GetMeshPin(), Node, TypedNodeRemBlocks->ParentLayoutIndex);

			// Validate if the ParentMaterialNode can be shared between LODs.
			UpdateSharedSurfaceId(GenerationContext, ParentMaterialNode, nullptr);
		}
	}

	else if (UCustomizableObjectNodeEditMaterial* TypedNodeEdit = Cast<UCustomizableObjectNodeEditMaterial>(Node))
	{
		mu::NodeSurfaceEditPtr SurfNode = new mu::NodeSurfaceEdit();
		Result = SurfNode;

		UCustomizableObjectNodeMaterial* ParentMaterialNode = TypedNodeEdit->GetParentMaterialNode();
		if (!ParentMaterialNode)
		{
			GenerationContext.Compiler->CompilerLog(LOCTEXT("ParentMissing", "Parent node not set (or not found)."), Node);
		}
		else
		{
			// Parent, probably generated, will be retrieved from the cache
			FMutableGraphSurfaceGenerationData DummySurfaceData;
			mu::NodeSurfacePtr ParentNode = GenerateMutableSourceSurface(ParentMaterialNode->OutputPin(), GenerationContext, DummySurfaceData);
			SurfNode->SetParent(ParentNode.get());


			const int32 NumImages = ParentMaterialNode->GetNumParameters(EMaterialParameterType::Texture);
			SurfNode->SetImageCount(NumImages);
			for (int32 ImageIndex = 0; ImageIndex < NumImages; ++ImageIndex)
			{
				const FGuid ImageId = ParentMaterialNode->GetParameterId(EMaterialParameterType::Texture, ImageIndex);

				if (TypedNodeEdit->UsesImage(ImageId))
				{
					check(ParentMaterialNode->IsImageMutableMode(ImageIndex)); // Ensured at graph time. If it fails, something is wrong.
					
					const UEdGraphPin* ConnectedImagePin = FollowInputPin(*TypedNodeEdit->GetUsedImagePin(ImageId));
					
					mu::NodePatchImagePtr ImagePatchNode = new mu::NodePatchImage;
					ImagePatchNode->SetMessageContext(Node);

					// \todo: expose these two options?
					ImagePatchNode->SetBlendType(mu::EBlendType::BT_BLEND);
					ImagePatchNode->SetApplyToAlphaChannel(true);

					// Calculate the LODBias for this texture
					UTexture2D* ReferenceTexture = ParentMaterialNode->GetImageReferenceTexture(ImageIndex);
					int32 LODBias = ComputeLODBias(GenerationContext, ReferenceTexture, ReferenceTexture ? ReferenceTexture->MaxTextureSize : 0, ParentMaterialNode, ImageIndex);

					GenerationContext.CurrentTextureLODBias = LODBias;

					mu::NodeImagePtr ImageNode = GenerateMutableSourceImage(ConnectedImagePin, GenerationContext, 0.f);
					ImagePatchNode->SetImage(ImageNode);

					const UEdGraphPin* ImageMaskPin = TypedNodeEdit->GetUsedImageMaskPin(ImageId);
					check(ImageMaskPin); // Ensured when reconstructing EditMaterial nodes. If it fails, something is wrong.
					
					if (const UEdGraphPin* ConnectedMaskPin = FollowInputPin(*ImageMaskPin))
					{
						mu::NodeImagePtr MaskNode = GenerateMutableSourceImage(ConnectedMaskPin, GenerationContext, 0.f);
						ImagePatchNode->SetMask(MaskNode);
					}

					// Add the block indices
					TArray<int> FoundBlockIndices;
					const TArray<UCustomizableObjectLayout*> Layouts = ParentMaterialNode->GetLayouts();

					if (!Layouts.IsValidIndex(TypedNodeEdit->ParentLayoutIndex))
					{
						GenerationContext.Compiler->CompilerLog(LOCTEXT("EditNode Invalid layout", "Node refers to an invalid texture layout."), Node);
						UE_LOG(LogMutable, Warning, TEXT("In object [%s] UCustomizableObjectNodeEditMaterial refers to an invalid texture layout index %d. Parent node has %d layouts."),
							*GenerationContext.Object->GetName(), TypedNodeEdit->ParentLayoutIndex, Layouts.Num());
					}
					else
					{
						if (const UCustomizableObjectNodeMaterial* ParentMaterial = Cast<UCustomizableObjectNodeMaterial>(ParentMaterialNode))
						{
							if (TypedNodeEdit->ParentLayoutIndex == ParentMaterial->GetImageUVLayout(ImageIndex))
							{
								const UCustomizableObjectLayout* Layout = Layouts[TypedNodeEdit->ParentLayoutIndex];

								for (const FGuid& Id : TypedNodeEdit->BlockIds)
								{
									int BlockIndex = Layout->FindBlock(Id);
									if (Layout->Blocks.IsValidIndex(BlockIndex))
									{
										FoundBlockIndices.Add(BlockIndex);
									}
									else
									{
										GenerationContext.Compiler->CompilerLog(LOCTEXT("EditNode InvalidLayoutBlock", "Node refers to an invalid layout block."), Node);
										UE_LOG(LogMutable, Warning, TEXT("In object [%s] UCustomizableObjectNodeEditMaterial refers to an invalid layout block id %s. Parent node has %d blocks."),
											*GenerationContext.Object->GetName(), *Id.ToString(), Layout->Blocks.Num());
									}
								}
							}
							else
							{
								GenerationContext.Compiler->CompilerLog(LOCTEXT("InvalidLayoutTexture", "Texture layout does not match the layout of Edit Material node or parent material changed."), Cast<UCustomizableObjectNode>(ConnectedImagePin->GetOwningNode()));
							}
						}
					}

					ImagePatchNode->SetBlockCount(FoundBlockIndices.Num());
					for (int BlockIndex = 0; BlockIndex < FoundBlockIndices.Num(); ++BlockIndex)
					{
						ImagePatchNode->SetBlock(BlockIndex, FoundBlockIndices[BlockIndex]);
					}

					SurfNode->SetPatch(ImageIndex, ImagePatchNode);
				}
				
				LayoutOperationUVNormalizedWarning(GenerationContext, ParentMaterialNode->GetMeshPin(), Node, ParentMaterialNode->GetImageUVLayout(ImageIndex));

				// Validate if the ParentMaterialNode can be shared between LODs.
				UpdateSharedSurfaceId(GenerationContext, ParentMaterialNode, nullptr);
			}
		}
	}

	else if (const UCustomizableObjectNodeMorphMaterial* TypedNodeMorph = Cast<UCustomizableObjectNodeMorphMaterial>(Node))
	{
		mu::NodeSurfaceEditPtr SurfNode = new mu::NodeSurfaceEdit();
		Result = SurfNode;

		UCustomizableObjectNodeMaterial* ParentMaterialNode = TypedNodeMorph->GetParentMaterialNode();
		if (!ParentMaterialNode)
		{
			GenerationContext.Compiler->CompilerLog(LOCTEXT("ParentMissing", "Parent node not set (or not found)."), Node);
		}
		else
		{
			// Parent, probably generated, will be retrieved from the cache
			FMutableGraphSurfaceGenerationData DummySurfaceData;
			mu::NodeSurfacePtr ParentNode = GenerateMutableSourceSurface(ParentMaterialNode->OutputPin(), GenerationContext, DummySurfaceData);
			SurfNode->SetParent(ParentNode.get());

			const UEdGraphPin* BaseSourcePin = FindMeshBaseSource(*ParentMaterialNode->OutputPin(), false);

			if (!BaseSourcePin)
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("ParentMissing", "Parent node not set (or not found)."), Node);
			}
			else
			{
				//Morph Mesh
				// The Mesh Base Source from the Parent Material will always have all LODs since it is not an auxiliary Mesh (clipping, reshape...).
				mu::MeshPtr MorphedSourceMesh = BuildMorphedMutableMesh(BaseSourcePin, TypedNodeMorph->MorphTargetName, GenerationContext, false); 

				mu::NodeMeshConstantPtr MorphedSourceMeshNode = new mu::NodeMeshConstant;
				MorphedSourceMeshNode->SetMessageContext(Node);					
				MorphedSourceMeshNode->SetValue(MorphedSourceMesh);
				
				SurfNode->SetMorph(MorphedSourceMeshNode);
				
				if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeMorph->FactorPin()))
				{
					UEdGraphNode* floatNode = ConnectedPin->GetOwningNode();
					bool validStaticFactor = true;
					if (const UCustomizableObjectNodeFloatParameter* floatParameterNode = Cast<UCustomizableObjectNodeFloatParameter>(floatNode))
					{
						if (floatParameterNode->DefaultValue < -1.0f || floatParameterNode->DefaultValue > 1.0f)
						{
							validStaticFactor = false;
							FString msg = FString::Printf(TEXT("Mesh morph nodes only accept factors between -1.0 and 1.0 inclusive but the default value of the float parameter node is (%f). Factor will be ignored."), floatParameterNode->DefaultValue);
							GenerationContext.Compiler->CompilerLog(FText::FromString(msg), Node);
						}
						if (floatParameterNode->ParamUIMetadata.MinimumValue < -1.0f)
						{
							validStaticFactor = false;
							FString msg = FString::Printf(TEXT("Mesh morph nodes only accept factors between -1.0 and 1.0 inclusive but the minimum UI value for the input float parameter node is (%f). Factor will be ignored."), floatParameterNode->ParamUIMetadata.MinimumValue);
							GenerationContext.Compiler->CompilerLog(FText::FromString(msg), Node);
						}
						if (floatParameterNode->ParamUIMetadata.MaximumValue > 1.0f)
						{
							validStaticFactor = false;
							FString msg = FString::Printf(TEXT("Mesh morph nodes only accept factors between -1.0 and 1.0 inclusive but the maximum UI value for the input float parameter node is (%f). Factor will be ignored."), floatParameterNode->ParamUIMetadata.MaximumValue);
							GenerationContext.Compiler->CompilerLog(FText::FromString(msg), Node);
						}
					}
					else if (const UCustomizableObjectNodeFloatConstant* floatConstantNode = Cast<UCustomizableObjectNodeFloatConstant>(floatNode))
					{
						if (floatConstantNode->Value < -1.0f || floatConstantNode->Value > 1.0f)
						{
							validStaticFactor = false;
							FString msg = FString::Printf(TEXT("Mesh morph nodes only accept factors between -1.0 and 1.0 inclusive but the value of the float constant node is (%f). Factor will be ignored."), floatConstantNode->Value);
							GenerationContext.Compiler->CompilerLog(FText::FromString(msg), Node);
						}
					}

					if (validStaticFactor)
					{
						mu::NodeScalarPtr FactorNode = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);
						SurfNode->SetFactor(FactorNode);
					}
				}
			}

			// Validate if the ParentMaterialNode can be shared between LODs.
			UpdateSharedSurfaceId(GenerationContext, ParentMaterialNode, nullptr);
		}
	}

	else if (const UCustomizableObjectNodeMaterialVariation* TypedNodeVar = Cast<UCustomizableObjectNodeMaterialVariation>(Node))
	{
		mu::NodeSurfaceVariationPtr SurfNode = new mu::NodeSurfaceVariation();
		Result = SurfNode;

		mu::NodeSurfaceVariation::VariationType muType = mu::NodeSurfaceVariation::VariationType::Tag;
		switch (TypedNodeVar->Type)
		{
		case ECustomizableObjectNodeMaterialVariationType::Tag: muType = mu::NodeSurfaceVariation::VariationType::Tag; break;
		case ECustomizableObjectNodeMaterialVariationType::State: muType = mu::NodeSurfaceVariation::VariationType::State; break;
		default:
			check(false);
			break;
		}
		SurfNode->SetVariationType(muType);

		for (const UEdGraphPin* ConnectedPin : FollowInputPinArray(*TypedNodeVar->DefaultPin()))
		{
			// Is it a modifier?
			FMutableGraphSurfaceGenerationData DummySurfaceData;
			mu::NodeSurfacePtr ChildNode = GenerateMutableSourceSurface(ConnectedPin, GenerationContext, DummySurfaceData);
			if (ChildNode)
			{
				SurfNode->AddDefaultSurface(ChildNode.get());
			}
			else
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("SurfaceFailed", "Surface generation failed."), Node);
			}
		}

		SurfNode->SetVariationCount(TypedNodeVar->Variations.Num());
		for (int VariationIndex = 0; VariationIndex < TypedNodeVar->Variations.Num(); ++VariationIndex)
		{
			mu::NodeSurfacePtr VariationSurfaceNode;

			if (UEdGraphPin* VariationPin = TypedNodeVar->VariationPin(VariationIndex))
			{
				SurfNode->SetVariationTag(VariationIndex, StringCast<ANSICHAR>(*TypedNodeVar->Variations[VariationIndex].Tag).Get());
				for (const UEdGraphPin* ConnectedPin : FollowInputPinArray(*VariationPin))
				{
					// Is it a modifier?
					FMutableGraphSurfaceGenerationData DummySurfaceData;
					mu::NodeSurfacePtr ChildNode = GenerateMutableSourceSurface(ConnectedPin, GenerationContext, DummySurfaceData);
					if (ChildNode)
					{
						SurfNode->AddVariationSurface(VariationIndex, ChildNode.get());
					}
					else
					{
						GenerationContext.Compiler->CompilerLog(LOCTEXT("SurfaceModifierFailed", "Surface generation failed."), Node);
					}
				}
			}
		}
	}

	else
	{
		GenerationContext.Compiler->CompilerLog(LOCTEXT("UnimplementedNode", "Node type not implemented yet."), Node);
	}


	if (Result)
	{
		Result->SetMessageContext(Node);
	}

	GenerationContext.Generated.Add(Key, FGeneratedData(Node, Result));
	GenerationContext.GeneratedNodes.Add(Node);

	return Result;
}


#undef LOCTEXT_NAMESPACE

