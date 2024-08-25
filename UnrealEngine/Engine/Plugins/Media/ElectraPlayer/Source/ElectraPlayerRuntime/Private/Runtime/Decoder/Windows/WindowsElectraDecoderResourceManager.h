// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IElectraDecoderResourceDelegate.h"
#include "Containers/Set.h"
#include "Templates/Function.h"
#include "Renderer/RendererBase.h"

class IElectraDecoderVideoOutput;

namespace Electra
{

class FElectraDecoderResourceManagerWindows : public IElectraDecoderResourceDelegateWindows
{
public:
	struct FCallbacks
	{
		TFunction<bool(void ** /*OutD3DDevice*/, int32* /*OutD3DVersionTimes1000*/, void* /*UserValue*/)> GetD3DDevice;
		TFunction<bool(TFunction<void()>&& CodeToRun, IAsyncConsecutiveTaskSync* TaskSync)> RunCodeAsync;
		TFunction<TSharedPtr<IElectraDecoderResourceDelegateBase::IAsyncConsecutiveTaskSync, ESPMode::ThreadSafe>()> CreateAsyncConsecutiveTaskSync;

		void* UserValue = nullptr;
	};

	static bool Startup(const FCallbacks& InCallbacks);
	static void Shutdown();
	static TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> GetDelegate();

	static bool SetupRenderBufferFromDecoderOutput(IMediaRenderer::IBuffer* InOutBufferToSetup, TSharedPtr<FParamDict, ESPMode::ThreadSafe> InOutBufferPropertes, TSharedPtr<IElectraDecoderVideoOutput, ESPMode::ThreadSafe> InDecoderOutput, IDecoderPlatformResource* InPlatformSpecificResource);

	IDecoderPlatformResource* CreatePlatformResource(void* InOwnerHandle, EDecoderPlatformResourceType InDecoderResourceType, const TMap<FString, FVariant> InOptions) override;
	void ReleasePlatformResource(void* InOwnerHandle, IDecoderPlatformResource* InHandleToDestroy) override;

	virtual bool GetD3DDevice(void **OutD3DDevice, int32* OutD3DVersionTimes1000) override;

	virtual TSharedPtr<IElectraDecoderResourceDelegateBase::IAsyncConsecutiveTaskSync, ESPMode::ThreadSafe> CreateAsyncConsecutiveTaskSync() override;
	virtual bool RunCodeAsync(TFunction<void()>&& CodeToRun, IAsyncConsecutiveTaskSync* TaskSync) override;

	virtual ~FElectraDecoderResourceManagerWindows();

private:
	class FInstanceVars;
	static bool SetupRenderBufferFromDecoderOutputFromMFSample(IMediaRenderer::IBuffer* InOutBufferToSetup, TSharedPtr<FParamDict, ESPMode::ThreadSafe> InOutBufferPropertes, TSharedPtr<IElectraDecoderVideoOutput, ESPMode::ThreadSafe> InDecoderOutput, IDecoderPlatformResource* InPlatformSpecificResource);
};

using FPlatformElectraDecoderResourceManager = FElectraDecoderResourceManagerWindows;

} // namespace Electra
