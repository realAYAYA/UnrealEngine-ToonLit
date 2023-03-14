// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"

#if PLATFORM_WINDOWS


#include "IElectraPlayerInterface.h"
#include "ElectraPlayerPrivate.h"
#include "ElectraPlayerPrivate_Platform.h"

#include "ElectraPlayer.h"

#include "ParameterDictionary.h"
#include "RHIDefinitions.h"

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

namespace Electra {

// ----------------------------------------------------------------------------------------------------------------------
	
TUniquePtr<FDXDeviceInfo> FDXDeviceInfo::s_DXDeviceInfo;

// ----------------------------------------------------------------------------------------------------------------------
	
static bool bHaveDirectXResources = false;

// ----------------------------------------------------------------------------------------------------------------------

bool CreateDirectXResources(const FParamDict& Params)
{
	if (!GIsClient && !bHaveDirectXResources)
	{
		// We claim success in all cases - avoiding an error code if we are not a client
		return true;
	}

	Electra::FDXDeviceInfo::s_DXDeviceInfo = MakeUnique<Electra::FDXDeviceInfo>();
	Electra::FDXDeviceInfo::s_DXDeviceInfo->DxVersion = Electra::FDXDeviceInfo::ED3DVersion::VersionUnknown;

	FDXDeviceInfo::s_DXDeviceInfo->RenderingDx11Device = nullptr;

	if (Electra::IsWindows8Plus())
	{
		static const FString DeviceKey("Device");
		static const FString DeviceTypeKey("DeviceType");

		if (!Params.HaveKey(DeviceKey) || !Params.HaveKey(DeviceTypeKey))
		{
			UE_LOG(LogElectraPlayer, Warning, TEXT("ElectraPlayer: Missing rendering device info"));
			return false;
		}


		UINT ResetToken = 0;
		CHECK_HR(MFCreateDXGIDeviceManager(&ResetToken, Electra::FDXDeviceInfo::s_DXDeviceInfo->DxDeviceManager.GetInitReference()));
		if (!Electra::FDXDeviceInfo::s_DXDeviceInfo->DxDeviceManager.IsValid())
		{
			UE_LOG(LogElectraPlayer, Warning, TEXT("ElectraPlayer: Could not create DXGI device manager"));
			return false;
		}

		// Create device from same adapter as already existing device

		uint32 UE4DxDeviceCreationFlags = 0;
		TRefCountPtr<IDXGIAdapter> DXGIAdapter;

		const ERHIInterfaceType RHIType = (ERHIInterfaceType)Params.GetValue(DeviceTypeKey).GetInt64();

		if (RHIType == ERHIInterfaceType::D3D11)
		{
			ID3D11Device* UE4DxDevice = static_cast<ID3D11Device*>(Params.GetValue(DeviceKey).GetPointer());

			TRefCountPtr<IDXGIDevice> DXGIDevice;
			UE4DxDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)DXGIDevice.GetInitReference());

			DXGIDevice->GetAdapter((IDXGIAdapter**)DXGIAdapter.GetInitReference());

			UE4DxDeviceCreationFlags = UE4DxDevice->GetCreationFlags();

			FDXDeviceInfo::s_DXDeviceInfo->RenderingDx11Device = TRefCountPtr<ID3D11Device>(UE4DxDevice, true);

			Electra::FDXDeviceInfo::s_DXDeviceInfo->DxVersion = Electra::FDXDeviceInfo::ED3DVersion::Version11Win8;
		}
		else if (RHIType == ERHIInterfaceType::D3D12)
		{
			TRefCountPtr<IDXGIFactory4> DXGIFactory;
			CHECK_HR(CreateDXGIFactory(__uuidof(IDXGIFactory4), (void**)DXGIFactory.GetInitReference()));

			ID3D12Device* UE4DxDevice = static_cast<ID3D12Device*>(Params.GetValue(DeviceKey).GetPointer());

			LUID Luid = UE4DxDevice->GetAdapterLuid();

			CHECK_HR(DXGIFactory->EnumAdapterByLuid(Luid, __uuidof(IDXGIAdapter), (void**)DXGIAdapter.GetInitReference()));

			Electra::FDXDeviceInfo::s_DXDeviceInfo->DxVersion = Electra::FDXDeviceInfo::ED3DVersion::Version12Win10;
		}
		else
		{
			UE_LOG(LogElectraPlayer, Warning, TEXT("ElectraPlayer: Dynamic RHI is not for DX11 or DX12"));
			return false;
		}

#ifdef ELECTRA_HAVE_DX11
		D3D_FEATURE_LEVEL FeatureLevel;

		uint32 DeviceCreationFlags = 0;
		if ((UE4DxDeviceCreationFlags & D3D11_CREATE_DEVICE_DEBUG) != 0)
		{
			DeviceCreationFlags |= D3D11_CREATE_DEVICE_DEBUG;
		}

		CHECK_HR(D3D11CreateDevice(
			DXGIAdapter,
			D3D_DRIVER_TYPE_UNKNOWN,
			nullptr,
			DeviceCreationFlags,
			nullptr,
			0,
			D3D11_SDK_VERSION,
			Electra::FDXDeviceInfo::s_DXDeviceInfo->DxDevice.GetInitReference(),
			&FeatureLevel,
			Electra::FDXDeviceInfo::s_DXDeviceInfo->DxDeviceContext.GetInitReference()));

		if (FeatureLevel < D3D_FEATURE_LEVEL_9_3)
		{
			Electra::FDXDeviceInfo::s_DXDeviceInfo->DxVersion = Electra::FDXDeviceInfo::ED3DVersion::VersionUnknown;
			UE_LOG(LogElectraPlayer, Warning, TEXT("ElectraPlayer: Unable to Create D3D11 Device with feature level 9.3 or above"));
			return false;
		}

		if (!Electra::FDXDeviceInfo::s_DXDeviceInfo->DxDevice.IsValid())
		{
			UE_LOG(LogElectraPlayer, Warning, TEXT("ElectraPlayer: Could not create DX11 device"));
			return false;
		}

		CHECK_HR(Electra::FDXDeviceInfo::s_DXDeviceInfo->DxDeviceManager->ResetDevice(Electra::FDXDeviceInfo::s_DXDeviceInfo->DxDevice, ResetToken));

		// multithread protect the newly created device as we're going to use it from decoding thread and from render thread for texture
		// sharing between decoding and rendering DX devices
		TRefCountPtr<ID3D10Multithread> DxMultithread;
		Electra::FDXDeviceInfo::s_DXDeviceInfo->DxDevice->QueryInterface(__uuidof(ID3D10Multithread), (void**)DxMultithread.GetInitReference());
		DxMultithread->SetMultithreadProtected(1);

		UE_LOG(LogElectraPlayer, Log, TEXT("D3D11 Device for h/w accelerated decoding created: %p"), Electra::FDXDeviceInfo::s_DXDeviceInfo->DxDevice.GetReference());
#endif
		bHaveDirectXResources = true;
	}
	else
	{
#ifdef ELECTRA_HAVE_DX9
		UINT ResetToken = 0;
		CHECK_HR_DX9(DXVA2CreateDirect3DDeviceManager9(&ResetToken, Electra::FDXDeviceInfo::s_DXDeviceInfo->Dx9DeviceManager.GetInitReference()));

		Electra::FDXDeviceInfo::s_DXDeviceInfo->Dx9 = Direct3DCreate9(D3D_SDK_VERSION);
		check(Electra::FDXDeviceInfo::s_DXDeviceInfo->Dx9);
		if (!Electra::FDXDeviceInfo::s_DXDeviceInfo->Dx9)
		{
			UE_LOG(LogElectraPlayer, Warning, TEXT("Direct3DCreate9(D3D_SDK_VERSION) failed"));
			return false;
		}

		Electra::FDXDeviceInfo::s_DXDeviceInfo->Dx9Device = nullptr;
		D3DPRESENT_PARAMETERS PresentParam;
		FMemory::Memzero(PresentParam);
		PresentParam.BackBufferWidth = 1;
		PresentParam.BackBufferHeight = 1;
		PresentParam.BackBufferFormat = D3DFMT_UNKNOWN;
		PresentParam.BackBufferCount = 1;
		PresentParam.SwapEffect = D3DSWAPEFFECT_DISCARD;
		PresentParam.hDeviceWindow = nullptr;
		PresentParam.Windowed = true;
		PresentParam.Flags = D3DPRESENTFLAG_VIDEO;
		CHECK_HR_DX9(Electra::FDXDeviceInfo::s_DXDeviceInfo->Dx9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, nullptr, D3DCREATE_MULTITHREADED | D3DCREATE_MIXED_VERTEXPROCESSING, &PresentParam, Electra::FDXDeviceInfo::s_DXDeviceInfo->Dx9Device.GetInitReference()));

		CHECK_HR_DX9(Electra::FDXDeviceInfo::s_DXDeviceInfo->Dx9DeviceManager->ResetDevice(Electra::FDXDeviceInfo::s_DXDeviceInfo->Dx9Device, ResetToken));

		Electra::FDXDeviceInfo::s_DXDeviceInfo->DxVersion = Electra::FDXDeviceInfo::ED3DVersion::Version9Win7;
		bHaveDirectXResources = true;
#endif
	}
	return bHaveDirectXResources;
}


bool DestroyDirectXResources()
{
	Electra::FDXDeviceInfo::s_DXDeviceInfo.Reset();
	bHaveDirectXResources = false;
	return true;
}


bool IsWindows8Plus()
{
	return FPlatformMisc::VerifyWindowsVersion(6, 2);
}


bool IsWindows7Plus()
{
	return FPlatformMisc::VerifyWindowsVersion(6, 1);
}

// ----------------------------------------------------------------------------------------------------------------------

// some Windows versions don't have Media Foundation preinstalled. We configure MF DLLs as delay-loaded and load them manually here
// checking the result and avoiding error message box if failed
static bool LoadMediaFoundationDLLs()
{
	// Ensure that all required modules are preloaded so they are not loaded just-in-time, causing a hitch.
	if (Electra::IsWindows8Plus())
	{
		return FPlatformProcess::GetDllHandle(TEXT("mf.dll"))
			&& FPlatformProcess::GetDllHandle(TEXT("mfplat.dll"))
			&& FPlatformProcess::GetDllHandle(TEXT("msmpeg2vdec.dll"))
			&& FPlatformProcess::GetDllHandle(TEXT("MSAudDecMFT.dll"));
	}
	else // Windows 7
	{
		return FPlatformProcess::GetDllHandle(TEXT("mf.dll"))
			&& FPlatformProcess::GetDllHandle(TEXT("mfplat.dll"))
			&& FPlatformProcess::GetDllHandle(TEXT("msmpeg2vdec.dll"))
			&& FPlatformProcess::GetDllHandle(TEXT("msmpeg2adec.dll"));
	}
}

// ----------------------------------------------------------------------------------------------------------------------

/**
 * Early startup during module load process
 */
bool PlatformEarlyStartup()
{
	// Win7+ only
	if (!IsWindows7Plus())
	{
		UE_LOG(LogElectraPlayer, Log, TEXT("Electra is incompatible with Windows prior to 7.0 version: %s"), *FPlatformMisc::GetOSVersion());
		return false;
	}

	if (!LoadMediaFoundationDLLs())
	{
		UE_LOG(LogElectraPlayer, Log, TEXT("Electra can't load Media Foundation, %s"), *FPlatformMisc::GetOSVersion());
		return false;
	}

	HRESULT Res = MFStartup(MF_VERSION);
	checkf(SUCCEEDED(Res), TEXT("MFStartup failed: %d"), Res);

	return true;
}

/**
 * Any platform specific memory hooks
 */
bool PlatformMemorySetup()
{
	return true;
}

/**
 * Shutdown of module
 */
bool PlatformShutdown()
{
	bool bRes = Electra::DestroyDirectXResources();
	if (!bRes)
	{
		UE_LOG(LogElectraPlayer, Warning, TEXT("ElectraPlayer: Failed to unintialize DXGI Manager and Device"));
	}
	MFShutdown();
	return bRes;
}


TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe> PlatformCreateVideoDecoderResourceDelegate(const TWeakPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe>& AdapterDelegate)
{
	return nullptr;
}

} //namespace Electra

// ----------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------------------

/**
 * Public call for pre-use initialization after module is loaded, allowing for user passed parameters
 */
bool FElectraPlayerPlatform::StartupPlatformResources(const Electra::FParamDict& Params)
{
	return Electra::CreateDirectXResources(Params);
}

void FElectraPlayer::PlatformNotifyOfOptionChange()
{
}

void FElectraPlayer::PlatformSuspendOrResumeDecoders(bool /*bSuspend*/)
{
}

#endif


