// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "IMediaOptions.h"
#include "IMediaOptions.h"
#include "Internationalization/Text.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Variant.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "UObject/UObjectGlobals.h"

#include "MediaSource.generated.h"

class FVariant;
class UMediaSource;
struct FFrame;

/** Delegate for creating a media source from a string. */
DECLARE_DELEGATE_RetVal_TwoParams(UMediaSource*, FMediaSourceSpawnDelegate, const FString&, UObject*);

/** Cache settings to pass to the player. */
USTRUCT(BlueprintType)
struct FMediaSourceCacheSettings
{
	GENERATED_USTRUCT_BODY()

	/**
	 * Override the default cache settings.
	 * Currently only the ImgMedia player supports these settings.
	 */
	UPROPERTY(EditAnywhere, Category = "Media Cache")
	bool bOverride = false;

	/**
	 * The cache will fill up with frames that are up to this time from the current time.
	 * E.g. if this is 0.2, and we are at time index 5 seconds,
	 * then we will fill the cache with frames between 5 seconds and 5.2 seconds.
	 */
	UPROPERTY(EditAnywhere, Category = "Media Cache")
	float TimeToLookAhead = 0.2f;
};

/**
 * Abstract base class for media sources.
 *
 * Media sources describe the location and/or settings of media objects that can
 * be played in a media player, such as a video file on disk, a video stream on
 * the internet, or a web cam attached to or built into the target device. The
 * location is encoded as a media URL string, whose URI scheme and optional file
 * extension will be used to locate a suitable media player.
 */
UCLASS(Abstract, editinlinenew, BlueprintType, hidecategories=(Object))
class MEDIAASSETS_API UMediaSource
	: public UObject
	, public IMediaOptions
{
	GENERATED_BODY()

public:

	/**
	 * Get the media source's URL string (must be implemented in child classes).
	 *
	 * @return The media URL.
	 * @see GetProxies
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaSource")
	virtual FString GetUrl() const PURE_VIRTUAL(UMediaSource::GetUrl, return FString(););

	/**
	 * Validate the media source settings (must be implemented in child classes).
	 *
	 * @return true if validation passed, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaSource")
	virtual bool Validate() const PURE_VIRTUAL(UMediaSource::Validate, return false;);

	/**
	 * Call this to set cache settings to pass to the player.
	 */
	void SetCacheSettings(const FMediaSourceCacheSettings& Settings);

	/**
	 * Call this to register a callback when someone calls SpawnMediaSourceForString.
	 * This lets you spawn a media source if the file extension matches what you want.
	 * 
	 * @param Extension		File extension to match. This is case insensitive.
	 * @param InDelegate	This will get called if the Url passed into GetMediaSourceForUrl
	 *						matches Extension.
	 */
	static void RegisterSpawnFromFileExtension(const FString& Extension, FMediaSourceSpawnDelegate InDelegate);
	
	/**
	 * Call this to unregister a callback set with RegisterSpawnFromFileExtension.
	 *
	 * @param Extension		File extension that the callack was registered with.
	 */
	static void UnregisterSpawnFromFileExtension(const FString& Extension);

	/**
	 * Call this to try and create a media source appropriate for the media.
	 *
	 * @param MediaPath		Can be a file location or a Url.
	 * @param Outer			Outer to use for this object.
	 * @return				Media source or nullptr if none are appropriate.
	 */
	static UMediaSource* SpawnMediaSourceForString(const FString& MediaPath, UObject* Outer);

public:

	//~ IMediaOptions interface

	virtual FName GetDesiredPlayerName() const override;
	virtual bool GetMediaOption(const FName& Key, bool DefaultValue) const override;
	virtual double GetMediaOption(const FName& Key, double DefaultValue) const override;
	virtual int64 GetMediaOption(const FName& Key, int64 DefaultValue) const override;
	virtual FString GetMediaOption(const FName& Key, const FString& DefaultValue) const override;
	virtual FText GetMediaOption(const FName& Key, const FText& DefaultValue) const override;
	virtual TSharedPtr<FDataContainer, ESPMode::ThreadSafe> GetMediaOption(const FName& Key, const TSharedPtr<FDataContainer, ESPMode::ThreadSafe>& DefaultValue) const override;
	virtual bool HasMediaOption(const FName& Key) const override;

	/** Set a boolean parameter to pass to the player. */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "SetMediaOption (boolean)"), Category = "Media|MediaSource")
	void SetMediaOptionBool(const FName& Key, bool Value);
	/** Set a float parameter to pass to the player. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetMediaOption (float)"), Category = "Media|MediaSource")
	void SetMediaOptionFloat(const FName& Key, float Value);
	/** Set a double parameter to pass to the player. */
	void SetMediaOptionDouble(const FName& Key, double Value);
	/** Set an integer64 parameter to pass to the player. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetMediaOption (integer64)"), Category = "Media|MediaSource")
	void SetMediaOptionInt64(const FName& Key, int64 Value);
	/** Set a string parameter to pass to the player. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetMediaOption (string)"), Category = "Media|MediaSource")
	void SetMediaOptionString(const FName& Key, const FString& Value);

private:
	/** Holds our media options. */
	TMap<FName, FVariant> MediaOptionsMap;

	/**
	 * Get the media option specified by the Key as a Variant.
	 * Returns nullptr if the Key does not exist.
	 */
	const FVariant* GetMediaOptionDefault(const FName& Key) const;

	/**
	 * Sets the media option specified by Key to the supplied Variant.
	 */
	void SetMediaOption(const FName& Key, FVariant& Value);

	/**
	 * Get a mapping of file extensions to spawn delegates.
	 */
	static TMap<FString, FMediaSourceSpawnDelegate>& GetSpawnFromFileExtensionDelegates();
};
