// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnsyncCommon.h"
#include "UnsyncCore.h"

namespace unsync {

struct FCmdDiffOptions
{
	FPath				   Source;
	FPath				   Base;
	FPath				   Output;
	int32				   CompressionLevel = 3;
	uint32				   BlockSize		= 0;
	EWeakHashAlgorithmID   WeakHasher		= EWeakHashAlgorithmID::BuzHash;
	EStrongHashAlgorithmID StrongHasher		= EStrongHashAlgorithmID::Blake3_128;
};

int32 CmdDiff(const FCmdDiffOptions& Options);

}  // namespace unsync
