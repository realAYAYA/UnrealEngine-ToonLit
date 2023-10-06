// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioEncoderFactory.h"
#include "AVEncoder.h"

#if PLATFORM_WINDOWS
#include "Encoders/WmfAudioEncoder.h"
#endif

namespace AVEncoder
{
	bool GDefaultFactoriesRegistered = false;
	void RegisterDefaultFactories();

	void DoDefaultRegistration()
	{
		if (GDefaultFactoriesRegistered)
			return;
		RegisterDefaultFactories();
	}

	TArray<FAudioEncoderFactory*> FAudioEncoderFactory::Factories;

	void FAudioEncoderFactory::RegisterFactory(FAudioEncoderFactory& Factory)
	{
		DoDefaultRegistration();

		Factories.AddUnique(&Factory);
	}

	void FAudioEncoderFactory::UnregisterFactory(FAudioEncoderFactory& Factory)
	{
		Factories.Remove(&Factory);
	}

	FAudioEncoderFactory* FAudioEncoderFactory::FindFactory(const FString& Codec)
	{
		DoDefaultRegistration();

		for (auto&& Factory : Factories)
		{
			if (Factory->GetSupportedCodecs().Find(Codec) != INDEX_NONE)
			{
				return Factory;
			}
		}

		return nullptr;
	}

	const TArray<FAudioEncoderFactory*> FAudioEncoderFactory::GetAllFactories()
	{
		DoDefaultRegistration();

		return Factories;
	}

	void RegisterDefaultFactories()
	{
		// We need to set this at the top, otherwise RegisterFactory will call this recursively
		GDefaultFactoriesRegistered = true;

#if PLATFORM_WINDOWS
		// Generic Windows/XBox Wmf encoder
		static FWmfAudioEncoderFactory WmfAudioEncoderFactory;
		FAudioEncoderFactory::RegisterFactory(WmfAudioEncoderFactory);
#endif

		// Log all available encoders
		{
			auto CodecsInfo = [&](auto&& Factories) -> FString
			{
				FString Str;
				for (auto&& Factory : Factories)
				{
					Str += FString::Printf(TEXT(", %s(%s) "), Factory->GetName(), *FString::Join(Factory->GetSupportedCodecs(), TEXT("/")));
				}
				if (Str.IsEmpty())
				{
					return FString(TEXT("None"));
				}
				else
				{
					return Str;
				}
			};

			UE_LOG(LogAVEncoder, Log, TEXT("Available audio encoders: %s "), *CodecsInfo(FAudioEncoderFactory::GetAllFactories()));
		}
	}
}