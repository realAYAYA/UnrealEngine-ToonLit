// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Video/VideoDecoder.h"
#include "Video/Resources/VideoResourceRHI.h"
#include "Video/Resources/VideoResourceVulkan.h"

#if PLATFORM_WINDOWS
#include "Video/Resources/Windows/VideoResourceD3D.h"
#endif

template <typename TConfig>
class TVideoDecoderRHI : public TVideoDecoder<FVideoResourceRHI, TConfig>
{
public:
	template <typename TChildConfig>
	static FVideoDecoder::TFactory<FVideoResourceRHI, TConfig>& Register()
	{
		return FVideoDecoder::Register<TVideoDecoderRHI<TConfig>, FVideoResourceRHI, TConfig>();
	}
	
	TSharedPtr<TVideoDecoder<FVideoResourceRHI, TConfig>> Child;

	TVideoDecoderRHI() = default;

	virtual bool IsOpen() const override
	{
		return Child.IsValid() && Child->IsOpen();
	}

	virtual FAVResult Open(TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance) override
	{
		if (this->IsOpen())
		{
			return FAVResult(EAVResult::ErrorInvalidState, TEXT("Decoder already open"), TEXT("RHI"));
		}

		FAVResult const Result = TVideoDecoder<FVideoResourceRHI, TConfig>::Open(NewDevice, NewInstance);
		if (Result.IsNotSuccess())
		{
			return Result;
		}

		switch (GDynamicRHI->GetInterfaceType())
		{
		case ERHIInterfaceType::Vulkan:
			Child = FVideoDecoder::Wrap<FVideoResourceRHI, TConfig>(FVideoDecoder::Create<FVideoResourceVulkan, TConfig>(NewDevice, NewInstance));
		
			break;
#if PLATFORM_WINDOWS
		case ERHIInterfaceType::D3D11:
			Child = FVideoDecoder::Wrap<FVideoResourceRHI, TConfig>(FVideoDecoder::Create<FVideoResourceD3D11, TConfig>(NewDevice, NewInstance));
		
			break;
		case ERHIInterfaceType::D3D12:
			Child = FVideoDecoder::Wrap<FVideoResourceRHI, TConfig>(FVideoDecoder::Create<FVideoResourceD3D12, TConfig>(NewDevice, NewInstance));
		
			break;
#endif
		default:
			return FAVResult(EAVResult::ErrorCreating, FString::Printf(TEXT("RHI type %d not supported"), GDynamicRHI->GetInterfaceType()), TEXT("RHI"));
		}

		if (!IsOpen())
		{
			return FAVResult(EAVResult::ErrorCreating, TEXT("Failed to create RHI child platform decoder"), TEXT("RHI"));
		}

		return EAVResult::Success;
	}
	
	virtual void Close() override
	{
		if (this->IsOpen())
		{
			Child.Reset();
		}
	}

	virtual FAVResult ApplyConfig() override
	{
		if (!this->IsOpen())
		{
			return FAVResult(EAVResult::ErrorInvalidState, TEXT("Decoder not open"), TEXT("RHI"));
		}

		FAVResult const Result = TVideoDecoder<FVideoResourceRHI, TConfig>::ApplyConfig();
		if (Result.IsNotSuccess())
		{
			return Result;
		}

		Child->SetPendingConfig(this->GetAppliedConfig());

		return EAVResult::Success;
	}

	virtual FAVResult SendPacket(FVideoPacket const& Packet) override
	{
		if (!this->IsOpen())
		{
			return FAVResult(EAVResult::ErrorInvalidState, TEXT("Decoder not open"), TEXT("RHI"));
		}

		FAVResult const Result = this->ApplyConfig();
		if (Result.IsNotSuccess())
		{
			return Result;
		}

		return Child->SendPacket(Packet);
	}

	virtual FAVResult ReceiveFrame(TResolvableVideoResource<FVideoResourceRHI>& InOutResource) override
	{
		if (!this->IsOpen())
		{
			return FAVResult(EAVResult::ErrorInvalidState, TEXT("Decoder not open"), TEXT("RHI"));
		}

		return Child->ReceiveFrame(InOutResource);
	}
};
