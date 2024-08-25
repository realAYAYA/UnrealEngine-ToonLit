// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalAmplificationShader.h: Metal RHI Amplification Shader Class Definition.
=============================================================================*/

#pragma once


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Amplification Shader Class


#if PLATFORM_SUPPORTS_MESH_SHADERS
class FMetalAmplificationShader : public TMetalBaseShader<FRHIAmplificationShader, SF_Amplification>
{
public:
    FMetalAmplificationShader(TArrayView<const uint8> InCode);
    FMetalAmplificationShader(TArrayView<const uint8> InCode, MTLLibraryPtr InLibrary);

    MTLFunctionPtr GetFunction();
};
#endif
