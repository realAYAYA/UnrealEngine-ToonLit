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

	class AVENCODER_API FAudioEncoderFactory
	{
	public:
		virtual ~FAudioEncoderFactory() {}
		virtual const TCHAR* GetName() const = 0;
		virtual TArray<FString> GetSupportedCodecs() const = 0;
		virtual TUniquePtr<FAudioEncoder> CreateEncoder(const FString& Codec) = 0;

		static void RegisterFactory(FAudioEncoderFactory& Factory);
		static void UnregisterFactory(FAudioEncoderFactory& Factory);
		static FAudioEncoderFactory* FindFactory(const FString& Codec);
		static const TArray<FAudioEncoderFactory*> GetAllFactories();

	private:
		static TArray<FAudioEncoderFactory*> Factories;
	};
}
