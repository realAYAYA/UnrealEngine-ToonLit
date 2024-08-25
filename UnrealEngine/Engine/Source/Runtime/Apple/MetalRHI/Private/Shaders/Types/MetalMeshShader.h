// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalMeshShader.h: Metal RHI Mesh Shader Class Definition.
=============================================================================*/

#pragma once


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Mesh Shader Class


#if PLATFORM_SUPPORTS_MESH_SHADERS
class FMetalMeshShader : public TMetalBaseShader<FRHIMeshShader, SF_Mesh>
{
public:
    FMetalMeshShader(TArrayView<const uint8> InCode);
    FMetalMeshShader(TArrayView<const uint8> InCode, MTLLibraryPtr InLibrary);

    MTLFunctionPtr GetFunction();
};
#endif
