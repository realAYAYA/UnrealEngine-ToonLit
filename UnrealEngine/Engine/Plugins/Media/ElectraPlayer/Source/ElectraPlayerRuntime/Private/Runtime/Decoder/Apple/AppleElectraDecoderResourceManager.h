// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IElectraDecoderResourceDelegate.h"

#include "HAL/Platform.h"

#if PLATFORM_MAC || PLATFORM_IOS || PLATFORM_TVOS

#include "Renderer/RendererBase.h"

class IElectraDecoderVideoOutput;
struct FDecoderTimeStamp;

namespace Electra
{

class FElectraDecoderResourceManagerApple : public IElectraDecoderResourceDelegateApple
{
public:
	struct FCallbacks
	{
		TFunction<bool(void ** /*OutMetalDevice*/, void* /*UserValue*/)> GetMetalDevice;
		void* UserValue = nullptr;
	};

	static bool Startup(const FCallbacks& InCallbacks);
	static void Shutdown();
	static TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> GetDelegate();

	static bool SetupRenderBufferFromDecoderOutput(IMediaRenderer::IBuffer* InOutBufferToSetup, TSharedPtr<FParamDict, ESPMode::ThreadSafe> InOutBufferPropertes, TSharedPtr<IElectraDecoderVideoOutput, ESPMode::ThreadSafe> InDecoderOutput, IDecoderPlatformResource* InPlatformSpecificResource);

	IDecoderPlatformResource* CreatePlatformResource(void* InOwnerHandle, EDecoderPlatformResourceType InDecoderResourceType, const TMap<FString, FVariant> InOptions) override;
	void ReleasePlatformResource(void* InOwnerHandle, IDecoderPlatformResource* InHandleToDestroy) override;

	virtual bool GetMetalDevice(void **OutMetalDevice) override;

	virtual ~FElectraDecoderResourceManagerApple();

private:
	class FInstanceVars;
};

using FPlatformElectraDecoderResourceManager = FElectraDecoderResourceManagerApple;

} // namespace Electra

#endif
