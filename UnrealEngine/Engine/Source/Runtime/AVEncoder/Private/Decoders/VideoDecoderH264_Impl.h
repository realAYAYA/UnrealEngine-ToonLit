// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VideoDecoderFactory.h"


namespace AVEncoder
{

class FVideoDecoderH264_Impl : public FVideoDecoder
{
public:
	// register decoder with video decoder factory
	static void Register(FVideoDecoderFactory& InFactory);

	virtual bool Setup(const FInit& InInit) = 0;
	virtual void Shutdown() = 0;

	virtual EDecodeResult Decode(const FVideoDecoderInput* InInput) = 0;
protected:
	virtual ~FVideoDecoderH264_Impl() = default;
};

} // namespace AVEncoder
