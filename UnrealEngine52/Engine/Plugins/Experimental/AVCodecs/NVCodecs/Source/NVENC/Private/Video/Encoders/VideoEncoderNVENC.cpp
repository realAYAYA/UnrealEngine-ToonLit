// Copyright Epic Games, Inc. All Rights Reserved.

#include "Video/Encoders/VideoEncoderNVENC.h"

FEncoderNVENC::~FEncoderNVENC()
{
	Close();
}

bool FEncoderNVENC::IsOpen() const
{
	return Encoder != nullptr;
}

FAVResult FEncoderNVENC::Open(TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance, TFunction<void(NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS&)> SetupEncoderSessionFunc)
{
	Close();

	NV_ENC_STRUCT(NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS, SessionParams);
	SessionParams.apiVersion = NVENCAPI_VERSION;

	SetupEncoderSessionFunc(SessionParams);

	NVENCSTATUS const Result = FAPI::Get<FNVENC>().nvEncOpenEncodeSessionEx(&const_cast<NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS&>(SessionParams), &Encoder);
	if (Result != NV_ENC_SUCCESS)
	{
		Close();

		return FAVResult(EAVResult::ErrorCreating, TEXT("Failed to create encoder"), TEXT("NVENC"), Result);
	}

	return EAVResult::Success;
}

void FEncoderNVENC::Close()
{
	if (IsOpen())
	{
		if (Buffer != nullptr)
		{
			NVENCSTATUS const Result = FAPI::Get<FNVENC>().nvEncDestroyBitstreamBuffer(Encoder, Buffer);
			if (Result != NV_ENC_SUCCESS)
			{
				FAVResult::Log(EAVResult::ErrorDestroying, TEXT("Failed to destroy output buffer"), TEXT("NVENC"), Result);
			}

			Buffer = nullptr;
		}

		if (Encoder != nullptr)
		{
			NVENCSTATUS const Result = FAPI::Get<FNVENC>().nvEncDestroyEncoder(Encoder);
			if (Result != NV_ENC_SUCCESS)
			{
				FAVResult::Log(EAVResult::ErrorDestroying, TEXT("Failed to destroy encoder"), TEXT("NVENC"), Result);
			}

			Encoder = nullptr;
		}
	}
}

bool FEncoderNVENC::IsInitialized() const
{
	return IsOpen() && Buffer != nullptr;
}

FAVResult FEncoderNVENC::ApplyConfig(FVideoEncoderConfigNVENC const& AppliedConfig, FVideoEncoderConfigNVENC const& PendingConfig, TFunction<FAVResult()> ApplyConfigFunc)
{
	if (IsOpen())
	{
		// FVideoEncoderConfigNVENC const& PendingConfig = GetPendingConfig();
		if (AppliedConfig != PendingConfig)
		{
			if (IsInitialized())
			{
				// Can be reconfigured? See https://docs.nvidia.com/video-technologies/video-codec-sdk/nvenc-video-encoder-api-prog-guide/#reconfigure-api
				if (AppliedConfig.maxEncodeWidth == PendingConfig.maxEncodeWidth
					&& AppliedConfig.maxEncodeHeight == PendingConfig.maxEncodeHeight
					&& AppliedConfig.enablePTD == PendingConfig.enablePTD
					&& AppliedConfig.enableEncodeAsync == PendingConfig.enableEncodeAsync
					&& AppliedConfig.encodeConfig->gopLength == PendingConfig.encodeConfig->gopLength
					&& AppliedConfig.encodeConfig->frameIntervalP == PendingConfig.encodeConfig->frameIntervalP
					&& AppliedConfig.encodeConfig->encodeCodecConfig.h264Config.idrPeriod == PendingConfig.encodeConfig->encodeCodecConfig.h264Config.idrPeriod)
				{
					NV_ENC_STRUCT(NV_ENC_RECONFIGURE_PARAMS, ReconfigureParams);
					FMemory::Memcpy(&ReconfigureParams.reInitEncodeParams, &static_cast<NV_ENC_INITIALIZE_PARAMS const&>(PendingConfig), sizeof(NV_ENC_INITIALIZE_PARAMS));
					ReconfigureParams.forceIDR = AppliedConfig.encodeWidth != PendingConfig.encodeWidth || AppliedConfig.encodeHeight != PendingConfig.encodeHeight;

					NVENCSTATUS const Result = FAPI::Get<FNVENC>().nvEncReconfigureEncoder(Encoder, &ReconfigureParams);
					if (Result != NV_ENC_SUCCESS)
					{
						return FAVResult(EAVResult::Error, TEXT("Failed to update encoder configuration"), TEXT("NVENC"), Result);
					}
				}
				else
				{
					// TODO: Destroy and recreate with original session
					unimplemented();
				}
			}

			if (!IsInitialized())
			{
				NVENCSTATUS Result = FAPI::Get<FNVENC>().nvEncInitializeEncoder(Encoder, &const_cast<FVideoEncoderConfigNVENC&>(PendingConfig));
				if (Result != NV_ENC_SUCCESS)
				{
					return FAVResult(EAVResult::ErrorCreating, TEXT("Failed to initialize encoder"), TEXT("NVENC"), Result);
				}

				NV_ENC_STRUCT(NV_ENC_CREATE_BITSTREAM_BUFFER, CreateBuffer);

				Result = FAPI::Get<FNVENC>().nvEncCreateBitstreamBuffer(Encoder, &CreateBuffer);
				if (Result != NV_ENC_SUCCESS)
				{
					return FAVResult(EAVResult::ErrorCreating, TEXT("Failed to create output buffer"), TEXT("NVENC"), Result);
				}

				Buffer = CreateBuffer.bitstreamBuffer;
			}
		}

		return ApplyConfigFunc();
	}

	return FAVResult(EAVResult::ErrorInvalidState, TEXT("Encoder not open"), TEXT("NVENC"));
}

int FEncoderNVENC::GetCapability(GUID EncodeGUID, NV_ENC_CAPS CapsToQuery) const
{
	if (IsOpen())
	{
		int CapsValue = 0;

		NV_ENC_STRUCT(NV_ENC_CAPS_PARAM, CapsParam);
		CapsParam.capsToQuery = CapsToQuery;

		NVENCSTATUS const Result = FAPI::Get<FNVENC>().nvEncGetEncodeCaps(Encoder, EncodeGUID, &CapsParam, &CapsValue);
		if (Result != NV_ENC_SUCCESS)
		{
			FAVResult::Log(EAVResult::Warning, FString::Printf(TEXT("Failed to query for capability %d"), CapsToQuery), TEXT("NVENC"), Result);

			return 0;
		}

		return CapsValue;
	}

	return FAVResult(EAVResult::ErrorInvalidState, TEXT("Encoder not open"), TEXT("NVENC"));
}

FAVResult FEncoderNVENC::SendFrame(TSharedPtr<FVideoResource> const& Resource, uint32 Timestamp, bool bForceKeyframe, TFunction<FAVResult()> ApplyConfigFunc, TFunction<void(NV_ENC_REGISTER_RESOURCE&)> SetResourceToRegisterFunc)
{
	if (IsOpen())
	{
		FAVResult AVResult = ApplyConfigFunc();
		if (AVResult.IsNotSuccess())
		{
			return AVResult;
		}

		NV_ENC_STRUCT(NV_ENC_MAP_INPUT_RESOURCE, MapResource);
		NV_ENC_STRUCT(NV_ENC_REGISTER_RESOURCE, RegisterResource);
		NV_ENC_STRUCT(NV_ENC_PIC_PARAMS, Picture);

		NVENCSTATUS Result;
		if (Resource.IsValid())
		{
			TAVResult<NV_ENC_BUFFER_FORMAT> const ConvertedPixelFormat = FVideoEncoderConfigNVENC::ConvertFormat(Resource->GetFormat());
			if (ConvertedPixelFormat.IsNotSuccess())
			{
				return ConvertedPixelFormat;
			}

			// START DEBUG
#if DEBUG_DUMP_TO_DISK
			{
				TArray64<uint8> OutData;

				Resource->ReadData(OutData);

				FString SaveName = FString::Printf(TEXT("%s/DumpInput/image%05d.%s"), *FPaths::ProjectSavedDir(), Timestamp, ConvertedPixelFormat == NV_ENC_BUFFER_FORMAT_NV12 ? TEXT("nv12") : TEXT("p016"));

				FFileHelper::SaveArrayToFile(OutData, *SaveName);
			}
#endif
			// END DEBUG

			SetResourceToRegisterFunc(RegisterResource);
			RegisterResource.width = Resource->GetWidth();
			RegisterResource.height = Resource->GetHeight();
			RegisterResource.pitch = Resource->GetStride();
			RegisterResource.bufferFormat = ConvertedPixelFormat;
			RegisterResource.bufferUsage = NV_ENC_INPUT_IMAGE;

			Result = FAPI::Get<FNVENC>().nvEncRegisterResource(Encoder, &RegisterResource);
			if (Result != NV_ENC_SUCCESS)
			{
				return FAVResult(EAVResult::ErrorMapping, TEXT("Failed to register frame resource"), TEXT("NVENC"), Result);
			}

			MapResource.registeredResource = RegisterResource.registeredResource;

			Result = FAPI::Get<FNVENC>().nvEncMapInputResource(Encoder, &MapResource);
			if (Result != NV_ENC_SUCCESS)
			{
				return FAVResult(EAVResult::ErrorMapping, TEXT("Failed to map frame resource"), TEXT("NVENC"), Result);
			}

			Picture.inputWidth = RegisterResource.width;
			Picture.inputHeight = RegisterResource.height;
			Picture.inputPitch = RegisterResource.pitch;
			Picture.inputBuffer = MapResource.mappedResource;
			Picture.bufferFmt = MapResource.mappedBufferFmt;
			Picture.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
			Picture.inputTimeStamp = Timestamp;

			if (bForceKeyframe)
			{
				Picture.encodePicFlags |= NV_ENC_PIC_FLAG_FORCEIDR;
			}
		}
		else
		{
			Picture.encodePicFlags = NV_ENC_PIC_FLAG_EOS;
		}

		if (Resource.IsValid())
		{
			Resource->Lock();
		}

		AVResult = SendFrame(Picture);

		if (Resource.IsValid())
		{
			Resource->Unlock();
		}

		if (MapResource.mappedResource != nullptr)
		{
			Result = FAPI::Get<FNVENC>().nvEncUnmapInputResource(Encoder, MapResource.mappedResource);
			if (Result != NV_ENC_SUCCESS)
			{
				FAVResult const UnmapResult = FAVResult(EAVResult::ErrorUnmapping, TEXT("Failed to unmap frame resource"), TEXT("NVENC"), Result);
				if (UnmapResult < AVResult)
				{
					AVResult = UnmapResult;
				}
			}
		}

		if (RegisterResource.registeredResource != nullptr)
		{
			Result = FAPI::Get<FNVENC>().nvEncUnregisterResource(Encoder, RegisterResource.registeredResource);
			if (Result != NV_ENC_SUCCESS)
			{
				FAVResult const UnmapResult = FAVResult(EAVResult::ErrorUnmapping, TEXT("Failed to unregister frame resource"), TEXT("NVENC"), Result);
				if (UnmapResult < AVResult)
				{
					AVResult = UnmapResult;
				}
			}
		}

		return AVResult;
	}

	return FAVResult(EAVResult::ErrorInvalidState, TEXT("Encoder not open"), TEXT("NVENC"));
}

FAVResult FEncoderNVENC::SendFrame(NV_ENC_PIC_PARAMS Input)
{
	if (IsOpen())
	{
		if (Input.encodePicFlags & NV_ENC_PIC_FLAG_EOS)
		{
			NVENCSTATUS Result = FAPI::Get<FNVENC>().nvEncEncodePicture(Encoder, &Input);
			if (Result != NV_ENC_SUCCESS)
			{
				return FAVResult(EAVResult::Error, TEXT("Error encoding end-of-stream picture"), TEXT("NVENC"), Result);
			}

			return EAVResult::Success;
		}
		else
		{
			if (Input.outputBitstream == nullptr)
			{
				Input.outputBitstream = Buffer;
			}

			NVENCSTATUS Result = FAPI::Get<FNVENC>().nvEncEncodePicture(Encoder, &Input);
			if (Result != NV_ENC_SUCCESS)
			{
				return FAVResult(EAVResult::Error, TEXT("Error encoding picture"), TEXT("NVENC"), Result);
			}

			NV_ENC_STRUCT(NV_ENC_LOCK_BITSTREAM, BitstreamLock);
			BitstreamLock.outputBitstream = Buffer;

			Result = FAPI::Get<FNVENC>().nvEncLockBitstream(Encoder, &BitstreamLock);
			if (Result != NV_ENC_SUCCESS)
			{
				return FAVResult(EAVResult::ErrorLocking, TEXT("Failed to lock output bitstream"), TEXT("NVENC"), Result);
			}

			TSharedPtr<uint8> const CopiedData = MakeShareable(new uint8[BitstreamLock.bitstreamSizeInBytes]);
			FMemory::BigBlockMemcpy(CopiedData.Get(), BitstreamLock.bitstreamBufferPtr, BitstreamLock.bitstreamSizeInBytes);

			Packets.Enqueue(
				FVideoPacket(
					CopiedData,
					BitstreamLock.bitstreamSizeInBytes,
					BitstreamLock.outputTimeStamp,
					BitstreamLock.frameIdx,
					BitstreamLock.frameAvgQP,
					(BitstreamLock.pictureType & NV_ENC_PIC_TYPE_IDR) != 0));

			Result = FAPI::Get<FNVENC>().nvEncUnlockBitstream(Encoder, BitstreamLock.outputBitstream);
			if (Result != NV_ENC_SUCCESS)
			{
				return FAVResult(EAVResult::ErrorUnlocking, TEXT("Failed to unlock output buffer"), TEXT("NVENC"));
			}

			return EAVResult::Success;
		}
	}

	return FAVResult(EAVResult::ErrorInvalidState, TEXT("Encoder not open"), TEXT("NVENC"));
}

#if PLATFORM_WINDOWS
FAVResult FEncoderNVENC::SendFrameD3D11(TRefCountPtr<ID3D11Device> Device, TSharedPtr<FVideoResourceD3D11> const& Resource, uint32 Timestamp, bool bForceKeyframe, TFunction<FAVResult()> ApplyConfigFunc)
{
	TRefCountPtr<ID3D11Texture2D>& Tex = const_cast<TRefCountPtr<ID3D11Texture2D>&>(Resource->GetRaw());

	// Very important: We create a new device for NVENC - we must share out D3D11 texture with the new device by "Opening" it to that device.
	HRESULT Result = Device->OpenSharedResource(Resource->GetSharedHandle(), __uuidof(ID3D11Texture2D), (void**)(Tex.GetInitReference()));
	if (Result != S_OK)
	{
		return FAVResult(EAVResult::Fatal, TEXT("Failed to open shared handle."), TEXT("NVENC"), Result);
	}

	return SendFrame(Resource, Timestamp, bForceKeyframe, ApplyConfigFunc, [Resource](NV_ENC_REGISTER_RESOURCE& RegisterResource) {
		RegisterResource.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
		RegisterResource.resourceToRegister = Resource->GetRaw();
	});
}

/**
 * @returns The FString version of HRESULT error message so we can log it.
 **/
const FString AVGetComErrorDescription(HRESULT Res)
{
	const uint32 BufSize = 4096;
	WIDECHAR Buffer[4096];
	if (::FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM,
			nullptr,
			Res,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
			Buffer,
			sizeof(Buffer) / sizeof(*Buffer),
			nullptr))
	{
		return Buffer;
	}
	else
	{
		return TEXT("[cannot find error description]");
	}
}

/**
 * You may be asking yourself, why on Earth would we need to create a seperate D3D11 device when using NVENC?
 * Well, it basically comes down to the D3D11 device created by Unreal Engine is not compatible with NVENC due to sdk version changes (we think?)
 * and Unreal Engine is in no hurry to bump D3D11 version - so we create out own device. This works only because
 * the D3D11 textures we are using are created as shared resources and accessed through shared handles.
 * Without this chicainery the D3D11 device will be ejected and Unreal Engine and NVENC will crash.
 */
FAVResult FEncoderNVENC::CreateD3D11Device(TSharedRef<FAVDevice> const& InDevice, TRefCountPtr<ID3D11Device>& OutEncoderDevice, TRefCountPtr<ID3D11DeviceContext>& OutEncoderDeviceContext)
{
	TRefCountPtr<IDXGIDevice> DXGIDevice;
	TRefCountPtr<IDXGIAdapter> Adapter;

	HRESULT Result = InDevice->GetContext<FVideoContextD3D11>()->Device->QueryInterface(__uuidof(IDXGIDevice), (void**)DXGIDevice.GetInitReference());
	if (Result != S_OK)
	{
		return FAVResult(EAVResult::Fatal, FString::Printf(TEXT("ID3D11Device::QueryInterface() failed 0x%X - %s."), Result, *AVGetComErrorDescription(Result)), TEXT("D3D11"));
	}
	else if ((Result = DXGIDevice->GetAdapter(Adapter.GetInitReference())) != S_OK)
	{
		return FAVResult(EAVResult::Fatal, FString::Printf(TEXT("DXGIDevice::GetAdapter() failed 0x%X - %s."), Result, *AVGetComErrorDescription(Result)), TEXT("D3D11"));
	}

	uint32 DeviceFlags = 0;
	D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0;
	D3D_FEATURE_LEVEL ActualFeatureLevel;

	if ((Result = D3D11CreateDevice(
			 Adapter,
			 D3D_DRIVER_TYPE_UNKNOWN,
			 NULL,
			 DeviceFlags,
			 &FeatureLevel,
			 1,
			 D3D11_SDK_VERSION,
			 OutEncoderDevice.GetInitReference(),
			 &ActualFeatureLevel,
			 OutEncoderDeviceContext.GetInitReference()))
		!= S_OK)
	{
		return FAVResult(EAVResult::Fatal, FString::Printf(TEXT("D3D11CreateDevice() failed 0x%X - %s."), Result, *AVGetComErrorDescription(Result)), TEXT("D3D11"));
	}

	return FAVResult(EAVResult::Success, TEXT("Created D3D11 device for NVENC."), TEXT("D3D11"));
}

#endif // PLATFORM_WINDOWS

FAVResult FEncoderNVENC::SendFrameCUDA(TSharedPtr<FVideoResourceCUDA> const& Resource, uint32 Timestamp, bool bForceKeyframe, TFunction<FAVResult()> ApplyConfigFunc)
{
	return SendFrame(Resource, Timestamp, bForceKeyframe, ApplyConfigFunc, [Resource](NV_ENC_REGISTER_RESOURCE& RegisterResource) {
		RegisterResource.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_CUDAARRAY;
		RegisterResource.resourceToRegister = Resource->GetRaw();
	});
}

FAVResult FEncoderNVENC::ReceivePacket(FVideoPacket& OutPacket)
{
	if (IsOpen())
	{
		if (Packets.Dequeue(OutPacket))
		{
			return EAVResult::Success;
		}

		return EAVResult::PendingInput;
	}

	return FAVResult(EAVResult::ErrorInvalidState, TEXT("Encoder not open"), TEXT("NVENC"));
}
