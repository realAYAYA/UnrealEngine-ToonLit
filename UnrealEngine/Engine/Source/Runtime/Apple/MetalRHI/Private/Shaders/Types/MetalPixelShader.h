// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalPixelShader.h: Metal RHI Pixel Shader Class Definition.
=============================================================================*/

#pragma once


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Pixel Shader Class


class FMetalPixelShader : public TMetalBaseShader<FRHIPixelShader, SF_Pixel>
{
public:
	FMetalPixelShader(TArrayView<const uint8> InCode);
	FMetalPixelShader(TArrayView<const uint8> InCode, MTLLibraryPtr InLibrary);

	MTLFunctionPtr GetFunction();
};
