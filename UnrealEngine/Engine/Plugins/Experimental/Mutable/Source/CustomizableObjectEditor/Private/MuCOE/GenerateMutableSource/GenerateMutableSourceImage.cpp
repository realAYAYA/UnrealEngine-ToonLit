// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSourceImage.h"

#include "Containers/Array.h"
#include "Containers/EnumAsByte.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/StringConv.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Logging/TokenizedMessage.h"
#include "Math/IntPoint.h"
#include "Math/IntVector.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CString.h"
#include "Misc/Guid.h"
#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"
#include "MuCO/CustomizableObjectUIData.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSource.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceColor.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceFloat.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceMesh.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceProjector.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceTable.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTexture.h"
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
#include "MuCOE/Nodes/CustomizableObjectNodeTextureToChannels.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureTransform.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureVariation.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "MuCOE/UnrealToMutableTextureConversionUtils.h"
#include "MuR/Image.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeImageBinarise.h"
#include "MuT/NodeImageColourMap.h"
#include "MuT/NodeImageConstant.h"
#include "MuT/NodeImageFormat.h"
#include "MuT/NodeImageInterpolate.h"
#include "MuT/NodeImageInvert.h"
#include "MuT/NodeImageLayer.h"
#include "MuT/NodeImageLayerColour.h"
#include "MuT/NodeImageParameter.h"
#include "MuT/NodeImagePlainColour.h"
#include "MuT/NodeImageProject.h"
#include "MuT/NodeImageResize.h"
#include "MuT/NodeImageSwitch.h"
#include "MuT/NodeImageSwizzle.h"
#include "MuT/NodeImageTable.h"
#include "MuT/NodeImageTransform.h"
#include "MuT/NodeImageVariation.h"
#include "MuT/NodeProjector.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeScalarConstant.h"
#include "MuT/NodeScalarEnumParameter.h"
#include "Templates/Casts.h"
#include "Templates/Tuple.h"
#include "TextureResource.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


mu::ImagePtr ConvertTextureUnrealToMutable(UTexture2D* Texture, const UCustomizableObjectNode* Node, FCustomizableObjectCompiler* Compiler, bool bIsNormalComposite)
{     
	mu::ImagePtr MutableImage;
	EUnrealToMutableConversionError Error = EUnrealToMutableConversionError::Unknown;

	Tie(MutableImage, Error) = ConvertTextureUnrealToMutable(Texture, bIsNormalComposite);

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
	case EUnrealToMutableConversionError::CompositeImageDimensionMissmatch:
		{
			Compiler->CompilerLog(
					LOCTEXT("CompositeImageDimensionMissmatch", 
							"Composite image dimension missmatch."), 
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


mu::NodeImagePtr ResizeToMaxTextureSize(float MaxTextureSize, const UTexture2D* BaseTexture, mu::NodeImageConstantPtr ImageNode)
{
	// To scale when above MaxTextureSize if defined
	if (MaxTextureSize > 0 && BaseTexture
		&& (BaseTexture->GetImportedSize().X > MaxTextureSize
			|| BaseTexture->GetImportedSize().Y > MaxTextureSize))
	{
		mu::NodeImageResizePtr ResizeImage = new mu::NodeImageResize();
		ResizeImage->SetBase(ImageNode.get());
		ResizeImage->SetRelative(false);
		float Factor = FMath::Min(
			MaxTextureSize / (float)(BaseTexture->GetImportedSize().X),
			MaxTextureSize / (float)(BaseTexture->GetImportedSize().Y));
		ResizeImage->SetSize(BaseTexture->GetImportedSize().X * Factor, BaseTexture->GetImportedSize().Y * Factor);
		return ResizeImage;
	}
	// TODO: MaxTextureSize = 0 indicates platform-specific maximum size, we should scale for it

	return ImageNode;
}


mu::NodeImagePtr GenerateMutableSourceImage(const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext, float MaxTextureSize)
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

	mu::NodeImagePtr Result;
	
	if (const UCustomizableObjectNodeTexture* TypedNodeTex = Cast<UCustomizableObjectNodeTexture>(Node))
	{
		UTexture2D* BaseTexture = TypedNodeTex->Texture;
		if (BaseTexture)
		{
			// Check the specific image cache
			FGeneratedImageKey imageKey = FGeneratedImageKey(Pin);
			mu::NodeImageConstantPtr ImageNode;
			mu::NodeImageConstantPtr* Cached = GenerationContext.GeneratedImages.Find(imageKey);
			if (Cached)
			{
				ImageNode = *Cached;
			}
			else
			{
				ImageNode = new mu::NodeImageConstant();
				GenerationContext.ArrayTextureUnrealToMutableTask.Add(FTextureUnrealToMutableTask(ImageNode, BaseTexture, Node));
				GenerationContext.GeneratedImages.Add(imageKey, ImageNode);
			}

			Result = ResizeToMaxTextureSize(MaxTextureSize, BaseTexture, ImageNode);

		}
		else
		{
			GenerationContext.Compiler->CompilerLog(LOCTEXT("MissingImage", "Missing image in texture node."), Node);
		}
	}

	else if (const UCustomizableObjectNodeTextureParameter* TypedNodeParam = Cast<UCustomizableObjectNodeTextureParameter>(Node))
	{
		mu::Ptr<mu::NodeImageParameter> TextureNode = new mu::NodeImageParameter();

		GenerationContext.AddParameterNameUnique(Node, TypedNodeParam->ParameterName);

		TextureNode->SetName(TCHAR_TO_ANSI(*TypedNodeParam->ParameterName));
		TextureNode->SetUid(TCHAR_TO_ANSI(*GenerationContext.GetNodeIdUnique(Node).ToString()));

		// TODO: Set a default value for the texture parameter?
		//ColorNode->SetDefaultValue(TypedNodeParam->DefaultValue.R, TypedNodeParam->DefaultValue.G, TypedNodeParam->DefaultValue.B);

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

		if (TypedNodeParam->DefaultValue)
		{
			ResizeNode->SetSize(TypedNodeParam->DefaultValue->GetResource()->GetSizeX(), TypedNodeParam->DefaultValue->GetResource()->GetSizeY() );
		}
		else
		{
			// \TODO: Let the user specify this in the node?
			ResizeNode->SetSize(1024,1024);
		}

		Result = ResizeNode;
	}

	else if (const UCustomizableObjectNodeMesh* TypedNodeMesh = Cast<UCustomizableObjectNodeMesh>(Node))
	{
		mu::NodeImageConstantPtr ImageNode = new mu::NodeImageConstant();
		Result = ImageNode;

		UTexture2D* Texture = TypedNodeMesh->FindTextureForPin(Pin);

		if (Texture)
		{
			GenerationContext.ArrayTextureUnrealToMutableTask.Add(FTextureUnrealToMutableTask(ImageNode, Texture, Node));
		}

		Result = ResizeToMaxTextureSize(MaxTextureSize, Texture, ImageNode);
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
				mu::NodeImagePtr TargetNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, MaxTextureSize);

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
			Result = GenerateMutableSourceImage(ConnectedPin, GenerationContext, MaxTextureSize);
		}

		for (int LayerIndex = 0; LayerIndex < TypedNodeLayer->GetNumLayers(); ++LayerIndex)
		{
			if (const UEdGraphPin* OtherPin = FollowInputPin(*TypedNodeLayer->LayerPin(LayerIndex)))
			{
				mu::NodeImagePtr MaskNode = nullptr;
				if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeLayer->MaskPin(LayerIndex)))
				{
					MaskNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, MaxTextureSize);
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
				case COTLE_ALPHA_OVERLAY: Type = mu::EBlendType::BT_ALPHA_OVERLAY; break;
				case COTLE_NORMAL_COMBINE: Type = mu::EBlendType::BT_NORMAL_COMBINE; break;
				default:
					GenerationContext.Compiler->CompilerLog(LOCTEXT("UnsupportedImageEffect", "Texture layer effect not supported. Setting to 'Blend'."), Node);
					break;
				}

				if (Type == mu::EBlendType::BT_BLEND && !MaskNode)
				{
					GenerationContext.Compiler->CompilerLog(LOCTEXT("ModulateWithoutMask", "Texture layer effect uses Modulate without a mask. It will replace everything below it!"), Node);
				}
				
				if (OtherPin->PinType.PinCategory == Helper_GetPinCategory(Schema->PC_Image))
				{
					mu::NodeImagePtr BlendNode = GenerateMutableSourceImage(OtherPin, GenerationContext, MaxTextureSize);

					mu::NodeImageLayerPtr LayerNode = new mu::NodeImageLayer;
					LayerNode->SetBlendType(Type);
					LayerNode->SetBase(Result);
					LayerNode->SetBlended(BlendNode);
					LayerNode->SetMask(MaskNode);
					Result = LayerNode;
				}

				else if (OtherPin->PinType.PinCategory == Helper_GetPinCategory(Schema->PC_Color))
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
						SwitchNode->SetOption(SelectorIndex, GenerateMutableSourceImage(TexturePin, GenerationContext, MaxTextureSize));
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
		mu::NodeImageVariationPtr TextureNode = new mu::NodeImageVariation();
		Result = TextureNode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeImageVar->DefaultPin()))
		{
			mu::NodeImagePtr ChildNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, MaxTextureSize);
			if (ChildNode)
			{
				TextureNode->SetDefaultImage(ChildNode.get());
			}
			else
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("TextureFailed", "Texture generation failed."), Node);
			}
		}
		else
		{
			GenerationContext.Compiler->CompilerLog(LOCTEXT("TextureVarMissingDef", "Texture variation node requires a default value."), Node);
		}

		TextureNode->SetVariationCount(TypedNodeImageVar->Variations.Num());
		for (int VariationIndex = 0; VariationIndex < TypedNodeImageVar->Variations.Num(); ++VariationIndex)
		{
			const UEdGraphPin* VariationPin = TypedNodeImageVar->VariationPin(VariationIndex);
			if (!VariationPin) continue;

			TextureNode->SetVariationTag(VariationIndex, TCHAR_TO_ANSI(*TypedNodeImageVar->Variations[VariationIndex].Tag));
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*VariationPin))
			{
				mu::NodeImagePtr ChildNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, MaxTextureSize);
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
	}

	else if (const UCustomizableObjectNodeTextureFromChannels* TypedNodeFrom = Cast<UCustomizableObjectNodeTextureFromChannels>(Node))
	{
		mu::NodeImagePtr RNode, GNode, BNode, ANode;
		bool RGB = false;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFrom->RPin()))
		{
			RNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, MaxTextureSize);
			RGB = true;
		}
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFrom->GPin()))
		{
			GNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, MaxTextureSize);
			RGB = true;
		}
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFrom->BPin()))
		{
			BNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, MaxTextureSize);
			RGB = true;
		}
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFrom->APin()))
		{
			ANode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, MaxTextureSize);
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
			BaseNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, MaxTextureSize);
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
		mu::NodeImageProjectPtr ImageNode = new mu::NodeImageProject();
		Result = ImageNode;

		if (!FollowInputPin(*TypedNodeProject->MeshPin()))
		{
			GenerationContext.Compiler->CompilerLog(LOCTEXT("MissingMeshInProjector", "Texture projector does not have a Mesh. It will be ignored. "), Node, EMessageSeverity::Warning);
			Result = nullptr;
		}

		ImageNode->SetLayout(TypedNodeProject->Layout);
		ImageNode->SetImageSize( FUintVector2(TypedNodeProject->TextureSizeX, TypedNodeProject->TextureSizeY) );

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
			mu::NodeMeshPtr MeshNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, DummyMeshData);
			ImageNode->SetMesh(MeshNode);
		}

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeProject->MeshMaskPin()))
		{
			mu::NodeImagePtr MeshMaskNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, MaxTextureSize);
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
			mu::NodeImagePtr SourceNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, MaxTextureSize);

			// Images that are projected won't be scaled by any means, so we need to apply the lodbias settings here
			if (GenerationContext.CurrentTextureLODBias > 0)
			{
				mu::NodeImageResizePtr ResizeImage = new mu::NodeImageResize();
				ResizeImage->SetBase(SourceNode.get());
				ResizeImage->SetRelative(true);
				float factor = FMath::Pow(0.5f, GenerationContext.CurrentTextureLODBias);
				ResizeImage->SetSize(factor, factor);
				ResizeImage->SetMessageContext(Node);
				SourceNode = ResizeImage;
			}

			ImageNode->SetImage(SourceNode);
		}
	}

	else if (const UCustomizableObjectNodeTextureBinarise* TypedNodeTexBin = Cast<UCustomizableObjectNodeTextureBinarise>(Node))
	{
		mu::NodeImageBinarisePtr BinariseNode = new mu::NodeImageBinarise();
		Result = BinariseNode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeTexBin->GetBaseImagePin()))
		{
			mu::NodeImagePtr BaseImageNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, MaxTextureSize);
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
			mu::NodeImagePtr BaseImageNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, MaxTextureSize);
			InvertNode->SetBase(BaseImageNode);
		}
	}

	else if (const UCustomizableObjectNodeTextureColourMap* TypedNodeColourMap = Cast<UCustomizableObjectNodeTextureColourMap>(Node))
	{
		mu::NodeImageColourMapPtr ColourMapNode = new mu::NodeImageColourMap();

		Result = ColourMapNode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeColourMap->GetMapPin()))
		{
			mu::NodeImagePtr GradientImageNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, MaxTextureSize);
			ColourMapNode->SetMap( GradientImageNode ); 
		}

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeColourMap->GetMaskPin()))
		{
			mu::NodeImagePtr GradientImageNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, MaxTextureSize);
			ColourMapNode->SetMask( GradientImageNode ); 
		}

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeColourMap->GetBasePin()))
		{
			mu::NodeImagePtr SourceImageNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, MaxTextureSize);
			ColourMapNode->SetBase( SourceImageNode );
		}
	}

	else if (const UCustomizableObjectNodeTextureTransform* TypedNodeTransform = Cast<UCustomizableObjectNodeTextureTransform>(Node))
	{
		mu::NodeImageTransformPtr TransformNode = new mu::NodeImageTransform();
		Result = TransformNode;
		
		if ( UEdGraphPin* BaseImagePin = FollowInputPin(*TypedNodeTransform->GetBaseImagePin()) )
		{
			mu::NodeImagePtr ImageNode = GenerateMutableSourceImage( BaseImagePin, GenerationContext, MaxTextureSize);
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
	}
	// If the node is a plain colour node, generate an image out of it
	else if (Pin->PinType.PinCategory == Helper_GetPinCategory(Schema->PC_Color))
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
			mu::NodeImageTablePtr ImageTableNode = new mu::NodeImageTable();
			Result = ImageTableNode;

			mu::TablePtr Table = nullptr;

			if (TypedNodeTable->Table)
			{
				FString ColumnName = Pin->PinFriendlyName.ToString();

				if (Pin->PinType.PinCategory == Schema->PC_MaterialAsset)
				{
					ColumnName = GenerationContext.CurrentMaterialTableParameterId;
				}

				Table = GenerateMutableSourceTable(TypedNodeTable->Table->GetName(), Pin, GenerationContext);
				
				ImageTableNode->SetTable(Table);
				ImageTableNode->SetColumn(TCHAR_TO_ANSI(*ColumnName));
				ImageTableNode->SetParameterName(TCHAR_TO_ANSI(*TypedNodeTable->ParameterName));

				GenerationContext.AddParameterNameUnique(Node, TypedNodeTable->ParameterName);
				
				if (Table->FindColumn(TCHAR_TO_ANSI(*ColumnName)) == -1)
				{
					FString Msg = FString::Printf(TEXT("Couldn't find pin column with name %s"), *ColumnName);
					GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), Node);
				}
			}
			else
			{
				Table = new mu::Table();
				ImageTableNode->SetTable(Table);

				GenerationContext.Compiler->CompilerLog(LOCTEXT("ImageTableError", "Couldn't find the data table of the node."), Node);
			}
		}
	}

	else
	{
		GenerationContext.Compiler->CompilerLog(LOCTEXT("UnimplementedNode", "Node type not implemented yet."), Node);
	}

	GenerationContext.Generated.Add(Key, FGeneratedData(Node, Result));
	GenerationContext.GeneratedNodes.Add(Node);

	if (Result)
	{
		Result->SetMessageContext(Node);
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE

