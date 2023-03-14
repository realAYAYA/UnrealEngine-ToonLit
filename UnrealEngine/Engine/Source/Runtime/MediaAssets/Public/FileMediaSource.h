// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseMediaSource.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "UObject/UObjectGlobals.h"

#include "FileMediaSource.generated.h"

class UObject;
struct FFrame;
struct FPropertyChangedEvent;


UCLASS(BlueprintType)
class MEDIAASSETS_API UFileMediaSource
	: public UBaseMediaSource
{
	GENERATED_BODY()

public:

	/**
	 * The path to the media file to be played.
	 *
	 * @see SetFilePath
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=File, AssetRegistrySearchable)
	FString FilePath;


	/** Load entire media file into memory and play from there (if possible). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=File, AdvancedDisplay)
	bool PrecacheFile;

public:

	/**
	 * Get the path to the media file to be played.
	 *
	 * @return The file path.
	 * @see GetFullPath, SetFilePath
	 */
	const FString& GetFilePath() const
	{
		return FilePath;
	}

	/**
	 * Get the full path to the file.
	 *
	 * @return The full file path.
	 * @return GetFilePath
	 */
	FString GetFullPath() const;

	/**
	 * Set the path to the media file that this source represents.
	 *
	 * Automatically converts full paths to media sources that reside in the
	 * Engine's or project's /Content/Movies directory into relative paths.
	 *
	 * @param Path The path to set.
	 * @see FilePath, GetFilePath
	 */
	UFUNCTION(BlueprintCallable, Category="Media|FileMediaSource")
	void SetFilePath(const FString& Path);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:

	//~ IMediaOptions interface

	virtual bool GetMediaOption(const FName& Key, bool DefaultValue) const override;
	virtual bool HasMediaOption(const FName& Key) const override;

public:

	//~ UMediaSource interface

	virtual FString GetUrl() const override;
	virtual bool Validate() const override;

public:

	/** Name of the PrecacheFile media option. */
	static FName PrecacheFileOption;

private:
	void ResolveFullPath() const;
	void ClearResolvedFullPath() const;
	mutable FString ResolvedFullPath; // this is a cached variable updated in ResolveFullPath hence mutable
};
