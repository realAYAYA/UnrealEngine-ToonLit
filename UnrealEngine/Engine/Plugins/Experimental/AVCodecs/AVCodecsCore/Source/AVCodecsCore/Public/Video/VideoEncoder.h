// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AVCoder.h"
#include "Video/VideoPacket.h"
#include "Video/VideoResource.h"

/*
 * Implementation of Video Encoding domain, see TAVCoder for inheritance model
 */

enum class ERateControlMode : uint8
{
	Unknown,
	ConstQP,
	VBR,
	CBR
};

enum class EMultipassMode : uint8
{
	Unknown,
	Disabled,
	Quarter,
	Full
};

struct FVideoEncoderConfig : public FAVConfig
{
public:
	uint32 Width = 1920;
	uint32 Height = 1080;

	uint32 TargetFramerate = 60;

	int32 TargetBitrate = 0;
	int32 MaxBitrate = 0;

	int32 MinQP = -1;
	int32 MaxQP = -1;

	ERateControlMode RateControlMode = ERateControlMode::CBR;
	uint8 bFillData : 1;
	
	// TODO (Remove and derive from latency mode)
	uint32 KeyframeInterval = 0;
	EMultipassMode MultipassMode = EMultipassMode::Full;
	
	FVideoEncoderConfig(EAVPreset Preset = EAVPreset::Default)
		: FAVConfig(Preset)
		, bFillData(false)
	{
		switch (Preset)
		{
		case EAVPreset::UltraLowQuality:
			TargetBitrate = 500000;
			MaxBitrate = 1000000;

			MinQP = 10;
			MaxQP = 20;

			RateControlMode = ERateControlMode::CBR;

			break;
		case EAVPreset::LowQuality:
			TargetBitrate = 3000000;
			MaxBitrate = 4500000;

			MinQP = 20;
			MaxQP = 30;

			RateControlMode = ERateControlMode::CBR;

			break;
		case EAVPreset::Default:
			TargetBitrate = 5000000;
			MaxBitrate = 12500000;

			MinQP = 25;
			MaxQP = 40;

			RateControlMode = ERateControlMode::CBR;

			break;
		case EAVPreset::HighQuality:
			TargetBitrate = 10000000;
			MaxBitrate = 20000000;

			MinQP = 35;
			MaxQP = 50;

			RateControlMode = ERateControlMode::VBR;

			break;
		case EAVPreset::Lossless:
			TargetBitrate = 0;
			MaxBitrate = 0;

			MinQP = -1;
			MaxQP = -1;

			RateControlMode = ERateControlMode::ConstQP;

			break;
		}
	}
};

/**
 * Video encoder with a factory, that supports typesafe resource handling and configuration.
 *
 * @see TAVCodec
 */
template <typename TResource = void, typename TConfig = void>
class TVideoEncoder : public TAVCoder<TVideoEncoder, TResource, TConfig>
{
public:
	/**
	 * Wrapper encoder that transforms resource/config types for use with a differently typed child encoder.
	 *
	 * @tparam TChildResource Type of child resource.
	 * @tparam TChildConfig Type of child config.
	 */
	template <typename TChildResource, typename TChildConfig>
	class TWrapper : public TAVCoder<TVideoEncoder, TResource, TConfig>::template TWrapper<TChildResource, TChildConfig>
	{
	public:
		TWrapper(TSharedRef<TVideoEncoder<TChildResource, TChildConfig>> const& Child)
			: TAVCoder<TVideoEncoder, TResource, TConfig>::template TWrapper<TChildResource, TChildConfig>(Child)
		{
		}

		virtual FAVResult SendFrame(TSharedPtr<TResource> const& Resource, uint32 Timestamp, bool bForceKeyframe = false) override
		{
			if (!this->IsOpen())
			{
				return FAVResult(EAVResult::ErrorInvalidState, TEXT("Encoder not open"));
			}

			FAVResult Result = this->ApplyConfig();
			if (Result.IsNotSuccess())
			{
				return Result;
			}

			if (!Resource.IsValid())
			{
				return this->Child->SendFrame(nullptr, Timestamp, bForceKeyframe);
			}

			FScopeLock const Lock = Resource->LockScope();

			TSharedPtr<TChildResource> MappedResource = Resource->template PinMapping<TChildResource>();
			if (!MappedResource.IsValid() || MappedResource->Validate().IsNotSuccess())
			{
				Result = FAVExtension::TransformResource<TChildResource, TResource>(MappedResource, Resource);
				if (Result.IsNotSuccess())
				{
					MappedResource.Reset();

					return Result;
				}
			}

			return this->Child->SendFrame(MappedResource, Timestamp, bForceKeyframe);
		}

		virtual FAVResult ReceivePacket(FVideoPacket& OutPacket) override
		{
			if (!this->IsOpen())
			{
				return FAVResult(EAVResult::ErrorInvalidState, TEXT("Encoder not open"));
			}

			return this->Child->ReceivePacket(OutPacket);
		}
	};

	/**
	 * Get generic configuration values.
	 *
	 * @return The current minimal configuration.
	 */
	virtual FVideoEncoderConfig GetMinimalConfig() override
	{
		FVideoEncoderConfig MinimalConfig;
		FAVExtension::TransformConfig<FVideoEncoderConfig, TConfig>(MinimalConfig, this->GetPendingConfig());

		return MinimalConfig;
	}
	
	/**
	 * Set generic configuration values.
	 *
	 * @param MinimalConfig New minimal configuration to set.
	 */
	virtual void SetMinimalConfig(FVideoEncoderConfig const& MinimalConfig) override
	{
		FAVExtension::TransformConfig<TConfig, FVideoEncoderConfig>(this->EditPendingConfig(), MinimalConfig);
	}
};

/**
 * Video encoder with a factory, that supports typesafe resource handling.
 *
 * @see TAVCodec
 */
template <typename TResource>
class TVideoEncoder<TResource> : public TAVCoder<TVideoEncoder, TResource>
{
public:
	/**
	 * Send a frame to the underlying codec architecture.
	 *
	 * @param Resource Resource holding the frame data. An invalid resource will perform a flush (@see FlushPackets) and invalidate the underlying architecture.
	 * @param Timestamp Recorded timestamp of the frame.
	 * @param bForceKeyframe Whether the frame should be forced to be a keyframe.
	 * @return Result of the operation, @see FAVResult.
	 */
	virtual FAVResult SendFrame(TSharedPtr<TResource> const& Resource, uint32 Timestamp, bool bForceKeyframe = false) = 0;

	/**
	 * Flush remaining packets and invalidate the underlying architecture.
	 * 
	 * @return Result of the operation, @see FAVResult.
	 */
	FAVResult FlushPackets()
	{
		return SendFrame(nullptr, 0);
	}
	
	/**
	 * Flush remaining packets and invalidate the underlying architecture.
	 * 
	 * @return Result of the operation, @see FAVResult.
	 */
	FAVResult FlushAndReceivePackets(TArray<FVideoPacket>& OutPackets)
	{
		FAVResult Result = FlushPackets();
		if (Result.IsNotSuccess())
		{
			return Result;
		}

		return this->ReceivePackets(OutPackets);
	}
};

/**
 * Video encoder with a factory.
 *
 * @see TAVCodec
 */
template <>
class TVideoEncoder<> : public TAVCoder<TVideoEncoder>
{
public:
	/**
	 * Read a finished packet out of the codec.
	 *
	 * @param OutPacket Output packet if one is complete.
	 * @return Result of the operation, @see FAVResult.
	 */
	virtual FAVResult ReceivePacket(FVideoPacket& OutPacket) = 0;

	/**
	 * Read all finished packets out of the codec.
	 *
	 * @param OutPackets Output array of completed packets.
	 * @return Result of the operation, @see FAVResult.
	 */
	virtual FAVResult ReceivePackets(TArray<FVideoPacket>& OutPackets)
	{
		FAVResult Result;

		FVideoPacket Packet;
		while ((Result = ReceivePacket(Packet)).IsSuccess())
		{
			OutPackets.Add(Packet);
		}

		return Result;
	}

	/**
	 * Get generic configuration values.
	 *
	 * @return The current minimal configuration.
	 */
	virtual FVideoEncoderConfig GetMinimalConfig() = 0;
	
	/**
	 * Set generic configuration values.
	 *
	 * @param MinimalConfig New minimal configuration to set.
	 */
	virtual void SetMinimalConfig(FVideoEncoderConfig const& MinimalConfig) = 0;
};

typedef TVideoEncoder<> FVideoEncoder;

DECLARE_TYPEID(FVideoEncoder, AVCODECSCORE_API);
