// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "MuR/Image.h"
#include "MuT/NodeImageConstant.h"
#include "MuT/NodeRange.h"

class FCustomizableObjectCompiler;
class UCustomizableObjectNode;
class UEdGraphPin;
class UTexture2D;
struct FMutableGraphGenerationContext;

mu::ImagePtr ConvertTextureUnrealToMutable(UTexture2D* Texture, const UCustomizableObjectNode* Node, FCustomizableObjectCompiler* Compiler, bool bIsNormalComposite);


mu::NodeImagePtr ResizeToMaxTextureSize(float MaxTextureSize, const UTexture2D* BaseTexture, mu::NodeImageConstantPtr ImageNode);


/** Convert a CustomizableObject Source Graph into a mutable source graph. */
mu::NodeImagePtr GenerateMutableSourceImage(const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext, float MaxTextureSize, mu::NodeRangePtr NodeRange = nullptr);
