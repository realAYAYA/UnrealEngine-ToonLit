// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Templates/UniquePtr.h"

namespace AVEncoder
{
	class FAudioEncoder;

	class FAudioEncoderFactory
	{
	public:
		virtual ~FAudioEncoderFactory() {}
		virtual const TCHAR* GetName() const = 0;
		virtual TArray<FString> GetSupportedCodecs() const = 0;
		virtual TUniquePtr<FAudioEncoder> CreateEncoder(const FString& Codec) = 0;

		static AVENCODER_API void RegisterFactory(FAudioEncoderFactory& Factory);
		static AVENCODER_API void UnregisterFactory(FAudioEncoderFactory& Factory);
		static AVENCODER_API FAudioEncoderFactory* FindFactory(const FString& Codec);
		static AVENCODER_API const TArray<FAudioEncoderFactory*> GetAllFactories();

	private:
		static AVENCODER_API TArray<FAudioEncoderFactory*> Factories;
	};
}
