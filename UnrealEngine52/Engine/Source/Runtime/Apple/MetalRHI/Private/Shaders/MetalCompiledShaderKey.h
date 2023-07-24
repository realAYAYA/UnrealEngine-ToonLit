// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCompiledShaderKey.h: Metal RHI Compiled Shader Key.
=============================================================================*/

#pragma once

struct FMetalCompiledShaderKey
{
	FMetalCompiledShaderKey(uint32 InCodeSize, uint32 InCodeCRC, uint32 InConstants)
		: CodeSize(InCodeSize)
		, CodeCRC(InCodeCRC)
		, Constants(InConstants)
	{
		// VOID
	}

	friend bool operator ==(const FMetalCompiledShaderKey& A, const FMetalCompiledShaderKey& B)
	{
		return A.CodeSize == B.CodeSize && A.CodeCRC == B.CodeCRC && A.Constants == B.Constants;
	}

	friend uint32 GetTypeHash(const FMetalCompiledShaderKey &Key)
	{
		return HashCombine(HashCombine(GetTypeHash(Key.CodeSize), GetTypeHash(Key.CodeCRC)), GetTypeHash(Key.Constants));
	}

	uint32 CodeSize;
	uint32 CodeCRC;
	uint32 Constants;
};
