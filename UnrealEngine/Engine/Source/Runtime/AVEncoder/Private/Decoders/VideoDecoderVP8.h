// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VideoDecoderFactory.h"


namespace AVEncoder
{

PRAGMA_DISABLE_DEPRECATION_WARNINGS
class FVideoDecoderVP8 : public FVideoDecoder
PRAGMA_ENABLE_DEPRECATION_WARNINGS
{
public:
	// register decoder with video decoder factory
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	static void Register(FVideoDecoderFactory& InFactory);
	virtual bool Setup(const FInit& InInit) = 0;
	virtual EDecodeResult Decode(const FVideoDecoderInput* InInput) = 0;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	virtual void Shutdown() = 0;

protected:
	virtual ~FVideoDecoderVP8() = default;
};

} // namespace AVEncoder
