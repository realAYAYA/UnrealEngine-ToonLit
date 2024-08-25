// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalGeometryShader.cpp: Metal RHI Geometry Shader Class Implementation.
=============================================================================*/


#include "MetalRHIPrivate.h"
#include "Templates/MetalBaseShader.h"
#include "MetalGeometryShader.h"


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Geometry Shader Class

#if METAL_USE_METAL_SHADER_CONVERTER
FMetalGeometryShader::FMetalGeometryShader(TArrayView<const uint8> InCode)
{
	FMetalCodeHeader Header;
	Init(InCode, Header, MTLLibraryPtr());
}

FMetalGeometryShader::FMetalGeometryShader(TArrayView<const uint8> InCode, MTLLibraryPtr InLibrary)
{
	FMetalCodeHeader Header;
	Init(InCode, Header, InLibrary);
}

MTLFunctionPtr FMetalGeometryShader::GetFunction()
{
	return GetCompiledFunction();
}
#endif
