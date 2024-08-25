// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSourceImage.h"

#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceColor.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceFloat.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceMesh.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceProjector.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceTable.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTexture.h"
#include "MuCOE/Nodes/CustomizableObjectNodePassThroughTexture.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureBinarise.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureColourMap.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureFromChannels.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureFromColor.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureInterpolate.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureInvert.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureLayer.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureParameter.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureProject.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureSwitch.h"
#include "MuCOE/Nodes/CustomizableObjectNodePassThroughTextureSwitch.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureToChannels.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureTransform.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureSaturate.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureVariation.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "MuCO/UnrealToMutableTextureConversionUtils.h"
#include "MuT/NodeImageBinarise.h"
#include "MuT/NodeImageColourMap.h"
#include "MuT/NodeImageFormat.h"
#include "MuT/NodeImageInterpolate.h"
#include "MuT/NodeImageInvert.h"
#include "MuT/NodeImageLayer.h"
#include "MuT/NodeImageLayerColour.h"
#include "MuT/NodeImageParameter.h"
#include "MuT/NodeImagePlainColour.h"
#include "MuT/NodeImageProject.h"
#include "MuT/NodeImageResize.h"
#include "MuT/NodeImageSaturate.h"
#include "MuT/NodeImageSwitch.h"
#include "MuT/NodeImageSwizzle.h"
#include "MuT/NodeImageTable.h"
#include "MuT/NodeImageTransform.h"
#include "MuT/NodeImageVariation.h"
#include "MuT/NodeImageConstant.h"
#include "MuT/NodeScalarConstant.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


mu::ImagePtr ConvertTextureUnrealToMutable(UTexture2D* Texture, const UCustomizableObjectNode* Node, FCustomizableObjectCompiler* Compiler, bool bIsNormalComposite)
{     
	MUTABLE_CPUPROFILER_SCOPE(ConvertTextureUnrealToMutable);

	mu::Ptr<mu::Image> MutableImage = new mu::Image;
	EUnrealToMutableConversionError Error = EUnrealToMutableConversionError::Unknown;

	Error = ConvertTextureUnrealSourceToMutable(MutableImage.get(), Texture, bIsNormalComposite, 0);

	switch(Error)
	{
	case EUnrealToMutableConversionError::Success: break;
	case EUnrealToMutableConversionError::UnsupportedFormat:
		{
			Compiler->CompilerLog(
					LOCTEXT("UnsupportedImageFormat", 
							"Image format not supported."), 
					Node);
			break;
		}
	case EUnrealToMutableConversionError::CompositeImageDimensionMismatch:
		{
			Compiler->CompilerLog(
					LOCTEXT("CompositeImageDimensionMismatch", 
							"Composite image dimension mismatch."), 
					Node);
			break;
		}
	case EUnrealToMutableConversionError::CompositeUnsupportedFormat:
		{
			Compiler->CompilerLog(
					LOCTEXT("CompositeUnsupportedFormat", 
							"Composite image format not supported."), 
					Node);
			break;
		}
	default:
		{
			Compiler->CompilerLog(
					LOCTEXT("ImageConversionUnknownError", 
							"Image conversion unknown error."), 
					Node);
			break;
		}
	}

	return MutableImage;
}

mu::Ptr<mu::NodeImage> ResizeTextureByNumMips(const mu::Ptr<mu::NodeImage>& ImageConstant, int32 MipsToSkip)
{
	if (MipsToSkip > 0)
	{
		mu::NodeImageResizePtr ImageResize = new mu::NodeImageResize();
		ImageResize->SetBase(ImageConstant.get());
		ImageResize->SetRelative(true);
		const float factor = FMath::Pow(0.5f, MipsToSkip);
		ImageResize->SetSize(factor, factor);
		return ImageResize;
	}

	return ImageConstant;
}


mu::NodeImagePtr GenerateMutableSourceImage(const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext, int32 ReferenceTextureSize)
{
	check(Pin)
	RETURN_ON_CYCLE(*Pin, GenerationContext)

	CheckNumOutputs(*Pin, GenerationContext);
	
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UCustomizableObjectNode* Node = CastChecked<UCustomizableObjectNode>(Pin->GetOwningNode());

	const FGeneratedKey Key(reinterpret_cast<void*>(&GenerateMutableSourceImage), *Pin, *Node, GenerationContext, true);
	if (const FGeneratedData* Generated = GenerationContext.Generated.Find(Key))
	{
		return static_cast<mu::NodeImage*>(Generated->Node.get());
	}

	if (Node->IsNodeOutDatedAndNeedsRefresh())
	{
		Node->SetRefreshNodeWarning();
	}

	bool bDoNotAddToGeneratedCache = false;

	mu::NodeImagePtr Result;
	
	if (const UCustomizableObjectNodeTexture* TypedNodeTex = Cast<UCustomizableObjectNodeTexture>(Node))
	{
		UTexture2D* BaseTexture = TypedNodeTex->Texture;
		if (BaseTexture)
		{
			GenerationContext.AddParticipatingObject(*BaseTexture);
			
			// Check the specific image cache
			FGeneratedImageKey ImageKey = FGeneratedImageKey(Pin);
			mu::NodeImagePtr ImageNode;
			
			if (mu::NodeImagePtr* Cached = GenerationContext.GeneratedImages.Find(ImageKey))
			{
				ImageNode = *Cached;
			}
			else
			{
				mu::Ptr<mu::Image> ImageConstant = GenerateImageConstant(BaseTexture, GenerationContext, false);

				mu::Ptr<mu::NodeImageConstant> ReferenceImageNode = new mu::NodeImageConstant;
				ReferenceImageNode->SetValue( ImageConstant.get() );
				ImageNode = ReferenceImageNode;

				GenerationContext.GeneratedImages.Add(ImageKey, ImageNode);
			}

			const uint32 MipsToSkip = ComputeLODBiasForTexture(GenerationContext, *BaseTexture, nullptr, ReferenceTextureSize);
			Result = ResizeTextureByNumMips(ImageNode, MipsToSkip);
		}
		else
		{
			GenerationContext.Compiler->CompilerLog(LOCTEXT("MissingImage", "Missing image in texture node."), Node, EMessageSeverity::Warning);
		}
	}

	else if (const UCustomizableObjectNodeTextureParameter* TypedNodeParam = Cast<UCustomizableObjectNodeTextureParameter>(Node))
	{
		mu::Ptr<mu::NodeImageParameter> TextureNode = new mu::NodeImageParameter();

		GenerationContext.AddParameterNameUnique(Node, TypedNodeParam->ParameterName);

		TextureNode->SetName(TypedNodeParam->ParameterName);
		TextureNode->SetUid(GenerationContext.GetNodeIdUnique(Node).ToString());

		if (TypedNodeParam->DefaultValue)
		{
			TextureNode->SetDefaultValue(FName(TypedNodeParam->DefaultValue->GetPathName()));			
		}

		GenerationContext.ParameterUIDataMap.Add(TypedNodeParam->ParameterName, FParameterUIData(
			TypedNodeParam->ParameterName,
			TypedNodeParam->ParamUIMetadata,
			EMutableParameterType::Texture));

		// Force the same format that the default texture if any.
		mu::NodeImageFormatPtr FormatNode = new mu::NodeImageFormat();
		// \TODO: Take if from default?
		//if (TypedNodeParam->DefaultValue)
		//{
		//	FormatNode->SetFormat(mu::EImageFormat::IF_RGBA_UBYTE);
		//}
		//else
		{
			// Force an "easy format" on the texture.
			FormatNode->SetFormat(mu::EImageFormat::IF_RGBA_UBYTE);
		}
		FormatNode->SetSource(TextureNode);

		mu::NodeImageResizePtr ResizeNode = new mu::NodeImageResize();
		ResizeNode->SetBase(FormatNode);
		ResizeNode->SetRelative(false);

		FUintVector2 TextureSize(TypedNodeParam->TextureSizeX, TypedNodeParam->TextureSizeY);

		const UTexture2D* ReferenceTexture = TypedNodeParam->ReferenceValue;
		if (ReferenceTexture)
		{
			GenerationContext.AddParticipatingObject(*ReferenceTexture);

			const uint32 LODBias = ComputeLODBiasForTexture(GenerationContext, *TypedNodeParam->ReferenceValue, ReferenceTexture, ReferenceTextureSize);
			TextureSize.X = FMath::Max(ReferenceTexture->Source.GetSizeX() >> LODBias, 1);
			TextureSize.Y = FMath::Max(ReferenceTexture->Source.GetSizeY() >> LODBias, 1);
		}
		else
		{
			const int32 MaxNodeTextureSize = FMath::Max(TypedNodeParam->TextureSizeX, TypedNodeParam->TextureSizeY);
			if (MaxNodeTextureSize <= 0)
			{
				TextureSize.X = TextureSize.Y = 1;
				GenerationContext.Compiler->CompilerLog(LOCTEXT("TextureParameterSize0", "Texture size not specified. Add a reference texture or set a valid value to the Texture Size variables."), Node);
			}
			else if (ReferenceTextureSize > 0 && ReferenceTextureSize < MaxNodeTextureSize)
			{
				const uint32 MipsToSkip = FMath::CeilLogTwo(MaxNodeTextureSize) - FMath::CeilLogTwo(ReferenceTextureSize);
				TextureSize.X = FMath::Max(TextureSize.X >> MipsToSkip, (uint32)1);
				TextureSize.Y = FMath::Max(TextureSize.Y >> MipsToSkip, (uint32)1);
			}
		}

		ResizeNode->SetSize(TextureSize.X, TextureSize.Y);

		Result = ResizeNode;
	}

	else if (const UCustomizableObjectNodeMesh* TypedNodeMesh = Cast<UCustomizableObjectNodeMesh>(Node))
	{
		mu::NodeImageConstantPtr ImageNode = new mu::NodeImageConstant();
		Result = ImageNode;

		UTexture2D* Texture = TypedNodeMesh->FindTextureForPin(Pin);

		if (Texture)
		{
			ImageNode->SetValue(GenerateImageConstant(Texture, GenerationContext, false).get());

			const uint32 MipsToSkip = ComputeLODBiasForTexture(GenerationContext, *Texture, nullptr, ReferenceTextureSize);
			Result = ResizeTextureByNumMips(ImageNode, MipsToSkip);
		}
		else
		{
			Result = mu::NodeImagePtr();
		}
	}

	else if (const UCustomizableObjectNodeTextureInterpolate* TypedNodeInterp = Cast<UCustomizableObjectNodeTextureInterpolate>(Node))
	{
		mu::NodeImageInterpolatePtr ImageNode = new mu::NodeImageInterpolate();
		Result = ImageNode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeInterp->FactorPin()))
		{
			mu::NodeScalarPtr FactorNode = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);
			ImageNode->SetFactor(FactorNode);
		}

		int currentTarget = 0;
		for (int LayerIndex = 0; LayerIndex < TypedNodeInterp->GetNumTargets(); ++LayerIndex)
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeInterp->Targets(LayerIndex)))
			{
				mu::NodeImagePtr TargetNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);

				if (TargetNode)
				{
					ImageNode->SetTargetCount(currentTarget + 1);
					ImageNode->SetTarget(currentTarget, TargetNode);
					++currentTarget;
				}
			}
		}
	}

	else if (const UCustomizableObjectNodeTextureLayer* TypedNodeLayer = Cast<UCustomizableObjectNodeTextureLayer>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeLayer->BasePin()))
		{
			Result = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
		}

		for (int LayerIndex = 0; LayerIndex < TypedNodeLayer->GetNumLayers(); ++LayerIndex)
		{
			if (const UEdGraphPin* OtherPin = FollowInputPin(*TypedNodeLayer->LayerPin(LayerIndex)))
			{
				mu::NodeImagePtr MaskNode = nullptr;
				if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeLayer->MaskPin(LayerIndex)))
				{
					MaskNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
				}

				mu::EBlendType Type = mu::EBlendType::BT_BLEND;
				switch (TypedNodeLayer->Layers[LayerIndex].Effect)
				{
				case COTLE_MODULATE: Type = mu::EBlendType::BT_BLEND; break;
				case COTLE_MULTIPLY: Type = mu::EBlendType::BT_MULTIPLY; break;
				case COTLE_SOFTLIGHT: Type = mu::EBlendType::BT_SOFTLIGHT; break;
				case COTLE_HARDLIGHT: Type = mu::EBlendType::BT_HARDLIGHT; break;
				case COTLE_DODGE: Type = mu::EBlendType::BT_DODGE; break;
				case COTLE_BURN: Type = mu::EBlendType::BT_BURN; break;
				case COTLE_SCREEN: Type = mu::EBlendType::BT_SCREEN; break;
				case COTLE_OVERLAY: Type = mu::EBlendType::BT_OVERLAY; break;
				case COTLE_ALPHA_OVERLAY: Type = mu::EBlendType::BT_LIGHTEN; break;
				case COTLE_NORMAL_COMBINE: Type = mu::EBlendType::BT_NORMAL_COMBINE; break;
				default:
					GenerationContext.Compiler->CompilerLog(LOCTEXT("UnsupportedImageEffect", "Texture layer effect not supported. Setting to 'Blend'."), Node);
					break;
				}

				if (Type == mu::EBlendType::BT_BLEND && !MaskNode)
				{
					GenerationContext.Compiler->CompilerLog(LOCTEXT("ModulateWithoutMask", "Texture layer effect uses Modulate without a mask. It will replace everything below it!"), Node);
				}
				
				if (OtherPin->PinType.PinCategory == Schema->PC_Image)
				{
					mu::NodeImagePtr BlendNode = GenerateMutableSourceImage(OtherPin, GenerationContext, ReferenceTextureSize);

					mu::NodeImageLayerPtr LayerNode = new mu::NodeImageLayer;
					LayerNode->SetBlendType(Type);
					LayerNode->SetBase(Result);
					LayerNode->SetBlended(BlendNode);
					LayerNode->SetMask(MaskNode);
					Result = LayerNode;
				}

				else if (OtherPin->PinType.PinCategory == Schema->PC_Color)
				{
					mu::NodeColourPtr ColorNode = GenerateMutableSourceColor(OtherPin, GenerationContext);

					mu::NodeImageLayerColourPtr LayerNode = new mu::NodeImageLayerColour;
					LayerNode->SetBlendType(Type);
					LayerNode->SetBase(Result);
					LayerNode->SetColour(ColorNode);
					LayerNode->SetMask(MaskNode);
					Result = LayerNode;
				}

				// We need it here because we create multiple nodes.
				Result->SetMessageContext(Node);
			}
		}
	}

	else if (const UCustomizableObjectNodeTextureSwitch* TypedNodeTextureSwitch = Cast<UCustomizableObjectNodeTextureSwitch>(Node))
	{
		Result = [&]()
		{
			const UEdGraphPin* SwitchParameter = TypedNodeTextureSwitch->SwitchParameter();

			// Check Switch Parameter arity preconditions.
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*SwitchParameter))
			{
				mu::NodeScalarPtr SwitchParam = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);
				// Switch Param not generated
				if (!SwitchParam)
				{
					const FText Message = LOCTEXT("FailedToGenerateSwitchParam", "Could not generate switch enum parameter. Please refesh the switch node and connect an enum.");
					GenerationContext.Compiler->CompilerLog(Message, Node);
					
					return Result;
				}

				if (SwitchParam->GetType() != mu::NodeScalarEnumParameter::GetStaticType())
				{
					const FText Message = LOCTEXT("WrongSwitchParamType", "Switch parameter of incorrect type.");
					GenerationContext.Compiler->CompilerLog(Message, Node);

					return Result;
				}

				const int32 NumSwitchOptions = TypedNodeTextureSwitch->GetNumElements();

				mu::NodeScalarEnumParameter* EnumParameter = static_cast<mu::NodeScalarEnumParameter*>(SwitchParam.get());
				if (NumSwitchOptions != EnumParameter->GetValueCount())
				{
					const FText Message = LOCTEXT("MismatchedSwitch", "Switch enum and switch node have different number of options. Please refresh the switch node to make sure the outcomes are labeled properly.");
					GenerationContext.Compiler->CompilerLog(Message, Node);
				}

				mu::NodeImageSwitchPtr SwitchNode = new mu::NodeImageSwitch;
				SwitchNode->SetParameter(SwitchParam);
				SwitchNode->SetOptionCount(NumSwitchOptions);

				for (int SelectorIndex = 0; SelectorIndex < NumSwitchOptions; ++SelectorIndex)
				{
					if (const UEdGraphPin* TexturePin = FollowInputPin(*TypedNodeTextureSwitch->GetElementPin(SelectorIndex)))
					{
						SwitchNode->SetOption(SelectorIndex, GenerateMutableSourceImage(TexturePin, GenerationContext, ReferenceTextureSize));
					}
					else
					{
						const FText Message = LOCTEXT("MissingTexture", "Unable to generate texture switch node. Required connection not found.");
						GenerationContext.Compiler->CompilerLog(Message, Node);
						return Result;
					}
				}

				Result = SwitchNode;
				return Result;
			}
			else
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("NoEnumParamInSwitch", "Switch nodes must have an enum switch parameter. Please connect an enum and refesh the switch node."), Node);
				return Result;
			}
		}(); // invoke lambda.
	}

	else if (const UCustomizableObjectNodeTextureVariation* TypedNodeImageVar = Cast<const UCustomizableObjectNodeTextureVariation>(Node))
	{
		// UCustomizableObjectNodePassThroughTextureVariation nodes are also handled here
		mu::NodeImageVariationPtr TextureNode = new mu::NodeImageVariation();
		Result = TextureNode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeImageVar->DefaultPin()))
		{
			mu::NodeImagePtr ChildNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
			if (ChildNode)
			{
				TextureNode->SetDefaultImage(ChildNode.get());
			}
			else
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("TextureFailed", "Texture generation failed."), Node);
			}
		}

		TextureNode->SetVariationCount(TypedNodeImageVar->Variations.Num());
		for (int32 VariationIndex = 0; VariationIndex < TypedNodeImageVar->Variations.Num(); ++VariationIndex)
		{
			const UEdGraphPin* VariationPin = TypedNodeImageVar->VariationPin(VariationIndex);
			if (!VariationPin)
			{
				continue;
			}

			TextureNode->SetVariationTag(VariationIndex, TypedNodeImageVar->Variations[VariationIndex].Tag);
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*VariationPin))
			{
				mu::NodeImagePtr ChildNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
				TextureNode->SetVariationImage(VariationIndex, ChildNode.get());
			}
		}
	}

	else if (const UCustomizableObjectNodeTextureFromColor* TypedNodeFromColor = Cast<UCustomizableObjectNodeTextureFromColor>(Node))
	{
		mu::NodeColourPtr color;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFromColor->ColorPin()))
		{
			color = GenerateMutableSourceColor(ConnectedPin, GenerationContext);
		}

		mu::NodeImagePlainColourPtr ImageFromColour = new mu::NodeImagePlainColour;
		Result = ImageFromColour;

		if (color)
		{
			ImageFromColour->SetColour(color);
		}

		if (ReferenceTextureSize > 0)
		{
			ImageFromColour->SetSize(ReferenceTextureSize, ReferenceTextureSize);
		}
	}

	else if (const UCustomizableObjectNodeTextureFromChannels* TypedNodeFrom = Cast<UCustomizableObjectNodeTextureFromChannels>(Node))
	{
		mu::NodeImagePtr RNode, GNode, BNode, ANode;
		bool RGB = false;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFrom->RPin()))
		{
			RNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
			RGB = true;
		}
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFrom->GPin()))
		{
			GNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
			RGB = true;
		}
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFrom->BPin()))
		{
			BNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
			RGB = true;
		}
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFrom->APin()))
		{
			ANode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
		}

		mu::NodeImageSwizzlePtr SwizzleNode = new mu::NodeImageSwizzle;
		Result = SwizzleNode;

		if (RGB && ANode)
		{
			SwizzleNode->SetFormat(mu::EImageFormat::IF_RGBA_UBYTE);
			SwizzleNode->SetSource(0, RNode);
			SwizzleNode->SetSourceChannel(0, 0);
			SwizzleNode->SetSource(1, GNode);
			SwizzleNode->SetSourceChannel(1, 0);
			SwizzleNode->SetSource(2, BNode);
			SwizzleNode->SetSourceChannel(2, 0);
			SwizzleNode->SetSource(3, ANode);
			SwizzleNode->SetSourceChannel(3, 0);
		}
		else if (RGB)
		{
			SwizzleNode->SetFormat(mu::EImageFormat::IF_RGB_UBYTE);
			SwizzleNode->SetSource(0, RNode);
			SwizzleNode->SetSourceChannel(0, 0);
			SwizzleNode->SetSource(1, GNode);
			SwizzleNode->SetSourceChannel(1, 0);
			SwizzleNode->SetSource(2, BNode);
			SwizzleNode->SetSourceChannel(2, 0);
		}
		else if (RNode)
		{
			SwizzleNode->SetFormat(mu::EImageFormat::IF_L_UBYTE);
			SwizzleNode->SetSource(0, RNode);
			SwizzleNode->SetSourceChannel(0, 0);
		}
		else if (ANode)
		{
			SwizzleNode->SetFormat(mu::EImageFormat::IF_L_UBYTE);
			SwizzleNode->SetSource(0, ANode);
			SwizzleNode->SetSourceChannel(0, 0);
		}
	}

	else if (const UCustomizableObjectNodeTextureToChannels* TypedNodeTo = Cast<UCustomizableObjectNodeTextureToChannels>(Node))
	{
		mu::NodeImagePtr BaseNode;
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeTo->InputPin()))
		{
			BaseNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
		}

		mu::NodeImageSwizzlePtr SwizzleNode = new mu::NodeImageSwizzle;
		Result = SwizzleNode;
		SwizzleNode->SetFormat(mu::EImageFormat::IF_L_UBYTE);
		SwizzleNode->SetSource(0, BaseNode);

		if (Pin == TypedNodeTo->RPin())
		{
			SwizzleNode->SetSourceChannel(0, 0);
		}
		else if (Pin == TypedNodeTo->GPin())
		{
			SwizzleNode->SetSourceChannel(0, 1);
		}
		else if (Pin == TypedNodeTo->BPin())
		{
			SwizzleNode->SetSourceChannel(0, 2);
		}
		else if (Pin == TypedNodeTo->APin())
		{
			SwizzleNode->SetSourceChannel(0, 3);
		}
		else
		{
			check(false);
		}
	}

	else if (const UCustomizableObjectNodeTextureProject* TypedNodeProject = Cast<UCustomizableObjectNodeTextureProject>(Node))
	{
		mu::Ptr<mu::NodeImageProject> ImageNode = new mu::NodeImageProject();
		Result = ImageNode;

		if (!FollowInputPin(*TypedNodeProject->MeshPin()))
		{
			GenerationContext.Compiler->CompilerLog(LOCTEXT("MissingMeshInProjector", "Texture projector does not have a Mesh. It will be ignored. "), Node, EMessageSeverity::Warning);
			Result = nullptr;
		}

		ImageNode->SetLayout(TypedNodeProject->Layout);

		// Calculate the max TextureSize allowed using the ReferenceTextureSize and the Reference texture from the node
		int32 MaxReferenceTextureSizeInGame = ReferenceTextureSize;
		if (TypedNodeProject->ReferenceTexture)
		{
			GenerationContext.AddParticipatingObject(*TypedNodeProject->ReferenceTexture);

			const UTextureLODSettings& TextureLODSettings = GenerationContext.Options.TargetPlatform->GetTextureLODSettings();
			const int32 FirstLODAvailable = GenerationContext.Options.bUseLODAsBias ? GenerationContext.FirstLODAvailable : 0;
			MaxReferenceTextureSizeInGame = GetTextureSizeInGame(*TypedNodeProject->ReferenceTexture, TextureLODSettings, FirstLODAvailable);
		}

		FUintVector2 TextureSize(TypedNodeProject->TextureSizeX, TypedNodeProject->TextureSizeY);

		// Max TextureSize allowed
		const int32 MaxProjectedTextureSizeInGame = ReferenceTextureSize > 0 && ReferenceTextureSize < MaxReferenceTextureSizeInGame ? ReferenceTextureSize : MaxReferenceTextureSizeInGame;

		const int32 ProjectorNodeTextureSize = FMath::Max(TextureSize.X, TextureSize.Y);
		if (ProjectorNodeTextureSize > 0 && ProjectorNodeTextureSize > MaxProjectedTextureSizeInGame)
		{
			const int32 NumMips = FMath::CeilLogTwo(ProjectorNodeTextureSize) + 1;
			const int32 MaxNumMips = FMath::CeilLogTwo(MaxProjectedTextureSizeInGame) + 1;

			TextureSize.X = TextureSize.X >> (NumMips - MaxNumMips);
			TextureSize.Y = TextureSize.Y >> (NumMips - MaxNumMips);
		}

		ImageNode->SetImageSize(TextureSize);

		ImageNode->SetEnableSeamCorrection(TypedNodeProject->bEnableTextureSeamCorrection);
		ImageNode->SetAngleFadeChannels(TypedNodeProject->bEnableAngleFadeOutForRGB, TypedNodeProject->bEnableAngleFadeOutForAlpha);
		ImageNode->SetSamplingMethod(Invoke(
			[](ETextureProjectSamplingMethod Method) -> mu::ESamplingMethod 
			{
				switch(Method)
				{
				case ETextureProjectSamplingMethod::Point:    return mu::ESamplingMethod::Point;
				case ETextureProjectSamplingMethod::BiLinear: return mu::ESamplingMethod::BiLinear;
				default: { check(false); return mu::ESamplingMethod::Point; }
				}
			}, 
			TypedNodeProject->SamplingMethod)
		);

		ImageNode->SetMinFilterMethod(Invoke(
			[](ETextureProjectMinFilterMethod Method) -> mu::EMinFilterMethod 
			{
				switch(Method)
				{
				case ETextureProjectMinFilterMethod::None:               return mu::EMinFilterMethod::None;
				case ETextureProjectMinFilterMethod::TotalAreaHeuristic: return mu::EMinFilterMethod::TotalAreaHeuristic;
				default: { check(false); return mu::EMinFilterMethod::None; }
				}
			}, 
			TypedNodeProject->MinFilterMethod)
		);

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeProject->AngleFadeStartPin()))
		{
			mu::NodeScalarPtr ScalarNode = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);
			ImageNode->SetAngleFadeStart(ScalarNode);
		}
		else
		{
			mu::NodeScalarConstantPtr ScalarNode = new mu::NodeScalarConstant;
			float value = FCString::Atof(*TypedNodeProject->AngleFadeStartPin()->DefaultValue);
			ScalarNode->SetValue(value);
			ImageNode->SetAngleFadeStart(ScalarNode);
		}

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeProject->AngleFadeEndPin()))
		{
			mu::NodeScalarPtr ScalarNode = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);
			ImageNode->SetAngleFadeEnd(ScalarNode);
		}
		else
		{
			mu::NodeScalarConstantPtr ScalarNode = new mu::NodeScalarConstant;
			float value = FCString::Atof(*TypedNodeProject->AngleFadeEndPin()->DefaultValue);
			ScalarNode->SetValue(value);
			ImageNode->SetAngleFadeEnd(ScalarNode);
		}

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeProject->MeshPin()))
		{
			FMutableGraphMeshGenerationData DummyMeshData;
			mu::NodeMeshPtr MeshNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, DummyMeshData, false, false);
			ImageNode->SetMesh(MeshNode);
		}

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeProject->MeshMaskPin()))
		{
			mu::NodeImagePtr MeshMaskNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
			ImageNode->SetTargetMask(MeshMaskNode);
		}

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeProject->ProjectorPin()))
		{
			mu::NodeProjectorPtr ProjectorNode = GenerateMutableSourceProjector(ConnectedPin, GenerationContext);
			ImageNode->SetProjector(ProjectorNode);
		}

		int TexIndex = -1;// TypedNodeProject->OutputPins.Find((UEdGraphPin*)Pin);

		for (int32 i = 0; i < TypedNodeProject->GetNumOutputs(); ++i)
		{
			if (TypedNodeProject->OutputPins(i) == Pin)
			{
				TexIndex = i;
			}
		}

		check(TexIndex >= 0 && TexIndex < TypedNodeProject->GetNumTextures());

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeProject->TexturePins(TexIndex)))
		{
			mu::NodeImagePtr SourceNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, FMath::Max(TextureSize.X, TextureSize.Y));
			ImageNode->SetImage(SourceNode);
		}
	}

	else if (const UCustomizableObjectNodeTextureBinarise* TypedNodeTexBin = Cast<UCustomizableObjectNodeTextureBinarise>(Node))
	{
		mu::NodeImageBinarisePtr BinariseNode = new mu::NodeImageBinarise();
		Result = BinariseNode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeTexBin->GetBaseImagePin()))
		{
			mu::NodeImagePtr BaseImageNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
			BinariseNode->SetBase(BaseImageNode);
		}

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeTexBin->GetThresholdPin()))
		{
			mu::NodeScalarPtr ThresholdNode = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);
			BinariseNode->SetThreshold(ThresholdNode);
		}
	}

	else if (const UCustomizableObjectNodeTextureInvert* TypedNodeTexInv = Cast<UCustomizableObjectNodeTextureInvert>(Node))
	{
		mu::NodeImageInvertPtr InvertNode = new mu::NodeImageInvert();
		Result = InvertNode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeTexInv->GetBaseImagePin()))
		{
			mu::NodeImagePtr BaseImageNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
			InvertNode->SetBase(BaseImageNode);
		}
	}

	else if (const UCustomizableObjectNodeTextureColourMap* TypedNodeColourMap = Cast<UCustomizableObjectNodeTextureColourMap>(Node))
	{
		mu::NodeImageColourMapPtr ColourMapNode = new mu::NodeImageColourMap();

		Result = ColourMapNode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeColourMap->GetMapPin()))
		{
			mu::NodeImagePtr GradientImageNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
			ColourMapNode->SetMap( GradientImageNode ); 
		}

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeColourMap->GetMaskPin()))
		{
			mu::NodeImagePtr GradientImageNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
			ColourMapNode->SetMask( GradientImageNode ); 
		}

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeColourMap->GetBasePin()))
		{
			mu::NodeImagePtr SourceImageNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
			ColourMapNode->SetBase( SourceImageNode );
		}
	}

	else if (const UCustomizableObjectNodeTextureTransform* TypedNodeTransform = Cast<UCustomizableObjectNodeTextureTransform>(Node))
	{
		mu::NodeImageTransformPtr TransformNode = new mu::NodeImageTransform();
		Result = TransformNode;
		
		if ( UEdGraphPin* BaseImagePin = FollowInputPin(*TypedNodeTransform->GetBaseImagePin()) )
		{
			mu::NodeImagePtr ImageNode = GenerateMutableSourceImage( BaseImagePin, GenerationContext, ReferenceTextureSize);
			TransformNode->SetBase( ImageNode ); 
		}

		if ( UEdGraphPin* OffsetXPin = FollowInputPin(*TypedNodeTransform->GetOffsetXPin()) )
		{
			mu::NodeScalarPtr OffsetXNode = GenerateMutableSourceFloat(OffsetXPin, GenerationContext);
			TransformNode->SetOffsetX( OffsetXNode ); 
		}

		if ( UEdGraphPin* OffsetYPin = FollowInputPin(*TypedNodeTransform->GetOffsetYPin()) )
		{
			mu::NodeScalarPtr OffsetYNode = GenerateMutableSourceFloat(OffsetYPin, GenerationContext);
			TransformNode->SetOffsetY( OffsetYNode ); 
		}

		if ( UEdGraphPin* ScaleXPin = FollowInputPin(*TypedNodeTransform->GetScaleXPin()) )
		{
			mu::NodeScalarPtr ScaleXNode = GenerateMutableSourceFloat(ScaleXPin, GenerationContext);
			TransformNode->SetScaleX( ScaleXNode ); 
		}

		if ( UEdGraphPin* ScaleYPin = FollowInputPin(*TypedNodeTransform->GetScaleYPin()) )
		{
			mu::NodeScalarPtr ScaleYNode = GenerateMutableSourceFloat(ScaleYPin, GenerationContext);
			TransformNode->SetScaleY( ScaleYNode ); 
		}

		if ( UEdGraphPin* RotationPin = FollowInputPin(*TypedNodeTransform->GetRotationPin()) )
		{
			mu::NodeScalarPtr RotationNode = GenerateMutableSourceFloat(RotationPin, GenerationContext);
			TransformNode->SetRotation( RotationNode ); 
		}

		TransformNode->SetAddressMode(Invoke([&]() 
		{
			switch (TypedNodeTransform->AddressMode)
			{
			case ETextureTransformAddressMode::Wrap:		 return mu::EAddressMode::Wrap;
			case ETextureTransformAddressMode::ClampToEdge:  return mu::EAddressMode::ClampToEdge;
			case ETextureTransformAddressMode::ClampToBlack: return mu::EAddressMode::ClampToBlack;
			default: { check(false); return mu::EAddressMode::None; }
			}
		}));

		FUintVector2 TextureSize(TypedNodeTransform->TextureSizeX, TypedNodeTransform->TextureSizeY);

		// Calculate the max TextureSize allowed using the ReferenceTextureSize and the Reference texture from the node
		int32 MaxReferenceTextureSizeInGame = ReferenceTextureSize;
		if (TypedNodeTransform->ReferenceTexture)
		{
			GenerationContext.AddParticipatingObject(*TypedNodeTransform->ReferenceTexture);

			const UTextureLODSettings& TextureLODSettings = GenerationContext.Options.TargetPlatform->GetTextureLODSettings();
			const uint8 SurfaceLODBias = GenerationContext.Options.bUseLODAsBias ? GenerationContext.FirstLODAvailable : 0;
			MaxReferenceTextureSizeInGame = GetTextureSizeInGame(*TypedNodeTransform->ReferenceTexture, TextureLODSettings, SurfaceLODBias);
		}

		// Max TextureSize allowed
		const int32 MaxTransformTextureSizeInGame = ReferenceTextureSize > 0 && ReferenceTextureSize < MaxReferenceTextureSizeInGame ? ReferenceTextureSize : MaxReferenceTextureSizeInGame;

		const int32 TransformNodeTextureSize = FMath::Max(TextureSize.X, TextureSize.Y);
		if (TransformNodeTextureSize > 0 && TransformNodeTextureSize > MaxTransformTextureSizeInGame)
		{
			const int32 NumMips = FMath::CeilLogTwo(TransformNodeTextureSize) + 1;
			const int32 MaxNumMips = FMath::CeilLogTwo(MaxTransformTextureSizeInGame) + 1;

			TextureSize.X = TextureSize.X >> (NumMips - MaxNumMips);
			TextureSize.Y = TextureSize.Y >> (NumMips - MaxNumMips);
		}

		TransformNode->SetKeepAspectRatio(TypedNodeTransform->bKeepAspectRatio);
		TransformNode->SetSizeX(TextureSize.X);
		TransformNode->SetSizeY(TextureSize.Y);
	}

	else if (const UCustomizableObjectNodeTextureSaturate* TypedNodeSaturate = Cast<UCustomizableObjectNodeTextureSaturate>(Node))
	{
		mu::Ptr<mu::NodeImageSaturate> SaturateNode = new mu::NodeImageSaturate();
		Result = SaturateNode;
	
		if ( UEdGraphPin* BaseImagePin = FollowInputPin(*TypedNodeSaturate->GetBaseImagePin()) )
		{
			mu::NodeImagePtr ImageNode = GenerateMutableSourceImage(BaseImagePin, GenerationContext, ReferenceTextureSize);
			SaturateNode->SetSource(ImageNode); 
		}

		if ( UEdGraphPin* FactorPin = FollowInputPin(*TypedNodeSaturate->GetFactorPin()))
		{
			mu::NodeScalarPtr FactorNode = GenerateMutableSourceFloat(FactorPin, GenerationContext);
			SaturateNode->SetFactor(FactorNode); 
		}
	}

	else if (const UCustomizableObjectNodePassThroughTexture* TypedNodePassThroughTex = Cast<UCustomizableObjectNodePassThroughTexture>(Node))
	{
		UTexture* BaseTexture = TypedNodePassThroughTex->PassThroughTexture;
		if (BaseTexture)
		{
			mu::NodeImageConstantPtr ImageNode = new mu::NodeImageConstant();
			Result = ImageNode;

			ImageNode->SetValue(GenerateImageConstant(BaseTexture, GenerationContext, true).get());
		}
		else
		{
			GenerationContext.Compiler->CompilerLog(LOCTEXT("MissingImagePassThrough", "Missing image in pass-through texture node."), Node);
		}
	}

	else if (const UCustomizableObjectNodePassThroughTextureSwitch* TypedNodePassThroughTextureSwitch = Cast<UCustomizableObjectNodePassThroughTextureSwitch>(Node))
	{
		Result = [&]()
		{
			const UEdGraphPin* SwitchParameter = TypedNodePassThroughTextureSwitch->SwitchParameter();

			// Check Switch Parameter arity preconditions.
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*SwitchParameter))
			{
				mu::NodeScalarPtr SwitchParam = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);
				// Switch Param not generated
				if (!SwitchParam)
				{
					const FText Message = LOCTEXT("FailedToGenerateSwitchParam", "Could not generate switch enum parameter. Please refesh the switch node and connect an enum.");
					GenerationContext.Compiler->CompilerLog(Message, Node);

					return Result;
				}

				if (SwitchParam->GetType() != mu::NodeScalarEnumParameter::GetStaticType())
				{
					const FText Message = LOCTEXT("WrongSwitchParamType", "Switch parameter of incorrect type.");
					GenerationContext.Compiler->CompilerLog(Message, Node);

					return Result;
				}

				const int32 NumSwitchOptions = TypedNodePassThroughTextureSwitch->GetNumElements();

				mu::NodeScalarEnumParameter* EnumParameter = static_cast<mu::NodeScalarEnumParameter*>(SwitchParam.get());
				if (NumSwitchOptions != EnumParameter->GetValueCount())
				{
					const FText Message = LOCTEXT("MismatchedSwitch", "Switch enum and switch node have different number of options. Please refresh the switch node to make sure the outcomes are labeled properly.");
					GenerationContext.Compiler->CompilerLog(Message, Node);
				}

				// TODO: Implement Mutable core pass-through switch nodes
				mu::Ptr<mu::NodeImageSwitch> SwitchNode = new mu::NodeImageSwitch;
				SwitchNode->SetParameter(SwitchParam);
				SwitchNode->SetOptionCount(NumSwitchOptions);

				for (int32 SelectorIndex = 0; SelectorIndex < NumSwitchOptions; ++SelectorIndex)
				{
					if (const UEdGraphPin* TexturePin = FollowInputPin(*TypedNodePassThroughTextureSwitch->GetElementPin(SelectorIndex)))
					{
						mu::Ptr<mu::NodeImage> PassThroughImage = GenerateMutableSourceImage(TexturePin, GenerationContext, ReferenceTextureSize);
						SwitchNode->SetOption(SelectorIndex, PassThroughImage);
					}
					else
					{
						const FText Message = LOCTEXT("MissingPassThroughTexture", "Unable to generate pass-through texture switch node. Required connection not found.");
						GenerationContext.Compiler->CompilerLog(Message, Node);
						return Result;
					}
				}

				Result = SwitchNode;
				return Result;
			}
			else
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("NoEnumParamInSwitch", "Switch nodes must have an enum switch parameter. Please connect an enum and refesh the switch node."), Node);
				return Result;
			}
		}(); // invoke lambda.
	}

	// If the node is a plain colour node, generate an image out of it
	else if (Pin->PinType.PinCategory == Schema->PC_Color)
	{
		mu::NodeColourPtr ColorNode = GenerateMutableSourceColor(Pin, GenerationContext);

		mu::NodeImagePlainColourPtr ImageNode = new mu::NodeImagePlainColour;
		ImageNode->SetSize(16, 16);
		ImageNode->SetColour(ColorNode);
		Result = ImageNode;
	}

	else if (const UCustomizableObjectNodeTable* TypedNodeTable = Cast<UCustomizableObjectNodeTable>(Node))
	{
		if (Pin->PinType.PinCategory == Schema->PC_Color)
		{
			mu::NodeColourPtr ColorNode = GenerateMutableSourceColor(Pin, GenerationContext);
			mu::NodeImagePlainColourPtr ImageNode = new mu::NodeImagePlainColour;

			ImageNode->SetSize(16, 16);
			ImageNode->SetColour(ColorNode);

			Result = ImageNode;
		}
		else
		{
			//This node will add a checker texture in case of error
			mu::NodeImageConstantPtr EmptyNode = new mu::NodeImageConstant();
			Result = EmptyNode;

			bool bSuccess = true;

			if (Pin->PinType.PinCategory == Schema->PC_MaterialAsset)
			{
				// Material pins have to skip the cache of nodes or they will return always the same column node
				bDoNotAddToGeneratedCache = true;
			}

			UDataTable* DataTable = GetDataTable(TypedNodeTable, GenerationContext);

			if (DataTable)
			{
				FString ColumnName = Pin->PinFriendlyName.ToString();
				FProperty* Property = DataTable->FindTableProperty(FName(*ColumnName));

				if (!Property)
				{
					FString Msg = FString::Printf(TEXT("Couldn't find the column [%s] in the data table's struct."), *ColumnName);
					GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), Node);

					bSuccess = false;
				}

				UTexture* DefaultTexture = TypedNodeTable->GetColumnDefaultAssetByType<UTexture>(Pin);

				if (bSuccess && Pin->PinType.PinCategory != Schema->PC_MaterialAsset && !DefaultTexture)
				{
					FString Msg = FString::Printf(TEXT("Couldn't find a default value in the data table's struct for the column [%s]. The default value is null or not a supported Texture"), *ColumnName);
					GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), Node);
					
					bSuccess = false;
				}

				if (bSuccess)
				{
					// Generating a new data table if not exists
					mu::TablePtr Table = nullptr;
					Table = GenerateMutableSourceTable(DataTable, TypedNodeTable, GenerationContext);

					if (Table)
					{
						mu::NodeImageTablePtr ImageTableNode = new mu::NodeImageTable();

						if (Pin->PinType.PinCategory == Schema->PC_MaterialAsset)
						{
							// Material parameters use the Data Table Column Name + Parameter id as mutable column Name to aboid duplicated names (i.e. two MI columns with the same parents but different values).
							ColumnName = Property->GetDisplayNameText().ToString() + GenerationContext.CurrentMaterialTableParameterId;
						}
						else
						{
							// Checking if this pin texture has been used in another table node but with a different texture mode
							const int32 ImageDataIndex = GenerationContext.GeneratedTableImages.Find({ ColumnName, Pin->PinType.PinCategory, Table });

							if (ImageDataIndex != INDEX_NONE )
							{
								if (GenerationContext.GeneratedTableImages[ImageDataIndex].PinType != Pin->PinType.PinCategory)
								{
									TArray<const UObject*> Nodes;
									Nodes.Add(TypedNodeTable);
									Nodes.Add(GenerationContext.GeneratedTableImages[ImageDataIndex].TableNode);

									FString Msg = FString::Printf(TEXT("Texture pin [%s] with different texture modes found in more than one table node. This will add multiple times the texture reseource in the final cook."), *ColumnName);
									GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), Nodes);
								}
							}
							else
							{
								GenerationContext.GeneratedTableImages.Add({ ColumnName, Pin->PinType.PinCategory, Table, TypedNodeTable });
							}

							// Encoding the texture mode of the type to allow different texture modes from the same pin
							if (Pin->PinType.PinCategory == Schema->PC_PassThroughImage)
							{
								ColumnName += "--PassThrough";
							}
						}

						// Generating a new Texture column if not exists
						if (Table->FindColumn(ColumnName) == INDEX_NONE)
						{
							int32 Dummy = -1; // TODO MTBL-1512
							bool Dummy2 = false;
							bSuccess = GenerateTableColumn(TypedNodeTable, Pin, Table, ColumnName, Property, Dummy, Dummy, GenerationContext.CurrentLOD, Dummy, Dummy2, GenerationContext);

							if (!bSuccess)
							{
								FString Msg = FString::Printf(TEXT("Failed to generate the mutable table column [%s]"), *ColumnName);
								GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), Node);
							}
						}

						if (bSuccess)
						{
							Result = ImageTableNode;

							ImageTableNode->SetTable(Table);
							ImageTableNode->SetColumn(ColumnName);
							ImageTableNode->SetParameterName(TypedNodeTable->ParameterName);
							ImageTableNode->SetNoneOption(TypedNodeTable->bAddNoneOption);
							ImageTableNode->SetDefaultRowName(TypedNodeTable->DefaultRowName.ToString());

							if (UTexture2D* DefaultTexture2D = Cast<UTexture2D>(DefaultTexture))
							{
								mu::FImageDesc ImageDesc = GenerateImageDescriptor(DefaultTexture2D);

								uint32 LODBias = ReferenceTextureSize > 0 ? ComputeLODBiasForTexture(GenerationContext, *DefaultTexture2D, nullptr, ReferenceTextureSize) : 0;
								ImageDesc.m_size[0] = ImageDesc.m_size[0] >> LODBias;
								ImageDesc.m_size[1] = ImageDesc.m_size[1] >> LODBias;
								
								const uint16 MaxTextureSize = FMath::Max3(ImageDesc.m_size[0], ImageDesc.m_size[1], (uint16)1);
								ImageTableNode->SetMaxTextureSize(MaxTextureSize);
								ImageTableNode->SetReferenceImageDescriptor(ImageDesc);
							}
						}
					}
					else
					{
						FString Msg = FString::Printf(TEXT("Couldn't generate a mutable table."));
						GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), Node);
					}
				}
			}
			else
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("ImageTableError", "Couldn't find the data table of the node."), Node);
			}
		}
	}

	else
	{
		GenerationContext.Compiler->CompilerLog(LOCTEXT("UnimplementedNode", "Node type not implemented yet."), Node);
	}

	if (!bDoNotAddToGeneratedCache)
	{
		GenerationContext.Generated.Add(Key, FGeneratedData(Node, Result));
		GenerationContext.GeneratedNodes.Add(Node);
	}

	if (Result)
	{
		Result->SetMessageContext(Node);
	}

	return Result;
}


#undef LOCTEXT_NAMESPACE

