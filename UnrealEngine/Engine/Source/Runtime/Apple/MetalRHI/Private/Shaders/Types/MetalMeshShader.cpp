// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalMeshShader.cpp: Metal RHI Mesh Shader Class Implementation.
=============================================================================*/


#include "MetalRHIPrivate.h"
#include "Templates/MetalBaseShader.h"
#include "MetalMeshShader.h"


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Mesh Shader Class

#if PLATFORM_SUPPORTS_MESH_SHADERS
FMetalMeshShader::FMetalMeshShader(TArrayView<const uint8> InCode)
{
	FMetalCodeHeader Header;
	Init(InCode, Header, MTLLibraryPtr());
}

FMetalMeshShader::FMetalMeshShader(TArrayView<const uint8> InCode, MTLLibraryPtr InLibrary)
{
	FMetalCodeHeader Header;
	Init(InCode, Header, InLibrary);
}

MTLFunctionPtr FMetalMeshShader::GetFunction()
{
	return GetCompiledFunction();
}
#endif
