// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"

#if PLATFORM_WINDOWS

#include "IElectraPlayerInterface.h"
#include "ElectraPlayerPrivate.h"

#include "ElectraPlayer.h"

#include "ParameterDictionary.h"
#include "VideoDecoderResourceDelegate.h"

#include "RHIDefinitions.h"

#include "WindowsElectraDecoderResourceManager.h"

// ----------------------------------------------------------------------------------------------------------------------

THIRD_PARTY_INCLUDES_START
#include <mfapi.h>
#include <d3d12.h>
#include <dxgi1_4.h>
THIRD_PARTY_INCLUDES_END
#ifdef ELECTRA_HAVE_DX11
#pragma comment(lib, "D3D11.lib")
#endif
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "DXGI.lib")

// ----------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------------------

namespace Electra
{

	// ----------------------------------------------------------------------------------------------------------------------

	/**
	 * Shutdown of module
	 */
	bool PlatformShutdown()
	{
		FElectraDecoderResourceManagerWindows::Shutdown();
		return true;
	}

	class FVideoDecoderResourceDelegate : public IVideoDecoderResourceDelegate
	{
	public:
		FVideoDecoderResourceDelegate(const TWeakPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe>& InAdapterDelegate)
		: PlayerAdapterDelegate(InAdapterDelegate)
		{
			const TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PinnedAdapterDelegate = PlayerAdapterDelegate.Pin();
			check(PinnedAdapterDelegate.IsValid());

			// Grab a reference to the IElectraPlayerResourceDelegate as we need it still when IElectraPlayerAdapterDelegate is already destroyed
			ResourceDelegate = PinnedAdapterDelegate->GetResourceDelegate();
			check(ResourceDelegate.IsValid());
		}

		virtual ~FVideoDecoderResourceDelegate() = default;

		virtual void ExecuteCodeWithCopyCommandQueueUsage(TFunction<void(ID3D12CommandQueue*)>&& CodeToRun) override
		{
			check(ResourceDelegate.IsValid());
			ResourceDelegate->ExecuteCodeWithCopyCommandQueueUsage(MoveTemp(CodeToRun));
		}

	private:
		TWeakPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> PlayerAdapterDelegate;
		TSharedPtr<IElectraPlayerResourceDelegate, ESPMode::ThreadSafe> ResourceDelegate;
	};

	TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe> PlatformCreateVideoDecoderResourceDelegate(const TWeakPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe>& AdapterDelegate)
	{
		return MakeShared<FVideoDecoderResourceDelegate, ESPMode::ThreadSafe>(AdapterDelegate);
	}

} //namespace Electra

// ----------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------------------

/**
 * Public call for pre-use initialization after module is loaded, allowing for user passed parameters
 */
bool FElectraPlayerPlatform::StartupPlatformResources(const Electra::FParamDict& Params)
{
	FElectraDecoderResourceManagerWindows::FCallbacks Callbacks;
	auto GetDeviceTypeCallback = reinterpret_cast<void(*)(void**, int64*)>(Params.GetValue(FName(TEXT("GetDeviceTypeCallback"))).SafeGetPointer());

	Callbacks.GetD3DDevice = [GetDeviceTypeCallback](void** OutD3DDevice, int32* OutD3DVersionTimes1000, void*) -> bool
		{
			if (GetDeviceTypeCallback)
			{
				void* DevicePointer = nullptr;
				int64 DeviceType = 0;
				GetDeviceTypeCallback(&DevicePointer, &DeviceType);
				const ERHIInterfaceType RHIType = (ERHIInterfaceType)DeviceType;
				if (DevicePointer && (RHIType == ERHIInterfaceType::D3D11 || RHIType == ERHIInterfaceType::D3D12))
				{
					*OutD3DDevice = DevicePointer;
					*OutD3DVersionTimes1000 = RHIType == ERHIInterfaceType::D3D11 ? 11000 : 12000;
					return true;
				}
			}
			return false;
		};

	auto RunCodeAsyncCallback = reinterpret_cast<void(*)(TFunction<void()>&& CodeToRun, IElectraDecoderResourceDelegateWindows::IAsyncConsecutiveTaskSync * TaskSync)>(Params.GetValue(FName(TEXT("RunCodeAsyncCallback"))).SafeGetPointer());

	Callbacks.RunCodeAsync = [RunCodeAsyncCallback](TFunction<void()>&& CodeToRun, IElectraDecoderResourceDelegateWindows::IAsyncConsecutiveTaskSync* TaskSync) -> bool
	{
		if (RunCodeAsyncCallback)
		{
			RunCodeAsyncCallback(MoveTemp(CodeToRun), TaskSync);
			return true;
		}
		return false;
	};

	auto CreateAsyncConsecutiveTaskSyncCallback = reinterpret_cast<TSharedPtr<IElectraDecoderResourceDelegateWindows::IAsyncConsecutiveTaskSync, ESPMode::ThreadSafe>(*)()>(Params.GetValue(FName(TEXT("CreateAsyncConsecutiveTaskSync"))).SafeGetPointer());

	Callbacks.CreateAsyncConsecutiveTaskSync = [CreateAsyncConsecutiveTaskSyncCallback]() -> TSharedPtr<IElectraDecoderResourceDelegateWindows::IAsyncConsecutiveTaskSync, ESPMode::ThreadSafe>
	{
		if (CreateAsyncConsecutiveTaskSyncCallback)
		{
			return CreateAsyncConsecutiveTaskSyncCallback();
		}
		return nullptr;
	};

	return FElectraDecoderResourceManagerWindows::Startup(Callbacks);
}

void FElectraPlayer::PlatformNotifyOfOptionChange()
{
}

void FElectraPlayer::PlatformSuspendOrResumeDecoders(bool /*bSuspend*/, const Electra::FParamDict& /*InOptions*/)
{
}

#endif
