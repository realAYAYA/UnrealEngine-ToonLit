// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Video/VideoEncoder.h"
#include "Video/Resources/VideoResourceCUDA.h"
#include "Video/VideoResource.h"
#include "NVENC.h"
#include "Video/Encoders/Configs/VideoEncoderConfigNVENC.h"
#include "Templates/RefCounting.h"
#include "Containers/Queue.h"

#if PLATFORM_WINDOWS
	#include "Video/Resources/D3D/VideoResourceD3D.h"
#endif // PLATFORM_WINDOWS

/**
 * This class is the core of the NVENC encoder logic.
 * It is embedded inside NVCodecEncoders using composition instead of inheritance because templates + inheritance was bit messy here where we need concrete types.
 **/
class NVENC_API FEncoderNVENC
{
private:
	void* Encoder = nullptr;
	NV_ENC_OUTPUT_PTR Buffer = nullptr;
	TQueue<FVideoPacket> Packets;

public:
	// Begin matching the TVideoEncoder interface
	virtual ~FEncoderNVENC();
	virtual bool IsOpen() const;
	virtual FAVResult Open(TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance, TFunction<void(NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS&)> SetupEncoderSessionFunc);
	virtual void Close();
	virtual FAVResult ApplyConfig(FVideoEncoderConfigNVENC const& AppliedConfig, FVideoEncoderConfigNVENC const& PendingConfig, TFunction<FAVResult()> ApplyConfigFunc);
#if PLATFORM_WINDOWS
	virtual FAVResult CreateD3D11Device(TSharedRef<FAVDevice> const& InDevice, TRefCountPtr<ID3D11Device>& OutEncoderDevice, TRefCountPtr<ID3D11DeviceContext>& OutEncoderDeviceContext);
	virtual FAVResult SendFrameD3D11(TRefCountPtr<ID3D11Device> Device, TSharedPtr<FVideoResourceD3D11> const& Resource, uint32 Timestamp, bool bForceKeyframe, TFunction<FAVResult()> ApplyConfigFunc);
#endif // PLATFORM_WINDOWS
	virtual FAVResult SendFrameCUDA(TSharedPtr<FVideoResourceCUDA> const& Resource, uint32 Timestamp, bool bForceKeyframe, TFunction<FAVResult()> ApplyConfigFunc);
	virtual FAVResult SendFrame(TSharedPtr<FVideoResource> const& Resource, uint32 Timestamp, bool bForceKeyframe, TFunction<FAVResult()> ApplyConfigFunc, TFunction<void(NV_ENC_REGISTER_RESOURCE&)> SetResourceToRegisterFunc);
	virtual FAVResult ReceivePacket(FVideoPacket& OutPacket);
	// End matching the TVideoEncoder interface

	bool IsInitialized() const;
	FAVResult SendFrame(NV_ENC_PIC_PARAMS Input);
	int GetCapability(GUID EncodeGUID, NV_ENC_CAPS CapsToQuery) const;
};

/**
 *  An AVCodecs VideoEncoder that takes a FVideoResourceCUDA.
 **/
class NVENC_API FVideoEncoderNVENCCUDA : public TVideoEncoder<FVideoResourceCUDA, FVideoEncoderConfigNVENC>
{
private:
	TUniquePtr<FEncoderNVENC> Base;

public:
	FVideoEncoderNVENCCUDA()
		: Base(MakeUnique<FEncoderNVENC>()) {}

	// Begin TVideoEncoder interface
	virtual ~FVideoEncoderNVENCCUDA() = default;
	virtual bool IsOpen() const override { return Base->IsOpen(); }
	virtual void Close() override { Base->Close(); };
	virtual FAVResult ReceivePacket(FVideoPacket& OutPacket) override { return Base->ReceivePacket(OutPacket); }

	virtual FAVResult ApplyConfig() override
	{
		return Base->ApplyConfig(AppliedConfig, GetPendingConfig(), [this]() {
			return TVideoEncoder::ApplyConfig();
		});
	}

	virtual FAVResult Open(TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance) override
	{
		return Base->Open(NewDevice, NewInstance, [this, NewDevice, NewInstance](NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS& SessionParams) {
			TVideoEncoder::Open(NewDevice, NewInstance);
			SessionParams.deviceType = NV_ENC_DEVICE_TYPE_CUDA;
			SessionParams.device = GetDevice()->GetContext<FVideoContextCUDA>()->Raw;
		});
	}

	virtual FAVResult SendFrame(TSharedPtr<FVideoResourceCUDA> const& Resource, uint32 Timestamp, bool bForceKeyframe = false) override
	{
		return Base->SendFrameCUDA(Resource, Timestamp, bForceKeyframe, [this]() {
			return ApplyConfig();
		});
	}
	// End TVideoEncoder interface
};

#if PLATFORM_WINDOWS

/**
 *  An AVCodecs VideoEncoder that takes a FVideoResourceD3D11.
 **/
class NVENC_API FVideoEncoderNVENCD3D11 : public TVideoEncoder<FVideoResourceD3D11, FVideoEncoderConfigNVENC>
{
private:
	TUniquePtr<FEncoderNVENC> Base;
	TRefCountPtr<ID3D11Device> EncoderDevice;
	TRefCountPtr<ID3D11DeviceContext> EncoderDeviceContext;

public:
	FVideoEncoderNVENCD3D11()
		: Base(MakeUnique<FEncoderNVENC>()) {}

	// Begin TVideoEncoder interface
	virtual ~FVideoEncoderNVENCD3D11() = default;
	virtual bool IsOpen() const override { return Base->IsOpen(); }
	virtual void Close() override { Base->Close(); };
	virtual FAVResult ReceivePacket(FVideoPacket& OutPacket) override { return Base->ReceivePacket(OutPacket); }

	virtual FAVResult ApplyConfig() override
	{
		return Base->ApplyConfig(AppliedConfig, GetPendingConfig(), [this]() {
			return TVideoEncoder::ApplyConfig();
		});
	};

	virtual FAVResult Open(TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance) override
	{

		// then pass device in SendFrameD3D11 and do OpenHandle

		return Base->Open(NewDevice, NewInstance, [this, NewDevice, NewInstance](NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS& SessionParams) {
			FAVResult Result = Base->CreateD3D11Device(NewDevice, EncoderDevice, EncoderDeviceContext);

			// If we failed to create device early out.
			if (Result.IsNotSuccess())
			{
				return Result;
			}

			TVideoEncoder::Open(NewDevice, NewInstance);
			SessionParams.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
			SessionParams.device = EncoderDevice;
			// SessionParams.device = GetDevice()->GetContext<FVideoContextD3D11>()->Device;
			return Result;
		});
	}

	virtual FAVResult SendFrame(TSharedPtr<FVideoResourceD3D11> const& Resource, uint32 Timestamp, bool bForceKeyframe = false)
	{
		return Base->SendFrameD3D11(EncoderDevice, Resource, Timestamp, bForceKeyframe, [this]() {
			return ApplyConfig();
		});
	}
	// End TVideoEncoder interface
};
#endif // PLATFORM_WINDOWS