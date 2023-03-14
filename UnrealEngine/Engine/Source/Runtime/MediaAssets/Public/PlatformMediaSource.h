// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Internationalization/Text.h"
#include "MediaSource.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "PlatformMediaSource.generated.h"

class FArchive;
class FObjectPreSaveContext;
class UObject;
struct FGuid;


/**
 * A media source that selects other media sources based on target platform.
 *
 * Use this asset to override media sources on a per-platform basis.
 */
UCLASS(BlueprintType)
class MEDIAASSETS_API UPlatformMediaSource
	: public UMediaSource
{
	GENERATED_BODY()

public:

#if WITH_EDITORONLY_DATA

	/** Media sources per platform. */
	UPROPERTY(transient, EditAnywhere, Category=Sources, Meta=(DisplayName="Media Sources"))
	TMap<FString, TObjectPtr<UMediaSource>> PlatformMediaSources;

private:
	/** Blind data encountered at load that could not be mapped to a known platform */
	TMap<FGuid, UMediaSource*> BlindPlatformMediaSources;

#endif

public:
	//~ UObject interface
	PRAGMA_DISABLE_DEPRECATION_WARNINGS // Suppress compiler warning on override of deprecated function
	UE_DEPRECATED(5.0, "Use version that takes FObjectPreSaveContext instead.")
	virtual void PreSave(const class ITargetPlatform* TargetPlatform);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext);
	virtual void Serialize(FArchive& Ar) override;

	//~ UMediaSource interface

	virtual FString GetUrl() const override;
	virtual bool Validate() const override;

public:

	//~ IMediaOptions interface

	virtual FName GetDesiredPlayerName() const override;
	virtual bool GetMediaOption(const FName& Key, bool DefaultValue) const override;
	virtual double GetMediaOption(const FName& Key, double DefaultValue) const override;
	virtual int64 GetMediaOption(const FName& Key, int64 DefaultValue) const override;
	virtual FString GetMediaOption(const FName& Key, const FString& DefaultValue) const override;
	virtual FText GetMediaOption(const FName& Key, const FText& DefaultValue) const override;
	virtual bool HasMediaOption(const FName& Key) const override;

private:

	/**
	 * Get the media source for the running target platform.
	 *
	 * @return The media source, or nullptr if not set.
	 */
	UMediaSource* GetMediaSource() const;

private:

	/**
	 * Default media source.
	 *
	 * This media source will be used if no source was specified for a target platform.
	 */
	UPROPERTY()
	TObjectPtr<UMediaSource> MediaSource;
};
