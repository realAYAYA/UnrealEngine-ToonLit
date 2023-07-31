// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MediaVideoDecoderOutput.h"

#include "Templates/RefCounting.h"
#include "Containers/Array.h"
#include "PixelFormat.h"

class FVideoDecoderOutputAndroid : public FVideoDecoderOutput
{
public:
	enum class EOutputType
	{
		Unknown = 0,
		DirectToSurfaceAsView,	//!< Output surface is assumed to be GUI element
		DirectToSurfaceAsQueue,	//!< Output surface is assumed to be queue for texture output
	};

	virtual EOutputType GetOutputType() const = 0;

	virtual void ReleaseToSurface() const = 0;
};

