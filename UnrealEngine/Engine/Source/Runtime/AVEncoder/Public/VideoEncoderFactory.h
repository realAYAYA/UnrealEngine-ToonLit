// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/CriticalSection.h"
#include "HAL/Platform.h"
#include "HAL/ThreadSafeBool.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "VideoCommon.h"
#include "VideoEncoder.h"

class FThreadSafeCounter;

namespace AVEncoder
{
	class FVideoEncoderInput;

	class AVENCODER_API FVideoEncoderFactory
	{
	public:
		static FVideoEncoderFactory& Get();
		static void Shutdown();
		static void Debug_SetDontRegisterDefaultCodecs();

		bool IsSetup() const { return bWasSetup; }

		// --- encoder registry

		// the callback type used to create a registered encoder
		using CreateEncoderCallback = TFunction<TUniquePtr<FVideoEncoder>()>;

		// register an encoder so that it can be iterated and created
		void Register(const FVideoEncoderInfo& InInfo, const CreateEncoderCallback& InCreateEncoder);

		// get a list of registered encoders
		const TArray<FVideoEncoderInfo>& GetAvailable() const { return AvailableEncoders; }
		bool GetInfo(uint32 InID, FVideoEncoderInfo& OutInfo) const;
		bool HasEncoderForCodec(ECodecType CodecType) const;

		// --- encoder creation

		TUniquePtr<FVideoEncoder> Create(uint32 InID, const FVideoEncoder::FLayerConfig& config);
		TUniquePtr<FVideoEncoder> Create(uint32 InID, TSharedPtr<FVideoEncoderInput> InInput, const FVideoEncoder::FLayerConfig& config);

	private:
		FVideoEncoderFactory() = default;
		~FVideoEncoderFactory() = default;
		FVideoEncoderFactory(const FVideoEncoderFactory&) = delete;
		FVideoEncoderFactory& operator=(const FVideoEncoderFactory&) = delete;

		void RegisterDefaultCodecs();

		static FCriticalSection			ProtectSingleton;
		static FVideoEncoderFactory		Singleton;
		static FThreadSafeCounter		NextID;
		bool							bDebugDontRegisterDefaultCodecs = false;
		FThreadSafeBool					bWasSetup;
		TArray<FVideoEncoderInfo>		AvailableEncoders;
		TArray<CreateEncoderCallback>	CreateEncoders;

	};
    
} /* namespace AVEncoder */