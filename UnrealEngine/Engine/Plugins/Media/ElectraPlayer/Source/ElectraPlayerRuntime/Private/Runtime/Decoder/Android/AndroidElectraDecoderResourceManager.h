// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IElectraDecoderResourceDelegate.h"
#include "Renderer/RendererBase.h"

class IElectraDecoderVideoOutput;
struct FDecoderTimeStamp;

namespace Electra
{

class FElectraDecoderResourceManagerAndroid : public IElectraDecoderResourceDelegateAndroid
{
public:
	static bool Startup();
	static void Shutdown();
	static TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> GetDelegate();

	static bool SetupRenderBufferFromDecoderOutput(IMediaRenderer::IBuffer* InOutBufferToSetup, TSharedPtr<FParamDict, ESPMode::ThreadSafe> InOutBufferPropertes, TSharedPtr<IElectraDecoderVideoOutput, ESPMode::ThreadSafe> InDecoderOutput, IDecoderPlatformResource* InPlatformSpecificResource);

	virtual ~FElectraDecoderResourceManagerAndroid();
	IDecoderPlatformResource* CreatePlatformResource(void* InOwnerHandle, EDecoderPlatformResourceType InDecoderResourceType, const TMap<FString, FVariant> InOptions) override;
	void ReleasePlatformResource(void* InOwnerHandle, IDecoderPlatformResource* InHandleToDestroy) override;

private:
	class FInstanceVars;
	FCriticalSection Lock;
	TSet<FInstanceVars*> InstanceVars;
	static void ReleaseToSurface(uint32 InID, const FDecoderTimeStamp& Time);
};

using FPlatformElectraDecoderResourceManager = FElectraDecoderResourceManagerAndroid;

} // namespace Electra
