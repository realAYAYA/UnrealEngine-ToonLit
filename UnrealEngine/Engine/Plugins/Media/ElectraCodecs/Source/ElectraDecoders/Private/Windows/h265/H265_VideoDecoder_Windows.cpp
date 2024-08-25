// Copyright Epic Games, Inc. All Rights Reserved.

#include "h265/H265_VideoDecoder_Windows.h"

#ifdef ELECTRA_DECODERS_ENABLE_DX

#include "DX/VideoDecoderH265_DX.h"
#include "DX/DecoderErrors_DX.h"

#include "ElectraDecodersUtils.h"
#include "IElectraDecoderFeaturesAndOptions.h"

/*********************************************************************************************************************/
#include COMPILED_PLATFORM_HEADER(PlatformHeaders_Video_DX.h)

#ifdef ELECTRA_DECODERS_HAVE_DX11
#pragma comment(lib, "D3D11.lib")
#endif
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "DXGI.lib")

/*********************************************************************************************************************/

namespace ElectraGuidsH265
{
DEFINE_GUID(MF_SA_D3D11_AWARE, 0x206b4fc8, 0xfcf9, 0x4c51, 0xaf, 0xe3, 0x97, 0x64, 0x36, 0x9e, 0x33, 0xa0);
}


class FH265VideoDecoderFactoryWindows : public IElectraCodecFactory
{
public:
	virtual ~FH265VideoDecoderFactoryWindows()
	{}

	int32 SupportsFormat(const FString& InCodecFormat, bool bInEncoder, const TMap<FString, FVariant>& InOptions) const override
	{
		// Encoder? Not supported here!
		if (bInEncoder)
		{
			return 0;
		}

		// Get properties that cannot be passed with the codec string alone.
		int32 Width = (int32)ElectraDecodersUtil::GetVariantValueSafeI64(InOptions, TEXT("width"), 0);
		int32 Height = (int32)ElectraDecodersUtil::GetVariantValueSafeI64(InOptions, TEXT("height"), 0);
		//int64 bps = ElectraDecodersUtil::GetVariantValueSafeI64(InOptions, TEXT("bitrate"), 0);
		double fps = ElectraDecodersUtil::GetVariantValueSafeDouble(InOptions, TEXT("fps"), 0.0);

		ElectraDecodersUtil::FMimeTypeVideoCodecInfo ci;
		// Codec?
		if (ElectraDecodersUtil::ParseCodecH265(ci, InCodecFormat))
		{
			// Ok.
		}
		// Mime type?
		else if (ElectraDecodersUtil::ParseMimeTypeWithCodec(ci, InCodecFormat))
		{
			// Note: This should ideally have the resolution.
		}
		else
		{
			return 0;
		}

		// Check if supported.
		TArray<IElectraVideoDecoderH265_DX::FSupportedConfiguration> Configs;
		IElectraVideoDecoderH265_DX::PlatformGetSupportedConfigurations(Configs);
		bool bSupported = false;
		for(int32 i=0; i<Configs.Num(); ++i)
		{
			const IElectraVideoDecoderH265_DX::FSupportedConfiguration& Cfg = Configs[i];
			if (Cfg.Profile == ci.Profile)
			{
				if (ci.Level > Cfg.Level)
				{
					continue;
				}
				if ((Width > Cfg.Width && Cfg.Width) || (Height > Cfg.Height && Cfg.Height))
				{
					continue;
				}
				if (fps > 0.0 && Cfg.FramesPerSecond && (int32)fps > Cfg.FramesPerSecond)
				{
					continue;
				}
				if (Cfg.Num8x8Macroblocks && (((Align(Width, 8) * Align(Height, 8)) / 64) * (fps > 0.0 ? fps : 1.0)) > Cfg.Num8x8Macroblocks)
				{
					continue;
				}
				bSupported = true;
				break;
			}
		}

		return bSupported ? 1 : 0;
	}


	void GetConfigurationOptions(TMap<FString, FVariant>& OutOptions) const override
	{
		IElectraVideoDecoderH265_DX::PlatformGetConfigurationOptions(OutOptions);
	}

	TSharedPtr<IElectraDecoder, ESPMode::ThreadSafe> CreateDecoderForFormat(const FString& InCodecFormat, const TMap<FString, FVariant>& InOptions, TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate) override
	{
		check(SupportsFormat(InCodecFormat, false, InOptions));
		return IElectraVideoDecoderH265_DX::Create(InOptions, InResourceDelegate);
	}

};


TSharedPtr<IElectraCodecFactory, ESPMode::ThreadSafe> FH265VideoDecoderWindows::CreateFactory()
{
	if (IElectraVideoDecoderH265_DX::PlatformStaticInitialize())
	{
		return MakeShared<FH265VideoDecoderFactoryWindows, ESPMode::ThreadSafe>();
	}
	return nullptr;
}


/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/



namespace IElectraVideoDecoderH265_DX_Platform
{
	static TArray<IElectraVideoDecoderH265_DX::FSupportedConfiguration> DecoderConfigurations;
	static bool bDecoderConfigurationsDirty = true;

	class FPlatformHandle : public IElectraVideoDecoderH265_DX::IPlatformHandle
	{
	public:
		virtual ~FPlatformHandle()
		{
			Close();
		}
		virtual int32 GetDXVersionTimes1000() const override
		{ return DXVersion; }
		virtual void* GetMFTransform() const override
		{ return MFT.GetReference(); }
		virtual void* GetDXDevice() const override
		{ return Ctx.DxDevice.GetReference(); }
		virtual void* GetDXDeviceContext() const override
		{ return Ctx.DxDeviceContext.GetReference(); }
		virtual bool IsSoftware() const override
		{ return bIsSW; }
		
		void Close()
		{
			MFT.SafeRelease();
			Ctx.Release();
			DxDeviceManager.SafeRelease();
		}

		TRefCountPtr<IMFTransform> MFT;
		int32 DXVersion = 0;
		bool bIsSW = false;


		FElectraVideoDecoderDXDeviceContext Ctx;
		TRefCountPtr<IMFDXGIDeviceManager> DxDeviceManager;
	};
}

bool IElectraVideoDecoderH265_DX::PlatformStaticInitialize()
{
	return true;
}

void IElectraVideoDecoderH265_DX::PlatformGetSupportedConfigurations(TArray<FSupportedConfiguration>& OutSupportedConfigurations)
{
	if (IElectraVideoDecoderH265_DX_Platform::bDecoderConfigurationsDirty)
	{
		IElectraVideoDecoderH265_DX_Platform::DecoderConfigurations.Empty();

		// Main
		IElectraVideoDecoderH265_DX_Platform::DecoderConfigurations.Emplace(IElectraVideoDecoderH265_DX::FSupportedConfiguration(0, 1, 0, 153, 120, 4096, 2304, 0));
		// Main10
		IElectraVideoDecoderH265_DX_Platform::DecoderConfigurations.Emplace(IElectraVideoDecoderH265_DX::FSupportedConfiguration(0, 2, 0, 153, 120, 4096, 2304, 0));

		IElectraVideoDecoderH265_DX_Platform::bDecoderConfigurationsDirty = false;
	}
	OutSupportedConfigurations = IElectraVideoDecoderH265_DX_Platform::DecoderConfigurations;
}

void IElectraVideoDecoderH265_DX::PlatformGetConfigurationOptions(TMap<FString, FVariant>& OutOptions)
{
	OutOptions.Emplace(IElectraDecoderFeature::MinimumNumberOfOutputFrames, FVariant((int32)8));
	OutOptions.Emplace(IElectraDecoderFeature::IsAdaptive, FVariant(false));
	OutOptions.Emplace(IElectraDecoderFeature::SupportsDroppingOutput, FVariant(true));
}


IElectraDecoder::FError IElectraVideoDecoderH265_DX::PlatformCreateMFDecoderTransform(IPlatformHandle** OutPlatformHandle, TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate, const TMap<FString, FVariant>& InOptions)
{
	IElectraDecoder::FError Error;
	check(OutPlatformHandle && InResourceDelegate.IsValid());
	if (OutPlatformHandle)
	{
		*OutPlatformHandle = nullptr;
	}
	if (OutPlatformHandle && InResourceDelegate.IsValid())
	{
		// Must be Windows 8 or better
		if (FPlatformMisc::VerifyWindowsVersion(6, 2))
		{
			TUniquePtr<IElectraVideoDecoderH265_DX_Platform::FPlatformHandle> pfh = MakeUnique<IElectraVideoDecoderH265_DX_Platform::FPlatformHandle>();

			// Create the decoder transform.
			HRESULT hr;
			MFT_REGISTER_TYPE_INFO MediaInputInfo { MFMediaType_Video, MFVideoFormat_HEVC };
			TRefCountPtr<IMFAttributes>	Attributes;
			IMFActivate** ActivateObjects = nullptr;
			UINT32 NumActivateObjects = 0;
			if ((hr = MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER, MFT_ENUM_FLAG_SYNCMFT, &MediaInputInfo, nullptr, &ActivateObjects, &NumActivateObjects)) != S_OK)
			{
				return Error.SetMessage(TEXT("MFTEnumEx() failed")).SetCode(ERRCODE_INTERNAL_BAD_PARAMETER).SetSdkCode(hr);
			}
			if (NumActivateObjects == 0)
			{
				return Error.SetMessage(TEXT("MFTEnumEx() returned zero activation objects")).SetCode(ERRCODE_INTERNAL_BAD_PARAMETER);
			}
			IMFTransform* NewDecoder = nullptr;
			hr = ActivateObjects[0]->ActivateObject(IID_PPV_ARGS(&NewDecoder));
			if (hr == S_OK)
			{
				*(pfh->MFT.GetInitReference()) = NewDecoder;
			}
			for(UINT32 i=0; i<NumActivateObjects; ++i)
			{
				ActivateObjects[i]->Release();
			}
			CoTaskMemFree(ActivateObjects);
			ActivateObjects = nullptr;
			if (hr != S_OK)
			{
				return Error.SetMessage(TEXT("H.265 decoder transform activation failed")).SetCode(ERRCODE_INTERNAL_BAD_PARAMETER).SetSdkCode(hr);
			}
			if ((hr = NewDecoder->GetAttributes(Attributes.GetInitReference())) != S_OK)
			{
				return Error.SetMessage(TEXT("Failed to get H.265 decoder transform attributes")).SetCode(ERRCODE_INTERNAL_BAD_PARAMETER).SetSdkCode(hr);
			}

			// Check if the decoder is DX11 aware, in which case we need to create a device manager and a decoding DX device
			// we can use to enable hardware acceleration.
			uint32 IsDX11Aware = 0;
			/*hr =*/ Attributes->GetUINT32(ElectraGuidsH265::MF_SA_D3D11_AWARE, &IsDX11Aware);
			if (IsDX11Aware)
			{
				uint32 DxDeviceCreationFlags = 0;
				TRefCountPtr<IDXGIAdapter> DXGIAdapter;
				void* ApplicationD3DDevice = nullptr;
				int32 ApplicationD3DDeviceVersion = 0;
				if (InResourceDelegate->GetD3DDevice(&ApplicationD3DDevice, &ApplicationD3DDeviceVersion))
				{
					if (ApplicationD3DDeviceVersion >= 12000)
					{
						TRefCountPtr<IDXGIFactory4> DXGIFactory;
						if ((hr = CreateDXGIFactory(__uuidof(IDXGIFactory4), (void**)DXGIFactory.GetInitReference())) != S_OK)
						{
							return Error.SetMessage(TEXT("CreateDXGIFactory() failed")).SetCode(ERRCODE_INTERNAL_BAD_PARAMETER).SetSdkCode(hr);
						}

						ID3D12Device* DxDevice = static_cast<ID3D12Device*>(ApplicationD3DDevice);
						LUID Luid = DxDevice->GetAdapterLuid();
						if ((hr = DXGIFactory->EnumAdapterByLuid(Luid, __uuidof(IDXGIAdapter), (void**)DXGIAdapter.GetInitReference())) != S_OK)
						{
							return Error.SetMessage(TEXT("EnumAdapterByLuid() failed")).SetCode(ERRCODE_INTERNAL_BAD_PARAMETER).SetSdkCode(hr);
						}

						pfh->DXVersion = 12000;
					}
					else if (ApplicationD3DDeviceVersion >= 11000)
					{
						ID3D11Device* DxDevice = static_cast<ID3D11Device*>(ApplicationD3DDevice);
						TRefCountPtr<IDXGIDevice> DXGIDevice;
						DxDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)DXGIDevice.GetInitReference());
						if (!DXGIDevice.IsValid())
						{
							return Error.SetMessage(TEXT("ID3D11Device::QueryInterface(IDXGIDevice) failed")).SetCode(ERRCODE_INTERNAL_BAD_PARAMETER);
						}
						if ((hr = DXGIDevice->GetAdapter((IDXGIAdapter**)DXGIAdapter.GetInitReference())) != S_OK)
						{
							return Error.SetMessage(TEXT("IDXGIDevice::GetAdapter() failed")).SetCode(ERRCODE_INTERNAL_BAD_PARAMETER).SetSdkCode(hr);;
						}
						DxDeviceCreationFlags = DxDevice->GetCreationFlags();
						pfh->DXVersion = 11000;
					}
					else
					{
						return Error.SetMessage(TEXT("Must be either DirectX 11 or 12")).SetCode(ERRCODE_INTERNAL_BAD_PARAMETER);
					}
				}
				else
				{
					// Not using D3D for rendering. Create decoding device on the default adapter then.
				}

				// Create device manager
				UINT ResetToken = 0;
				if ((hr = MFCreateDXGIDeviceManager(&ResetToken, pfh->DxDeviceManager.GetInitReference())) != S_OK)
				{
					return Error.SetMessage(TEXT("MFCreateDXGIDeviceManager() failed")).SetCode(ERRCODE_INTERNAL_BAD_PARAMETER).SetSdkCode(hr);
				}

				// Try setting the D3D12 device with the device manager.
				bool bAssociatedDeviceManager = false;
#if ALLOW_MFSAMPLE_WITH_DX12
				if (pfh->DXVersion >= 12000)
				{
					bAssociatedDeviceManager = SUCCEEDED(pfh->DxDeviceManager->ResetDevice(static_cast<ID3D12Device*>(ApplicationD3DDevice), ResetToken));
					// note: pfh->Ctx.DxDevice & pfh->Ctx.DxDeviceContext will be nullptr (everything uses the DX12 render device)
				}
#endif

#ifdef ELECTRA_DECODERS_HAVE_DX11
				if (!bAssociatedDeviceManager)
				{
					D3D_FEATURE_LEVEL FeatureLevel;

					uint32 DeviceCreationFlags = 0;
					if ((DxDeviceCreationFlags & D3D11_CREATE_DEVICE_DEBUG) != 0)
					{
						DeviceCreationFlags |= D3D11_CREATE_DEVICE_DEBUG;
					}

					hr = D3D11CreateDevice(DXGIAdapter, DXGIAdapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, nullptr,
							DeviceCreationFlags, nullptr, 0, D3D11_SDK_VERSION, pfh->Ctx.DxDevice.GetInitReference(), &FeatureLevel, pfh->Ctx.DxDeviceContext.GetInitReference());
					if (hr != S_OK || !pfh->Ctx.DxDevice.IsValid())
					{
						return Error.SetMessage(TEXT("D3D11CreateDevice() failed")).SetCode(ERRCODE_INTERNAL_BAD_PARAMETER).SetSdkCode(hr);
					}
					if (FeatureLevel < D3D_FEATURE_LEVEL_9_3)
					{
						return Error.SetMessage(TEXT("Failed to create D3D11 Device with feature level 9.3 or above")).SetCode(ERRCODE_INTERNAL_BAD_PARAMETER);
					}

					hr = pfh->DxDeviceManager->ResetDevice(pfh->Ctx.DxDevice, ResetToken);
					if (hr != S_OK)
					{
						return Error.SetMessage(TEXT("DXGIDeviceManager::ResetDevice() failed")).SetCode(ERRCODE_INTERNAL_BAD_PARAMETER).SetSdkCode(hr);
					}
					// Multithread protect the newly created device as we're going to use it from decoding thread and from render thread for texture
					// sharing between decoding and rendering DX devices
					TRefCountPtr<ID3D10Multithread> DxMultithread;
					if ((hr = pfh->Ctx.DxDevice->QueryInterface(__uuidof(ID3D10Multithread), (void**)DxMultithread.GetInitReference())) == S_OK)
					{
						DxMultithread->SetMultithreadProtected(1);
					}
					bAssociatedDeviceManager = true;
				}
#endif

				// No need to set a device manager when we didn't create a device for it to manage.
				if (!bAssociatedDeviceManager || (hr = NewDecoder->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, reinterpret_cast<ULONG_PTR>(pfh->DxDeviceManager.GetReference()))) != S_OK)
				{
					// Fallback to software decoding.
					pfh->bIsSW = true;
					pfh->Ctx.Release();
					pfh->DxDeviceManager.SafeRelease();

					//return Error.SetMessage(TEXT("Failed to set the D3D manager on the decoder transform")).SetCode(ERRCODE_INTERNAL_BAD_PARAMETER).SetSdkCode(hr);
				}

				*OutPlatformHandle = pfh.Release();
			}
			else
			{
				// Not D3D aware. Fallback to software decoding.
				pfh->bIsSW = true;
				*OutPlatformHandle = pfh.Release();
			}
		}
		else
		{
			return Error.SetMessage(TEXT("Must be at least Windows 8")).SetCode(ERRCODE_INTERNAL_BAD_PARAMETER);
		}
	}
	else
	{
		return Error.SetMessage(TEXT("Bad parameters")).SetCode(ERRCODE_INTERNAL_BAD_PARAMETER);
	}
	return Error;
}

void IElectraVideoDecoderH265_DX::PlatformReleaseMFDecoderTransform(IPlatformHandle** InOutPlatformHandle)
{
	if (InOutPlatformHandle && *InOutPlatformHandle)
	{
		TUniquePtr<IElectraVideoDecoderH265_DX_Platform::FPlatformHandle> pfh(static_cast<IElectraVideoDecoderH265_DX_Platform::FPlatformHandle*>(*InOutPlatformHandle));
		*InOutPlatformHandle = nullptr;
		pfh->Close();
		pfh.Reset();
	}
}

#endif
