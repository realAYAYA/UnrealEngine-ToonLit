// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IElectraDecoderResourceDelegate.h"

#include "HAL/Platform.h"

#if PLATFORM_LINUX || PLATFORM_UNIX

#include "Renderer/RendererBase.h"

class IElectraDecoderVideoOutput;
struct FDecoderTimeStamp;

namespace Electra
{

class FElectraDecoderResourceManagerLinux : public IElectraDecoderResourceDelegateLinux
{
public:

	static bool Startup();
	static void Shutdown();
	static TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> GetDelegate();

	static bool SetupRenderBufferFromDecoderOutput(IMediaRenderer::IBuffer* InOutBufferToSetup, TSharedPtr<FParamDict, ESPMode::ThreadSafe> InOutBufferPropertes, TSharedPtr<IElectraDecoderVideoOutput, ESPMode::ThreadSafe> InDecoderOutput, IDecoderPlatformResource* InPlatformSpecificResource);

	virtual ~FElectraDecoderResourceManagerLinux();
};

using FPlatformElectraDecoderResourceManager = FElectraDecoderResourceManagerLinux;

} // namespace Electra

#endif
