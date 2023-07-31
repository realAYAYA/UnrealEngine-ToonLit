// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSourceGroupProjector.h"

#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Containers/ContainersFwd.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Containers/StringConv.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/DataTable.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureDefines.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Text.h"
#include "Logging/TokenizedMessage.h"
#include "MaterialTypes.h"
#include "Materials/MaterialInterface.h"
#include "Math/IntVector.h"
#include "Math/Transform.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Guid.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"
#include "MuCO/CustomizableObjectUIData.h"
#include "MuCO/MultilayerProjector.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSource.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceProjector.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuCOE/Nodes/CustomizableObjectNodeAnimationPose.h"
#include "MuCOE/Nodes/CustomizableObjectNodeExtendMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeGroupProjectorParameter.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterialBase.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObjectGroup.h"
#include "MuR/Image.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/NodeColourConstant.h"
#include "MuT/NodeColourFromScalars.h"
#include "MuT/NodeImageConstant.h"
#include "MuT/NodeImageLayerColour.h"
#include "MuT/NodeImageMultiLayer.h"
#include "MuT/NodeImagePlainColour.h"
#include "MuT/NodeImageProject.h"
#include "MuT/NodeImageResize.h"
#include "MuT/NodeImageSwitch.h"
#include "MuT/NodeImageSwizzle.h"
#include "MuT/NodeMeshApplyPose.h"
#include "MuT/NodeMeshSwitch.h"
#include "MuT/NodeProjector.h"
#include "MuT/NodeRangeFromScalar.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeScalarConstant.h"
#include "MuT/NodeScalarEnumParameter.h"
#include "MuT/NodeScalarParameter.h"
#include "Templates/Casts.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectPtr.h"
#include "UObject/SoftObjectPtr.h"

class UPoseAsset;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


mu::NodeImagePtr GenerateMutableGroupProjection(const int32 NodeLOD, const int32 ImageIndex, mu::NodeMeshPtr MeshNode, FMutableGraphGenerationContext& GenerationContext,
	const UCustomizableObjectNodeMaterialBase* NodeMaterialBase, bool& bShareProjectionTexturesBetweenLODs, bool& bIsGroupProjectorImage,
	UTexture2D*& GroupProjectionReferenceTexture, TMap<FString, float>& TextureNameToProjectionResFactor, FString& AlternateResStateName,
	UCustomizableObjectNodeMaterial* ParentMaterial)
{
	if (!GenerationContext.ProjectorGroupMap.Num() || !MeshNode.get())
	{
		return mu::NodeImagePtr();
	}
	
	const UCustomizableObjectNodeMaterial* TypedNodeMat = Cast<UCustomizableObjectNodeMaterial>(NodeMaterialBase);
	check(!TypedNodeMat || !ParentMaterial); // Logical implication

	const UCustomizableObjectNodeExtendMaterial* TypedNodeExt = Cast<UCustomizableObjectNodeExtendMaterial>(NodeMaterialBase);
	check(!TypedNodeExt || ParentMaterial); // Logical implication

	check(TypedNodeMat || TypedNodeExt);
	
	TArray<mu::NodeImageProjectPtr> ImageNodes;
	TArray<FGroupProjectorTempData> ImageNodes_ProjectorTempData;

	int32 TextureSize = 512;

	for (TPair<const UCustomizableObjectNodeObjectGroup*, FGroupProjectorTempData>& Elem : GenerationContext.ProjectorGroupMap)
	{
		FGroupProjectorTempData& ProjectorTempData = Elem.Value;

		const int32& DropProjectionTextureAtLOD = ProjectorTempData.CustomizableObjectNodeGroupProjectorParameter->DropProjectionTextureAtLOD;
		if (DropProjectionTextureAtLOD >= 0 && NodeLOD >= DropProjectionTextureAtLOD)
		{
			continue;
		}

		bShareProjectionTexturesBetweenLODs |= ProjectorTempData.CustomizableObjectNodeGroupProjectorParameter->bShareProjectionTexturesBetweenLODs;

		if (!GroupProjectionReferenceTexture)
		{
			GroupProjectionReferenceTexture = ProjectorTempData.CustomizableObjectNodeGroupProjectorParameter->ReferenceTexture;
		}

		const bool bProjectToImage = [&]
		{
			if (TypedNodeMat)
			{
				const FString ParameterName = TypedNodeMat->GetParameterName(EMaterialParameterType::Texture, ImageIndex).ToString();
				return ParameterName == ProjectorTempData.CustomizableObjectNodeGroupProjectorParameter->MaterialChannelNameToConnect;
			}
			else
			{
				const FGuid ImageId = ParentMaterial->GetParameterId(EMaterialParameterType::Texture, ImageIndex);
				const FString ImagePinName = TypedNodeExt->GetUsedImagePin(ImageId)->PinName.ToString();
				return ImagePinName.StartsWith(ProjectorTempData.CustomizableObjectNodeGroupProjectorParameter->MaterialChannelNameToConnect);
			}
		}();
		
		if (bProjectToImage)
		{
			const bool bWarningReplacedImage = [&]
			{
				if (TypedNodeMat)
				{
					return TypedNodeMat->IsImageMutableMode(ImageIndex);
				}
				else
				{
					const FGuid ImageId = ParentMaterial->GetParameterId(EMaterialParameterType::Texture, ImageIndex);
					return TypedNodeExt->UsesImage(ImageId);
				}
			}();
			
			if (bWarningReplacedImage)
			{
				const FString ImageName = [&]
				{
					if (TypedNodeMat)
					{
						return TypedNodeMat->GetParameterName(EMaterialParameterType::Texture, ImageIndex).ToString();
					}
					else
					{
						const FGuid ImageId = ParentMaterial->GetParameterId(EMaterialParameterType::Texture, ImageIndex);
						return TypedNodeExt->GetUsedImagePin(ImageId)->PinName.ToString();
					}
				}();
				
				FString msg = FString::Printf(TEXT("Material image [%s] is connected to an image but will be replaced by a Group Projector."), *ImageName);
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), TypedNodeMat);
				continue;
			}
			
			mu::NodeImageProjectPtr ImageNode = new mu::NodeImageProject();
			bIsGroupProjectorImage = true;
			ImageNode->SetLayout(ProjectorTempData.CustomizableObjectNodeGroupProjectorParameter->UVLayout);

			{
				if (TypedNodeMat &&
					!ProjectorTempData.CustomizableObjectNodeGroupProjectorParameter->AlternateProjectionResolutionStateName.IsEmpty()
					&& ProjectorTempData.CustomizableObjectNodeGroupProjectorParameter->AlternateProjectionResolutionFactor > 0)
				{
					TextureNameToProjectionResFactor.Add(
						TypedNodeMat->GetParameterName(EMaterialParameterType::Texture, ImageIndex).ToString(),
						ProjectorTempData.CustomizableObjectNodeGroupProjectorParameter->AlternateProjectionResolutionFactor);

					if (!AlternateResStateName.IsEmpty() &&
						AlternateResStateName != ProjectorTempData.CustomizableObjectNodeGroupProjectorParameter->AlternateProjectionResolutionStateName &&
						!ProjectorTempData.bAlternateResStateNameWarningDisplayed)
					{
						FString msg = FString::Printf(TEXT("All 'Alternate Projection Resolution State Name' properties in Group Projector Parameter nodes connected to same Group node must have the same value or be blank. Only the value of the last connected node will be used."));
						GenerationContext.Compiler->CompilerLog(FText::FromString(msg), ProjectorTempData.CustomizableObjectNodeGroupProjectorParameter);
						ProjectorTempData.bAlternateResStateNameWarningDisplayed = true;
					}

					AlternateResStateName = ProjectorTempData.CustomizableObjectNodeGroupProjectorParameter->AlternateProjectionResolutionStateName;
				}
			}

			{
				mu::NodeScalarConstantPtr ScalarNode = new mu::NodeScalarConstant;
				ScalarNode->SetValue(120.f);
				ImageNode->SetAngleFadeStart(ScalarNode);
			}

			{
				mu::NodeScalarConstantPtr ScalarNode = new mu::NodeScalarConstant;
				ScalarNode->SetValue(150.f);
				ImageNode->SetAngleFadeEnd(ScalarNode);
			}

			mu::NodeMeshSwitchPtr MeshSwitchNode = new mu::NodeMeshSwitch;
			MeshSwitchNode->SetParameter(ProjectorTempData.PoseOptionsParameter);
			MeshSwitchNode->SetOptionCount(ProjectorTempData.CustomizableObjectNodeGroupProjectorParameter->OptionPoses.Num() + 1);
			MeshSwitchNode->SetOption(0, MeshNode);

			for (int32 SelectorIndex = 0; SelectorIndex < ProjectorTempData.CustomizableObjectNodeGroupProjectorParameter->OptionPoses.Num(); ++SelectorIndex)
			{
				if (ProjectorTempData.CustomizableObjectNodeGroupProjectorParameter->OptionPoses[SelectorIndex].OptionPose)
				{
					mu::NodeMeshApplyPosePtr NodeMeshApplyPose = CreateNodeMeshApplyPose(MeshNode, GenerationContext.Object,
						ProjectorTempData.PoseBoneDataArray[SelectorIndex].ArrayBoneName, ProjectorTempData.PoseBoneDataArray[SelectorIndex].ArrayTransform);

					if (!NodeMeshApplyPose)
					{
						FString msg = FString::Printf(TEXT("Couldn't get bone transform information from a Pose Asset."));
						GenerationContext.Compiler->CompilerLog(FText::FromString(msg), NodeMaterialBase);
					}

					MeshSwitchNode->SetOption(SelectorIndex + 1, NodeMeshApplyPose);
				}
				else
				{
					MeshSwitchNode->SetOption(SelectorIndex + 1, MeshNode);
				}
			}

			ImageNode->SetMesh(MeshSwitchNode);
			ImageNode->SetProjector(ProjectorTempData.NodeProjectorParameterPtr);

			mu::NodeImagePtr ImageToProject = ProjectorTempData.NodeImagePtr;
			{
				// Apply LODBias to the images to project

				int32 LODBias = ComputeLODBias(GenerationContext, GroupProjectionReferenceTexture, GroupProjectionReferenceTexture ? GroupProjectionReferenceTexture->MaxTextureSize : 0, TypedNodeMat, ImageIndex);

				if (LODBias > 0)
				{
					mu::NodeImageResizePtr ResizeImage = new mu::NodeImageResize();
					ResizeImage->SetBase(ImageToProject.get());
					ResizeImage->SetRelative(true);
					float factor = FMath::Pow(0.5f, LODBias);
					ResizeImage->SetSize(factor, factor);
					ImageToProject = ResizeImage;
				}

			}

			ImageNode->SetImage(ImageToProject);

			TextureSize = [&]
			{
				if (const int32 Size = ProjectorTempData.CustomizableObjectNodeGroupProjectorParameter->ProjectionTextureSize;
					Size <= 0 || !FMath::IsPowerOfTwo(Size))
				{
					// \todo: closest power of 2 bigger than the set value?
					return 512;
				}
				else
				{
					return Size;
				}
			}();
			ImageNode->SetImageSize( FUintVector2(TextureSize, TextureSize) );

			ImageNodes.Add(ImageNode);
			ImageNodes_ProjectorTempData.Add(ProjectorTempData);

			// Generate the projection mask-out texture cache
			const FString& MaskName = ProjectorTempData.CustomizableObjectNodeGroupProjectorParameter->MaskedOutAreaMaterialChannelName;

			if (!MaskName.IsEmpty())
			{
				const UMaterialInterface* MaskMaterial = TypedNodeMat ? TypedNodeMat->Material : ParentMaterial->Material;
				if (MaskMaterial)
				{
					UTexture* MaskTexture = nullptr;

					if (MaskMaterial->GetTextureParameterValue(FName(*MaskName), MaskTexture))
					{
						UTexture2D* MaskTexture2D = Cast<UTexture2D>(MaskTexture);

						if (MaskTexture2D && MaskTexture2D->Source.GetNumMips() > 0)
						{
							TArray64<uint8> TempData;
							MaskTexture2D->Source.GetMipData(TempData, 0, 0, 0);
							uint32 TotalTexels = MaskTexture2D->Source.GetSizeX() * MaskTexture2D->Source.GetSizeY();

							ETextureSourceFormat PixelFormat = MaskTexture2D->Source.GetFormat();
							int32 BytesPerPixel = MaskTexture2D->Source.GetBytesPerPixel();
							ensure((PixelFormat == TSF_BGRA8 && BytesPerPixel == 4) || BytesPerPixel == 1);

							{
								uint32 DataSize = TotalTexels * MaskTexture2D->Source.GetBytesPerPixel();
								FString MaskTexture2DPath = MaskTexture2D->GetPathName();

								if (!GenerationContext.MaskOutTextureCache.Find(MaskTexture2DPath) && DataSize > 1024) // Discard default or too small textures to have any mask detail
								{
									FMaskOutTexture& CachedTexture = GenerationContext.MaskOutTextureCache.Add(MaskTexture2DPath);
									
									CachedTexture.SetTextureSize(MaskTexture2D->Source.GetSizeX(), MaskTexture2D->Source.GetSizeY());

									for (size_t p = 0; p < DataSize / BytesPerPixel; ++p)
									{
										if (BytesPerPixel == 4)
										{
											CachedTexture.GetTexelReference(p) = TempData[p * 4 + 3] > 0; // Copy alpha channel of PF_R8G8B8A8
										}
										else if (BytesPerPixel == 1)
										{
											CachedTexture.GetTexelReference(p) = TempData[p] > 0;
										}
										else
										{
											check(false);
										}
									}
								}

								GenerationContext.MaskOutMaterialCache.Add(MaskMaterial->GetPathName(), MaskTexture2DPath);
							}
						}
					}
				}
			}
		}
	}

	if (ImageNodes.Num() == 0)
	{
		return mu::NodeImagePtr();
	}
	
	mu::NodeColourConstantPtr ZeroColorNode = new mu::NodeColourConstant();
	ZeroColorNode->SetValue(0.f, 0.f, 0.f);

	if (TextureSize <= 0 || !FMath::IsPowerOfTwo(TextureSize))
	{
		TextureSize = 512;
	}

	mu::NodeImagePlainColourPtr ZeroPlainColourNode = new mu::NodeImagePlainColour;
	ZeroPlainColourNode->SetSize(TextureSize, TextureSize);
	ZeroPlainColourNode->SetColour(ZeroColorNode);

	mu::NodeImageSwizzlePtr ZeroChannelNode = new mu::NodeImageSwizzle;
	ZeroChannelNode->SetFormat(mu::EImageFormat::IF_L_UBYTE);
	ZeroChannelNode->SetSource(0, ZeroPlainColourNode);
	ZeroChannelNode->SetSourceChannel(0, 2); // Just take a zeroed channel for the base alpha

	mu::NodeScalarConstantPtr OneConstantNode = new mu::NodeScalarConstant;
	OneConstantNode->SetValue(1.f);

	mu::NodeImagePtr ResultAlpha = ZeroChannelNode;
	mu::NodeImagePtr ResultImage = ZeroPlainColourNode;

	for (int32 i = 0; i < ImageNodes.Num(); ++i)
	{
		if (i > 0) // Resize the projection texture if necessary after the first iteration
		{
			int32 NewTextureSize = ImageNodes_ProjectorTempData.IsValidIndex(i) ? ImageNodes_ProjectorTempData[i].CustomizableObjectNodeGroupProjectorParameter->ProjectionTextureSize : TextureSize;

			if (NewTextureSize <= 0 || !FMath::IsPowerOfTwo(NewTextureSize))
			{
				NewTextureSize = TextureSize;
			}

			if (NewTextureSize != TextureSize)
			{
				TextureSize = NewTextureSize;

				ZeroPlainColourNode = new mu::NodeImagePlainColour;
				ZeroPlainColourNode->SetSize(TextureSize, TextureSize);
				ZeroPlainColourNode->SetColour(ZeroColorNode);
			}
		}

		mu::NodeImageSwizzlePtr ImageNodesAlphaChannelNode = new mu::NodeImageSwizzle;
		ImageNodesAlphaChannelNode->SetFormat(mu::EImageFormat::IF_L_UBYTE);
		ImageNodesAlphaChannelNode->SetSource(0, ImageNodes[i]);
		ImageNodesAlphaChannelNode->SetSourceChannel(0, 3);

		mu::NodeColourFromScalarsPtr ColourFromScalars = new mu::NodeColourFromScalars;
		ColourFromScalars->SetX(ImageNodes_ProjectorTempData[i].NodeOpacityParameter);
		ColourFromScalars->SetY(ImageNodes_ProjectorTempData[i].NodeOpacityParameter);
		ColourFromScalars->SetZ(ImageNodes_ProjectorTempData[i].NodeOpacityParameter);
		ColourFromScalars->SetW(OneConstantNode);

		mu::NodeImageLayerColourPtr OpacityMultiLayerNode = new mu::NodeImageLayerColour;
		OpacityMultiLayerNode->SetBlendType(mu::EBlendType::BT_MULTIPLY);
		OpacityMultiLayerNode->SetColour(ColourFromScalars);
		OpacityMultiLayerNode->SetBase(ImageNodesAlphaChannelNode);
		//OpacityMultiLayerNode->SetMask(OneChannelNode); // No mask needed

		mu::NodeImageSwizzlePtr MultiplySwizzleNode = new mu::NodeImageSwizzle;
		MultiplySwizzleNode->SetFormat(mu::EImageFormat::IF_L_UBYTE);
		MultiplySwizzleNode->SetSource(0, OpacityMultiLayerNode);
		MultiplySwizzleNode->SetSourceChannel(0, 0);

		mu::NodeImageMultiLayerPtr BaseAlphaMultiLayerNode = new mu::NodeImageMultiLayer;
		BaseAlphaMultiLayerNode->SetRange(ImageNodes_ProjectorTempData[i].NodeRange);
		BaseAlphaMultiLayerNode->SetBlendType(mu::EBlendType::BT_ALPHA_OVERLAY);
		BaseAlphaMultiLayerNode->SetBase(ResultAlpha);
		BaseAlphaMultiLayerNode->SetBlended(MultiplySwizzleNode);
		//BaseAlphaMultiLayerNode->SetMask(MultiplySwizzleNode); // No mask needed
		ResultAlpha = BaseAlphaMultiLayerNode;

		mu::NodeImageMultiLayerPtr BaseMultiLayerNode = new mu::NodeImageMultiLayer;
		BaseMultiLayerNode->SetRange(ImageNodes_ProjectorTempData[i].NodeRange);
		BaseMultiLayerNode->SetBlendType(mu::EBlendType::BT_BLEND);
		BaseMultiLayerNode->SetBase(ResultImage);
		BaseMultiLayerNode->SetBlended(ImageNodes[i]);
		BaseMultiLayerNode->SetMask(MultiplySwizzleNode);
		ResultImage = BaseMultiLayerNode;
	}

	mu::NodeImageSwizzlePtr SwizzleNodeR = new mu::NodeImageSwizzle;
	SwizzleNodeR->SetFormat(mu::EImageFormat::IF_L_UBYTE);
	SwizzleNodeR->SetSource(0, ResultImage);
	SwizzleNodeR->SetSourceChannel(0, 0);

	mu::NodeImageSwizzlePtr SwizzleNodeG = new mu::NodeImageSwizzle;
	SwizzleNodeG->SetFormat(mu::EImageFormat::IF_L_UBYTE);
	SwizzleNodeG->SetSource(0, ResultImage);
	SwizzleNodeG->SetSourceChannel(0, 1);

	mu::NodeImageSwizzlePtr SwizzleNodeB = new mu::NodeImageSwizzle;
	SwizzleNodeB->SetFormat(mu::EImageFormat::IF_L_UBYTE);
	SwizzleNodeB->SetSource(0, ResultImage);
	SwizzleNodeB->SetSourceChannel(0, 2);

	mu::NodeImageSwizzlePtr FinalSwizzleNode = new mu::NodeImageSwizzle;
	FinalSwizzleNode->SetFormat(mu::EImageFormat::IF_RGBA_UBYTE);
	FinalSwizzleNode->SetSource(0, SwizzleNodeR);
	FinalSwizzleNode->SetSourceChannel(0, 0);
	FinalSwizzleNode->SetSource(1, SwizzleNodeG);
	FinalSwizzleNode->SetSourceChannel(1, 0);
	FinalSwizzleNode->SetSource(2, SwizzleNodeB);
	FinalSwizzleNode->SetSourceChannel(2, 0);
	FinalSwizzleNode->SetSource(3, ResultAlpha);
	FinalSwizzleNode->SetSourceChannel(3, 0);

	return FinalSwizzleNode;
}


bool GenerateMutableSourceGroupProjector(const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext, const UCustomizableObjectNodeObjectGroup* originalGroup)
{
	check(Pin)
	check(originalGroup);
	RETURN_ON_CYCLE(*Pin, GenerationContext)

	CheckNumOutputs(*Pin, GenerationContext);
	
	UCustomizableObjectNode* Node = CastChecked<UCustomizableObjectNode>(Pin->GetOwningNode());

	const FGeneratedKey Key(reinterpret_cast<void*>(&GenerateMutableSourceGroupProjector), *Pin, *Node, GenerationContext, true);
	if (const FGeneratedData* Generated = GenerationContext.Generated.Find(Key))
	{
		return true;
	}

	mu::NodeProjectorPtr Result;
	
	if (UCustomizableObjectNodeGroupProjectorParameter* ProjParamNode = Cast<UCustomizableObjectNodeGroupProjectorParameter>(Node))
	{
		FGroupProjectorTempData GroupProjectorTempData;
		// The static cast works here because it's already known to be a mu::NodeProjectorParameter* because of the UE5 Cast in the previous line
		GroupProjectorTempData.NodeProjectorParameterPtr = static_cast<mu::NodeProjectorParameter*>(GenerateMutableSourceProjector(Pin, GenerationContext).get());

		if (GroupProjectorTempData.NodeProjectorParameterPtr)
		{
			// Use the projector parameter uid + suffix to identify parameters derived from this node
			FString ProjectorParamUid = GroupProjectorTempData.NodeProjectorParameterPtr->GetUid();

			// Add to UCustomizableObjectNodeGroupProjectorParameter::OptionImages those textures that are present in
			// UCustomizableObjectNodeGroupProjectorParameter::OptionImagesDataTable avoiding any repeated element
			TArray<FGroupProjectorParameterImage> ArrayOptionImage = ProjParamNode->GetFinalOptionImagesNoRepeat();

			if ((ProjParamNode->OptionImagesDataTable != nullptr) &&
				(ProjParamNode->DataTableTextureColumnName.ToString().IsEmpty() || (ProjParamNode->DataTableTextureColumnName.ToString() == "None")))
			{
				FString msg = FString::Printf(TEXT("The group projection node has a table assigned to the Option Images Data Table property, but no column to read textures is specified at the Data Table Texture Column Name property."));
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), ProjParamNode, EMessageSeverity::Error, true);
			}

			GroupProjectorTempData.CustomizableObjectNodeGroupProjectorParameter = ProjParamNode;

			mu::NodeScalarParameterPtr NodeScalarParam = new mu::NodeScalarParameter;
			FString NodeScalarParamName = ProjParamNode->ParameterName + FMultilayerProjector::NUM_LAYERS_PARAMETER_POSTFIX;
			NodeScalarParam->SetName(TCHAR_TO_ANSI(*(NodeScalarParamName)));
			NodeScalarParam->SetUid(TCHAR_TO_ANSI(*(ProjectorParamUid + FString("_NL"))));
			GenerationContext.AddParameterNameUnique(originalGroup, NodeScalarParamName);
			NodeScalarParam->SetDefaultValue(0.f);

			GenerationContext.ParameterUIDataMap.Add(NodeScalarParamName, FParameterUIData(
				NodeScalarParamName,
				ProjParamNode->ParamUIMetadata,
				EMutableParameterType::Int));

			mu::Ptr<mu::NodeRangeFromScalar> NodeRangeFromScalar = new mu::NodeRangeFromScalar;
			NodeRangeFromScalar->SetSize(NodeScalarParam);
			GroupProjectorTempData.NodeRange = NodeRangeFromScalar;
			GroupProjectorTempData.NodeProjectorParameterPtr->SetRangeCount(1);
			GroupProjectorTempData.NodeProjectorParameterPtr->SetRange(0, NodeRangeFromScalar);

			mu::NodeScalarEnumParameterPtr EnumParameterNode = new mu::NodeScalarEnumParameter;
			FString NodeEnumParamName = ProjParamNode->ParameterName + FMultilayerProjector::IMAGE_PARAMETER_POSTFIX;
			EnumParameterNode->SetName(TCHAR_TO_ANSI(*(NodeEnumParamName)));
			EnumParameterNode->SetUid(TCHAR_TO_ANSI(*(ProjectorParamUid + FString("_SI"))));
			GenerationContext.AddParameterNameUnique(originalGroup, NodeEnumParamName);
			EnumParameterNode->SetValueCount(ArrayOptionImage.Num());
			EnumParameterNode->SetDefaultValueIndex(0);
			EnumParameterNode->SetRangeCount(1);
			EnumParameterNode->SetRange(0, NodeRangeFromScalar);


			FParameterUIData ParameterUIData(NodeEnumParamName, ProjParamNode->ParamUIMetadata, EMutableParameterType::Int);
			ParameterUIData.IntegerParameterGroupType = ECustomizableObjectGroupType::COGT_ONE;
			ParameterUIData.ParamUIMetadata.ExtraInformation.Add(FString("UseThumbnails"));

			mu::NodeScalarEnumParameterPtr PoseEnumParameterNode = new mu::NodeScalarEnumParameter;
			FString PoseNodeEnumParamName = ProjParamNode->ParameterName + FMultilayerProjector::POSE_PARAMETER_POSTFIX;
			PoseEnumParameterNode->SetName(TCHAR_TO_ANSI(*(PoseNodeEnumParamName)));
			PoseEnumParameterNode->SetUid(TCHAR_TO_ANSI(*(ProjectorParamUid + FString("_SP"))));
			GenerationContext.AddParameterNameUnique(originalGroup, PoseNodeEnumParamName);
			PoseEnumParameterNode->SetValueCount(ProjParamNode->OptionPoses.Num() + 1);
			PoseEnumParameterNode->SetDefaultValueIndex(0);
			GroupProjectorTempData.PoseOptionsParameter = PoseEnumParameterNode;

			GenerationContext.ParameterUIDataMap.Add(PoseNodeEnumParamName, FParameterUIData(
				PoseNodeEnumParamName,
				ProjParamNode->ParamUIMetadata,
				EMutableParameterType::Int));

			mu::NodeScalarParameterPtr OpacityParameterNode = new mu::NodeScalarParameter;
			FString OpacityParameterNodeName = ProjParamNode->ParameterName + FMultilayerProjector::OPACITY_PARAMETER_POSTFIX;
			OpacityParameterNode->SetName(TCHAR_TO_ANSI(*(OpacityParameterNodeName)));
			OpacityParameterNode->SetUid(TCHAR_TO_ANSI(*(ProjectorParamUid + FString("_O"))));
			GenerationContext.AddParameterNameUnique(originalGroup, OpacityParameterNodeName);
			OpacityParameterNode->SetDefaultValue(0.75f);
			OpacityParameterNode->SetRangeCount(1);
			OpacityParameterNode->SetRange(0, NodeRangeFromScalar);
			FMutableParamUIMetadata OpacityMetadata = ProjParamNode->ParamUIMetadata;
			OpacityMetadata.ObjectFriendlyName = FString("Transparency");
			GroupProjectorTempData.NodeOpacityParameter = OpacityParameterNode;

			GenerationContext.ParameterUIDataMap.Add(OpacityParameterNodeName, FParameterUIData(
				OpacityParameterNodeName,
				OpacityMetadata,
				EMutableParameterType::Float));

			if (ArrayOptionImage.Num() == 0)
			{
				FString msg = FString::Printf(TEXT("The group projection node must have at least one option image connected to a texture or at least one valid element in Option Images Data Table."));
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), ProjParamNode, EMessageSeverity::Error, true);
				return false;
			}

			for (int ImageIndex = 0; ImageIndex < ArrayOptionImage.Num(); ++ImageIndex)
			{
				EnumParameterNode->SetValue(ImageIndex, (float)ImageIndex, TCHAR_TO_ANSI(*ArrayOptionImage[ImageIndex].OptionName));

				FMutableParamUIMetadata optionMetadata = ParameterUIData.ParamUIMetadata;
				optionMetadata.UIThumbnail = ArrayOptionImage[ImageIndex].OptionImage;
				ParameterUIData.ArrayIntegerParameterOption.Add(FIntegerParameterUIData(
					ArrayOptionImage[ImageIndex].OptionName,
					optionMetadata));
			}

			GenerationContext.ParameterUIDataMap.Add(NodeEnumParamName, ParameterUIData);

			PoseEnumParameterNode->SetValue(0, 0.f, "Default pose");

			for (int PoseIndex = 0; PoseIndex < ProjParamNode->OptionPoses.Num(); ++PoseIndex)
			{
				PoseEnumParameterNode->SetValue(PoseIndex + 1, (float)PoseIndex + 1.f, TCHAR_TO_ANSI(*ProjParamNode->OptionPoses[PoseIndex].PoseName));

				TArray<FString> ArrayBoneName;
				TArray<FTransform> ArrayTransform;
				UPoseAsset* PoseAsset = ProjParamNode->OptionPoses[PoseIndex].OptionPose;
				if (PoseAsset == nullptr) // Check if the slot has a selected pose. Could be left empty by the user
				{
					FString msg = FString::Printf(TEXT("The group projection node must have a pose assigned on each Option Poses element."));
					GenerationContext.Compiler->CompilerLog(FText::FromString(msg), ProjParamNode, EMessageSeverity::Error, true);
					return false;
				}

				check(GroupProjectorTempData.PoseBoneDataArray.Num() == PoseIndex);
				GroupProjectorTempData.PoseBoneDataArray.AddDefaulted(1);
				UCustomizableObjectNodeAnimationPose::StaticRetrievePoseInformation(PoseAsset, GenerationContext.GetCurrentComponentInfo().RefSkeletalMesh,
					GroupProjectorTempData.PoseBoneDataArray[PoseIndex].ArrayBoneName, GroupProjectorTempData.PoseBoneDataArray[PoseIndex].ArrayTransform);
			}

			mu::NodeImageSwitchPtr SwitchNode = new mu::NodeImageSwitch;
			SwitchNode->SetParameter(EnumParameterNode);
			SwitchNode->SetOptionCount(ArrayOptionImage.Num());

			bool bFoundUnlinkedPin = false;

			for (int SelectorIndex = 0; SelectorIndex < ArrayOptionImage.Num(); ++SelectorIndex)
			{
				if (ArrayOptionImage[SelectorIndex].OptionImage)
				{
					mu::NodeImageConstantPtr ImageNode = new mu::NodeImageConstant();
					GenerationContext.ArrayTextureUnrealToMutableTask.Add(FTextureUnrealToMutableTask(ImageNode, ArrayOptionImage[SelectorIndex].OptionImage, ProjParamNode));
					SwitchNode->SetOption(SelectorIndex, ImageNode);
				}
				else
				{
					//SwitchNode->SetOption(SelectorIndex, nullptr);
					bFoundUnlinkedPin = true;
				}
			}

			if (bFoundUnlinkedPin)
			{
				FString msg = FString::Printf(TEXT("The group projection node must have all the option images connected to a texture. Please set a texture for all the options."));
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), ProjParamNode);
			}

			GroupProjectorTempData.NodeImagePtr = SwitchNode;

			GenerationContext.ProjectorGroupMap.Add(originalGroup, GroupProjectorTempData);
		}
		else
		{
			GenerationContext.Compiler->CompilerLog(FText::FromString(TEXT("Error getting group projection properties.")), ProjParamNode, EMessageSeverity::Error, true);
			return false;
		}
	}
	

	GenerationContext.Generated.Add(Key, FGeneratedData(Node, Result));
	GenerationContext.GeneratedNodes.Add(Node);

	return true;
}

#undef LOCTEXT_NAMESPACE

