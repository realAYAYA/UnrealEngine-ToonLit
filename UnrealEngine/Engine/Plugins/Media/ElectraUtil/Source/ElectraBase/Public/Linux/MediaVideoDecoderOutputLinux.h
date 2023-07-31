// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MediaVideoDecoderOutput.h"
#include "Containers/Array.h"

class FVideoDecoderOutputLinux : public FVideoDecoderOutput
{
public:
	~FVideoDecoderOutputLinux() = default;

	virtual const TArray<uint8>& GetBuffer() const = 0;
	virtual FIntPoint GetBufferDimensions() const = 0;
	virtual uint32 GetStride() const = 0;
};
