// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if ELECTRA_DECODERS_HAVE_PLATFORM_DEFAULTS
#include COMPILED_PLATFORM_HEADER(ElectraDecoderResourceManager.h)

#else

#include "Renderer/RendererBase.h"

class IElectraDecoderVideoOutput;

class FPlatformElectraDecoderResourceManager
{
public:
	static bool Startup()
	{ return true; }
	static void Shutdown()
	{ }
	static TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> GetDelegate()
	{ return nullptr; }

	class IDecoderPlatformResource;

	static bool SetupRenderBufferFromDecoderOutput(Electra::IMediaRenderer::IBuffer* InOutBufferToSetup, Electra::FParamDict* InOutBufferPropertes, TSharedPtr<IElectraDecoderVideoOutput, ESPMode::ThreadSafe> InDecoderOutput, IDecoderPlatformResource* InPlatformSpecificResource)
	{ return false; }
};

#endif
