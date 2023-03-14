// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "OpenColorIOColorSpace.generated.h"


class UOpenColorIOConfiguration;

/**
 * Structure to identify a ColorSpace as described in an OCIO configuration file. 
 * Members are populated by data coming from a config file.
 */
USTRUCT(BlueprintType)
struct OPENCOLORIO_API FOpenColorIOColorSpace
{
	GENERATED_BODY()

public:
	/** Default constructor. */
	FOpenColorIOColorSpace();

	/**
	 * Create and initialize a new instance.
	 */
	FOpenColorIOColorSpace(const FString& InColorSpaceName, int32 InColorSpaceIndex, const FString& InFamilyName);

	/** The ColorSpace name. */
	UPROPERTY(VisibleAnywhere, Category=ColorSpace)
	FString ColorSpaceName;

	/** The index of the ColorSpace in the config */
	UPROPERTY(VisibleAnywhere, Category=ColorSpace)
	int32 ColorSpaceIndex;

	/** 
	 * The family of this ColorSpace as specified in the configuration file. 
	 * When you have lots of colorspaces, you can regroup them by family to facilitate browsing them. 
	 */
	UPROPERTY(VisibleAnywhere, Category=ColorSpace)
	FString FamilyName;

	/** Delimiter used in the OpenColorIO library to make family hierarchies */
	static const TCHAR* FamilyDelimiter;

public:
	bool operator==(const FOpenColorIOColorSpace& Other) const { return Other.ColorSpaceIndex == ColorSpaceIndex && Other.ColorSpaceName == ColorSpaceName; }
	bool operator!=(const FOpenColorIOColorSpace& Other) const { return !operator==(Other); }

	/**
	 * Get the string representation of this color space.
	 * @return ColorSpace name. 
	 */
	FString ToString() const;

	/** Return true if the index and name have been set properly */
	bool IsValid() const;

	/** Reset members to default/empty values. */
	void Reset();

	/** 
	 * Return the family name at the desired depth level 
	 * @param InDepth Desired depth in the family string. 0 == First layer. 
	 * @return FamilyName at the desired depth. Empty string if depth level doesn't exist.
	 */
	FString GetFamilyNameAtDepth(int32 InDepth) const;
};


/**
 * Transformation direction type for display-view transformations.
 */
UENUM(BlueprintType)
enum class EOpenColorIOViewTransformDirection : uint8
{
	Forward = 0     UMETA(DisplayName = "Forward"),
	Inverse = 1     UMETA(DisplayName = "Inverse")
};


USTRUCT(BlueprintType)
struct OPENCOLORIO_API FOpenColorIODisplayView
{
	GENERATED_BODY()

public:
	/** Default constructor. */
	FOpenColorIODisplayView();

	/**
	 * Create and initialize a new instance.
	 */
	FOpenColorIODisplayView(FStringView InDisplayName, FStringView InViewName);

	UPROPERTY(VisibleAnywhere, Category = ColorSpace)
	FString Display;

	UPROPERTY(VisibleAnywhere, Category = ColorSpace)
	FString View;

	FString ToString() const;

	/** Return true if the index and name have been set properly */
	bool IsValid() const;

	/** Reset members to default/empty values. */
	void Reset();

public:
	bool operator==(const FOpenColorIODisplayView& Other) const { return Other.Display == Display && Other.View == View; }
	bool operator!=(const FOpenColorIODisplayView& Other) const { return !operator==(Other); }
};

/**
 * Identifies a OCIO ColorSpace conversion.
 */
USTRUCT(BlueprintType)
struct OPENCOLORIO_API FOpenColorIOColorConversionSettings
{
	GENERATED_BODY()

public:

	/** Default constructor. */
	FOpenColorIOColorConversionSettings();

	/** The source color space name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorSpace)
	TObjectPtr<UOpenColorIOConfiguration> ConfigurationSource;

	/** The source color space name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorSpace)
	FOpenColorIOColorSpace SourceColorSpace;

	/** The destination color space name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorSpace)
	FOpenColorIOColorSpace DestinationColorSpace;

	/** The destination display view name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorSpace)
	FOpenColorIODisplayView DestinationDisplayView;

	/** The display view direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorSpace)
	EOpenColorIOViewTransformDirection DisplayViewDirection = EOpenColorIOViewTransformDirection::Forward;

public:

	/**
	 * Get a string representation of this conversion.
	 * @return String representation, i.e. "ConfigurationAssetName - SourceColorSpace to DestinationColorSpace".
	 */
	FString ToString() const;

	/**
	 * Returns true if the source and destination color spaces are found in the configuration file
	 */
	bool IsValid() const;

	/**
	* Ensure that the selected source and destination color spaces are valid, resets them otherwise.
	*/
	void ValidateColorSpaces();

private:
	/** Whether or not these settings are of the display-view type. */
	bool IsDisplayView() const;
};

/**
 * Identifies an OCIO Display look configuration 
 */
USTRUCT(BlueprintType)
struct OPENCOLORIO_API FOpenColorIODisplayConfiguration
{
	GENERATED_BODY()

public:
	/** Whether or not this display configuration is enabled
	 *  Since display look are applied on viewports, this will 
	 * dictate whether it's applied or not to it
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorSpace)
	bool bIsEnabled = false;
	
	/** Conversion to apply when this display is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorSpace)
	FOpenColorIOColorConversionSettings ColorConfiguration;
};

