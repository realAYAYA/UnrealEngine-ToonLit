// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FSlateShaderResourceProxy;
class FSlateTexture2DRHIRef;

/** Represents a dynamic texture resource for rendering in Slate*/
class FSlateDynamicTextureResource
{
public:
	FSlateDynamicTextureResource(FSlateTexture2DRHIRef* ExistingTexture);
	~FSlateDynamicTextureResource();

	FSlateShaderResourceProxy* Proxy;
	FSlateTexture2DRHIRef* RHIRefTexture;
};
