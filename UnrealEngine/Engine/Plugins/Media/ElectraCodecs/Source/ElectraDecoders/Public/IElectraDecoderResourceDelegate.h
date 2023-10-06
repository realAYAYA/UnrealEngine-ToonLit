// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IElectraDecoderResourceDelegateBase.h"

#if ELECTRA_DECODERS_HAVE_PLATFORM_DEFAULTS
#include COMPILED_PLATFORM_HEADER(ElectraDecoderResourceDelegate.h)

#else

class IElectraDecoderResourceDelegateNull : public IElectraDecoderResourceDelegateBase
{
public:
	virtual ~IElectraDecoderResourceDelegateNull()
	{ }
};

using IElectraDecoderResourceDelegate = IElectraDecoderResourceDelegateNull;

#endif
