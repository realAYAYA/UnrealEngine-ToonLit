// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/Commandlet.h"
#include "LocTextHelper.h"
#include "LocalizationSourceControlUtil.h"
#include "LocalizedAssetUtil.h"
#include "GatherTextCommandletBase.generated.h"

struct FGatherTextDelegates
{
	/** Delegate called during a localization gather to allow code to inject new gather and exclude paths for the given localization target */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FGetAdditionalGatherPaths, const FString& /*InLocalizationTargetName*/, TArray<FString>& /*InOutIncludePathFilters*/, TArray<FString>& /*InOutExcludePathFilters*/);
	static UNREALED_API FGetAdditionalGatherPaths GetAdditionalGatherPaths;
};

/** Performs fuzzy path matching against a set of include and exclude paths */
class FFuzzyPathMatcher
{
public:
	enum class EPathMatch
	{
		Included,
		Excluded,
		NoMatch,
	};

public:
	UNREALED_API FFuzzyPathMatcher(const TArray<FString>& InIncludePathFilters, const TArray<FString>& InExcludePathFilters);

	UNREALED_API EPathMatch TestPath(const FString& InPathToTest) const;

private:
	enum class EPathType : uint8
	{
		Include,
		Exclude,
	};

	struct FFuzzyPath
	{
		FFuzzyPath(FString InPathFilter, const EPathType InPathType)
			: PathFilter(MoveTemp(InPathFilter))
			, PathType(InPathType)
		{
		}

		FString PathFilter;
		EPathType PathType;
	};

	TArray<FFuzzyPath> FuzzyPaths;
};

/**
 *	UGatherTextCommandletBase: Base class for localization commandlets. Just to force certain behaviors and provide helper functionality. 
 */
UCLASS(MinimalAPI)
class UGatherTextCommandletBase : public UCommandlet
{
	GENERATED_UCLASS_BODY()

public:
	UNREALED_API virtual void Initialize( const TSharedRef< FLocTextHelper >& InGatherManifestHelper, const TSharedPtr< FLocalizationSCC >& InSourceControlInfo );
	UNREALED_API virtual void BeginDestroy() override;

	// Wrappers for extracting config values
	UNREALED_API bool GetBoolFromConfig( const TCHAR* Section, const TCHAR* Key, bool& OutValue, const FString& Filename );
	UNREALED_API bool GetStringFromConfig( const TCHAR* Section, const TCHAR* Key, FString& OutValue, const FString& Filename );
	UNREALED_API bool GetPathFromConfig( const TCHAR* Section, const TCHAR* Key, FString& OutValue, const FString& Filename );
	UNREALED_API int32 GetStringArrayFromConfig( const TCHAR* Section, const TCHAR* Key, TArray<FString>& OutArr, const FString& Filename );
	UNREALED_API int32 GetPathArrayFromConfig( const TCHAR* Section, const TCHAR* Key, TArray<FString>& OutArr, const FString& Filename );

	// Utilities for split platform detection
	UNREALED_API bool IsSplitPlatformName(const FName InPlatformName) const;
	UNREALED_API bool ShouldSplitPlatformForPath(const FString& InPath, FName* OutPlatformName = nullptr) const;
	UNREALED_API FName GetSplitPlatformNameFromPath(const FString& InPath) const;

	// Utility to get the correct base path (engine or project) for the current environment
	static UNREALED_API const FString& GetProjectBasePath();

	/**
* Returns true if this commandlet should run during a preview run.
* Override in child classes to conditionally skip a commandlet from being run.
* Most commandlets that require source control, write to files etc should be skipped for preview runs
*/
	virtual bool ShouldRunInPreview(const TArray<FString>& Switches, const TMap<FString, FString>& ParamVals) const
	{
		return false;
	}

protected:
	TSharedPtr< FLocTextHelper > GatherManifestHelper;

	TSharedPtr< FLocalizationSCC > SourceControlInfo;

	/** Mapping from platform name to the path marker for that platform */
	TMap<FName, FString> SplitPlatforms;

	// Common params and switches among all text gathering commadnlets 
	static UNREALED_API const TCHAR* ConfigParam;
	static UNREALED_API const TCHAR* EnableSourceControlSwitch;
	static UNREALED_API const TCHAR* DisableSubmitSwitch;
	static UNREALED_API const TCHAR* PreviewSwitch;
	static UNREALED_API const TCHAR* GatherTypeParam;

private:
	UNREALED_API virtual void CreateCustomEngine(const FString& Params) override ; //Disallow other text commandlets to make their own engine.	
};
