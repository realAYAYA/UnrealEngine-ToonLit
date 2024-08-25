// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalVertexShader.cpp: Metal RHI Vertex Shader Class Implementation.
=============================================================================*/


#include "MetalRHIPrivate.h"
#include "Templates/MetalBaseShader.h"
#include "MetalVertexShader.h"


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Vertex Shader Class


FMetalVertexShader::FMetalVertexShader(TArrayView<const uint8> InCode)
{
	FMetalCodeHeader Header;
	Init(InCode, Header, MTLLibraryPtr());
}

FMetalVertexShader::FMetalVertexShader(TArrayView<const uint8> InCode, MTLLibraryPtr InLibrary)
{
	FMetalCodeHeader Header;
	Init(InCode, Header, InLibrary);
}

MTLFunctionPtr FMetalVertexShader::GetFunction()
{
	return GetCompiledFunction();
}

#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
MTLFunctionPtr FMetalVertexShader::GetObjectFunctionForGeometryEmulation()
{
    return GetCompiledFunction(false, 0);
}
#endif
