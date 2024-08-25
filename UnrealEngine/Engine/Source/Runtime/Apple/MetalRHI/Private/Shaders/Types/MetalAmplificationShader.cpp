// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalAmplificationShader.cpp: Metal RHI Amplification Shader Class Implementation.
=============================================================================*/


#include "MetalRHIPrivate.h"
#include "Templates/MetalBaseShader.h"
#include "MetalAmplificationShader.h"


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Amplification Shader Class

#if PLATFORM_SUPPORTS_MESH_SHADERS
FMetalAmplificationShader::FMetalAmplificationShader(TArrayView<const uint8> InCode)
{
	FMetalCodeHeader Header;
	Init(InCode, Header, MTLLibraryPtr());
}

FMetalAmplificationShader::FMetalAmplificationShader(TArrayView<const uint8> InCode, MTLLibraryPtr InLibrary)
{
	FMetalCodeHeader Header;
	Init(InCode, Header, InLibrary);
}

MTLFunctionPtr FMetalAmplificationShader::GetFunction()
{
	return GetCompiledFunction();
}
#endif
