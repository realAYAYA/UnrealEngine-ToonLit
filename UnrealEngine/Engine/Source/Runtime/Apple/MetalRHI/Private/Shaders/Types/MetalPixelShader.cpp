// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalPixelShader.cpp: Metal RHI Pixel Shader Class Implementation.
=============================================================================*/


#include "MetalRHIPrivate.h"
#include "Templates/MetalBaseShader.h"
#include "MetalPixelShader.h"


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Pixel Shader Class


FMetalPixelShader::FMetalPixelShader(TArrayView<const uint8> InCode)
{
	FMetalCodeHeader Header;
	Init(InCode, Header, MTLLibraryPtr());
}

FMetalPixelShader::FMetalPixelShader(TArrayView<const uint8> InCode, MTLLibraryPtr InLibrary)
{
	FMetalCodeHeader Header;
	Init(InCode, Header, InLibrary);
}

MTLFunctionPtr FMetalPixelShader::GetFunction()
{
	return GetCompiledFunction();
}
