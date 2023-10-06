// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXPixelMappingRenderElement.h"


namespace UE::DMXPixelMapping::Rendering
{
	void FPixelMapRenderElement::SetParameters(FPixelMapRenderElementParameters InParameters)
	{
#if !defined(_MSC_VER) 			// Atomic for MSVC only
		const FScopeLock LockParameters(&AccessParametersMutex);
#endif
		Parameters = InParameters;
	}

	FPixelMapRenderElementParameters FPixelMapRenderElement::GetParameters() const
	{
#if !defined(_MSC_VER)  		// Atomic for MSVC only
		const FScopeLock LockParameters(&AccessParametersMutex);
#endif
		return Parameters;
	}

	void FPixelMapRenderElement::SetColor(const FLinearColor& InColor)
	{
#if !defined(_MSC_VER)  		// Atomic for MSVC only
		const FScopeLock LockColorMutex(&AccessColorMutex);
#endif
		Color = InColor;
	}

	FLinearColor FPixelMapRenderElement::GetColor() const
	{
#if !defined(_MSC_VER)  		// Atomic for MSVC only
		const FScopeLock LockColorMutex(&AccessColorMutex);
#endif
		return Color;
	}
}
