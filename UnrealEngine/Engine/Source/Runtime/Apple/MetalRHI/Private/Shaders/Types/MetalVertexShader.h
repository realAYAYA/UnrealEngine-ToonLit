// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalVertexShader.h: Metal RHI Vertex Shader Class Definition.
=============================================================================*/

#pragma once


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Vertex Shader Class


class FMetalVertexShader : public TMetalBaseShader<FRHIVertexShader, SF_Vertex>
{
public:
	FMetalVertexShader(TArrayView<const uint8> InCode);
	FMetalVertexShader(TArrayView<const uint8> InCode, MTLLibraryPtr InLibrary);

	MTLFunctionPtr GetFunction();
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
    MTLFunctionPtr GetObjectFunctionForGeometryEmulation();
#endif
};
