// Copyright Epic Games, Inc. All Rights Reserved.

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "IMediaOptions.h"
#include "IMediaPlayerFactory.h"
#include "IMediaModule.h"
#include "Internationalization/Internationalization.h"
#include "ITextureMediaPlayerModule.h"
#include "Logging/LogMacros.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/NameTypes.h"

#define LOCTEXT_NAMESPACE "FTextureMediaPlayerFactoryModule"

class FFactoryModule
	: public IMediaPlayerFactory
	, public IModuleInterface
{
public:
	FFactoryModule() { }

public:
	// IMediaPlayerFactory interface

	bool CanPlayUrl(const FString& Url, const IMediaOptions* Options, TArray<FText>* OutWarnings, TArray<FText>* OutErrors) const override
	{
		FString Scheme;
		FString Location;

		// check scheme
		if (!Url.Split(TEXT("://"), &Scheme, &Location, ESearchCase::CaseSensitive))
		{
			if (OutErrors != nullptr)
			{
				OutErrors->Add(LOCTEXT("NoSchemeFound", "No URI scheme found"));
			}

			return false;
		}

		if (!SupportedUriSchemes.Contains(Scheme))
		{
			if (OutErrors != nullptr)
			{
				OutErrors->Add(FText::Format(LOCTEXT("SchemeNotSupported", "The URI scheme '{0}' is not supported"), FText::FromString(Scheme)));
			}

			return false;
		}

		return true;
	}

	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) override
	{
		auto TextureMediaPlayerModule = FModuleManager::LoadModulePtr<ITextureMediaPlayerModule>("TextureMediaPlayer");
		return (TextureMediaPlayerModule != nullptr) ? TextureMediaPlayerModule->CreatePlayer(EventSink) : nullptr;
	}

	FText GetDisplayName() const override
	{
		return LOCTEXT("MediaPlayerDisplayName", "Texture Media Player");
	}

	virtual FName GetPlayerName() const override
	{
		static FName PlayerName(TEXT("TextureMediaPlayer"));
		return PlayerName;
	}

	virtual FGuid GetPlayerPluginGUID() const override
	{
		static FGuid PlayerPluginGUID(0x16137521, 0x26364e57, 0xa5b66211, 0x821b9819);
		return PlayerPluginGUID;
	}

	const TArray<FString>& GetSupportedPlatforms() const override
	{
		return SupportedPlatforms;
	}

	bool SupportsFeature(EMediaFeature Feature) const override
	{
		return (Feature == EMediaFeature::VideoSamples);
	}

public:

	// IModuleInterface interface

	void StartupModule() override
	{
		// We make sure that the correct modules are loaded.
		FModuleManager::Get().LoadModule(TEXT("TextureMediaPlayer"));

		// supported platforms
		auto MediaModule = FModuleManager::GetModulePtr<IMediaModule>("Media");
		if (MediaModule != nullptr)
		{		
			AddSupportedPlatform(FGuid(0xd1d5f296, 0xff834a87, 0xb20faaa9, 0xd6b8e9a6));
			AddSupportedPlatform(FGuid(0xb80decd6, 0x997a4b3f, 0x92063970, 0xe572c0db));
			AddSupportedPlatform(FGuid(0x30ebce04, 0x2c8247bd, 0xaf873017, 0x5a27ed45));
			AddSupportedPlatform(FGuid(0xb596ce6f, 0xd8324a9c, 0x84e9f880, 0x21322535));
			AddSupportedPlatform(FGuid(0x941259d5, 0x0a2746aa, 0xadc0ba84, 0x4790ad8a));
			AddSupportedPlatform(FGuid(0xb67dd9c6, 0x77694fd5, 0xb2b0c8bf, 0xe0c1c673));
			AddSupportedPlatform(FGuid(0xccf05903, 0x822b47e1, 0xb2236a28, 0xdfd78817));
		}

		// supported schemes
		SupportedUriSchemes.Add(TEXT("texture"));

		// register player factory
		if (MediaModule != nullptr)
		{
			MediaModule->RegisterPlayerFactory(*this);
		}
	}

	void ShutdownModule() override
	{
		// unregister player factory
		auto MediaModule = FModuleManager::GetModulePtr<IMediaModule>("Media");

		if (MediaModule != nullptr)
		{
			MediaModule->UnregisterPlayerFactory(*this);
		}
	}

private:
	void AddSupportedPlatform(const FGuid& PlatformGuid)
	{
		auto MediaModule = FModuleManager::GetModulePtr<IMediaModule>("Media");
		check(MediaModule);

		FName PlatformName = MediaModule->GetPlatformName(PlatformGuid);
		if (!PlatformName.IsNone())
		{
			SupportedPlatforms.Add(PlatformName.ToString());
		}
	}

	/** List of platforms that the media player support. */
	TArray<FString> SupportedPlatforms;

	/** List of supported URI schemes. */
	TArray<FString> SupportedUriSchemes;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FFactoryModule, TextureMediaPlayerFactory);
