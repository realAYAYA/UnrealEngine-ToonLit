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
	Init(InCode, Header);
}

FMetalPixelShader::FMetalPixelShader(TArrayView<const uint8> InCode, mtlpp::Library InLibrary)
{
	FMetalCodeHeader Header;
	Init(InCode, Header, InLibrary);
}

mtlpp::Function FMetalPixelShader::GetFunction()
{
	return GetCompiledFunction();
}
