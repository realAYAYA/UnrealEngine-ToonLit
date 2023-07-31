// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#if PLATFORM_WINDOWS

#include "AudioEncoderFactory.h"

namespace AVEncoder
{
	class FWmfAudioEncoderFactory : public FAudioEncoderFactory
	{
	public:
		FWmfAudioEncoderFactory();
		~FWmfAudioEncoderFactory() override;
		const TCHAR* GetName() const override;
		TArray<FString> GetSupportedCodecs() const override;
		TUniquePtr<FAudioEncoder> CreateEncoder(const FString& Codec) override;
	private:
	};
}

#endif //PLATFORM_WINDOWS

