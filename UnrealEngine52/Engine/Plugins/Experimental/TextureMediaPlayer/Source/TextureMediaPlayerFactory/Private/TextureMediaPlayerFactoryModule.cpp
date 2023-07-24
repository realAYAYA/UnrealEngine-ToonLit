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
		SupportedPlatforms.Add(TEXT("Windows"));
		SupportedPlatforms.Add(TEXT("PS4"));
		SupportedPlatforms.Add(TEXT("Switch"));
		SupportedPlatforms.Add(TEXT("XboxOne"));
		SupportedPlatforms.Add(TEXT("XboxOneGDK"));
		SupportedPlatforms.Add(TEXT("PS5"));
		SupportedPlatforms.Add(TEXT("XSX"));

		// supported schemes
		SupportedUriSchemes.Add(TEXT("texture"));

		// register player factory
		auto MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

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
	/** List of platforms that the media player support. */
	TArray<FString> SupportedPlatforms;

	/** List of supported URI schemes. */
	TArray<FString> SupportedUriSchemes;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FFactoryModule, TextureMediaPlayerFactory);
