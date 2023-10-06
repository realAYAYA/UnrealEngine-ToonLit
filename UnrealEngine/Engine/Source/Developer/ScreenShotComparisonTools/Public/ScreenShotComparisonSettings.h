// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "ScreenShotComparisonSettings.generated.h"

/**
* Holds settings for screenshot fallbacks
*/
USTRUCT()
struct FScreenshotFallbackEntry
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(Config)
	FString Parent;

	UPROPERTY(Config)
	FString Child;

	bool operator==(const FScreenshotFallbackEntry& Other) const
	{
		return Child == Other.Child;
	}
};

FORCEINLINE uint32 GetTypeHash(const FScreenshotFallbackEntry& Object)
{
	return GetTypeHash(Object.Child);
}

UCLASS(config = Engine, defaultconfig)
class SCREENSHOTCOMPARISONTOOLS_API UScreenShotComparisonSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/**
	* If true, any checked-in test results for confidential platforms will be put under <ProjectDir>/Platforms/<Platform>/Test instead of <ProjectDir>/Test
	*/
	UPROPERTY(Config)
	bool bUseConfidentialPlatformPathsForSavedResults;

	/**
	* An array of entries that describe other platforms we can use for fallbacks when comparing screenshots
	*/
	UPROPERTY(Config)
	TArray<FScreenshotFallbackEntry>  ScreenshotFallbackPlatforms;

	/**
	* Creates class instance
	* @param PlatformName Reference to a string containing platform name (if it is empty the current platform name is used).
	*/
	static UScreenShotComparisonSettings* Create(const FString& PlatformName = FString{});

	/**
	 * Loads settings of corresponding config.
	 */
	virtual void LoadSettings() final;

	/**
	 * Overrides config hierarchy platform to be used in UObject internals
	 */
	virtual const TCHAR* GetConfigOverridePlatform() const override;

#if WITH_EDITOR
public:
	static const TSet<FScreenshotFallbackEntry>& GetAllPlatformSettings();
#endif // WITH_EDITOR

protected:

	/**
	 * Returns platform name reference. As the class can store platform-independent config, it returns an empty string if the platform was not specified.
	 */
	virtual const FString& GetPlatformName() const;

	/**
	 * Sets platform and reloads settings.
	 * @param PlatformName Reference to a string containing platform name (if it is empty the default config is used).
	 */
	virtual void SetPlatform(const FString& PlatformName);

private:
	FString Platform;
};
