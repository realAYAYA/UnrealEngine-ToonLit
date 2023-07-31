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
	Init(InCode, Header);
}

FMetalVertexShader::FMetalVertexShader(TArrayView<const uint8> InCode, mtlpp::Library InLibrary)
{
	FMetalCodeHeader Header;
	Init(InCode, Header, InLibrary);
}

mtlpp::Function FMetalVertexShader::GetFunction()
{
	return GetCompiledFunction();
}
