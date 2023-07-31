// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "Templates/Function.h"
#include "VideoDecoder.h"

#include <HAL/ThreadSafeBool.h>

class FThreadSafeCounter;

namespace AVEncoder
{
	struct FVideoDecoderInfo;

	class AVENCODER_API FVideoDecoderFactory
	{
	public:
		static FVideoDecoderFactory& Get();
		static void Debug_SetDontRegisterDefaultCodecs();

		// --- decoder registry

		// the callback type used to create a registered decoder
		using CreateDecoderCallback = TFunction<FVideoDecoder*()>;

		// register a decoder so that it can be iterated and created
		void Register(const FVideoDecoderInfo& InInfo, const CreateDecoderCallback& InCreateDecoder);

		// get a list of registered decoders
		const TArray<FVideoDecoderInfo>& GetAvailable() const { return AvailableDecoders; }
		bool GetInfo(uint32 InID, FVideoDecoderInfo& OutInfo) const;

		// --- decoder creation
		// create a decoder instance. If not nullptr it must be destroyed through invoking its Shutdown() method!
		FVideoDecoder* Create(uint32 InID, const FVideoDecoder::FInit& InInit);

	private:
		FVideoDecoderFactory();
		~FVideoDecoderFactory();
		FVideoDecoderFactory(const FVideoDecoderFactory&) = delete;
		FVideoDecoderFactory& operator=(const FVideoDecoderFactory&) = delete;

		void RegisterDefaultCodecs();

		static FThreadSafeCounter		NextID;
		static bool						bDebugDontRegisterDefaultCodecs;

		TArray<FVideoDecoderInfo>		AvailableDecoders;
		TArray<CreateDecoderCallback>	CreateDecoders;
	};

} /* namespace AVEncoder */