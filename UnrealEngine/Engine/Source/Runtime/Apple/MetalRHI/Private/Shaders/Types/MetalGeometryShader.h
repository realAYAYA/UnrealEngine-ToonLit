// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalGeometryShader.h: Metal RHI Geometry Shader Class Definition.
=============================================================================*/

#pragma once


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Geometry Shader Class


class FMetalGeometryShader : public TMetalBaseShader<FRHIGeometryShader, SF_Geometry>
{
#if METAL_USE_METAL_SHADER_CONVERTER
public:
    FMetalGeometryShader(TArrayView<const uint8> InCode);
    FMetalGeometryShader(TArrayView<const uint8> InCode, MTLLibraryPtr InLibrary);

    MTLFunctionPtr GetFunction();
#endif
};
