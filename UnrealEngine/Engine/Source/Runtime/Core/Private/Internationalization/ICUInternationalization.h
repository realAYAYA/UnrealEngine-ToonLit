// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Containers/Set.h"
#include "Containers/Map.h"
#include "Internationalization/CulturePointer.h"
#include "Templates/UniquePtr.h"

class FInternationalization;

#if UE_ENABLE_ICU

THIRD_PARTY_INCLUDES_START
	#include <unicode/umachine.h>
PRAGMA_PUSH_PLATFORM_DEFAULT_PACKING
	#include <unicode/gregocal.h> // icu::Calendar can be affected by the non-standard packing UE4 uses, so force the platform default
PRAGMA_POP_PLATFORM_DEFAULT_PACKING
THIRD_PARTY_INCLUDES_END

// This should be defined by ICU.build.cs
#ifndef NEEDS_ICU_DLLS
	#define NEEDS_ICU_DLLS 0
#endif

class FICUInternationalization
{
public:
	FICUInternationalization(FInternationalization* const I18N);

	bool Initialize();
	void Terminate();

	void LoadAllCultureData();

	bool IsCultureRemapped(const FString& Name, FString* OutMappedCulture);
	bool IsCultureAllowed(const FString& Name);

	void RefreshCultureDisplayNames(const TArray<FString>& InPrioritizedDisplayCultureNames);
	void RefreshCachedConfigData();

	void HandleLanguageChanged(const FCultureRef InNewLanguage);
	void GetCultureNames(TArray<FString>& CultureNames) const;
	TArray<FString> GetPrioritizedCultureNames(const FString& Name);
	FCulturePtr GetCulture(const FString& Name);

	UDate UEDateTimeToICUDate(const FDateTime& DateTime);

private:
#if NEEDS_ICU_DLLS
	void LoadDLLs();
	void UnloadDLLs();
#endif

	void InitializeAvailableCultures();
	void ConditionalInitializeCultureMappings();
	void ConditionalInitializeAllowedCultures();

	enum class EAllowDefaultCultureFallback : uint8 { No, Yes, };
	FCulturePtr FindOrMakeCulture(const FString& Name, const EAllowDefaultCultureFallback AllowDefaultFallback);
	FCulturePtr FindOrMakeCanonizedCulture(const FString& Name, const EAllowDefaultCultureFallback AllowDefaultFallback);

	void InitializeTimeZone();
	void InitializeInvariantGregorianCalendar();

private:
	struct FICUCultureData
	{
		FString Name;
		FString LanguageCode;
		FString ScriptCode;
		FString CountryCode;

		bool operator==(const FICUCultureData& Other) const
		{
			return Name == Other.Name;
		}

		bool operator!=(const FICUCultureData& Other) const
		{
			return Name != Other.Name;
		}
	};

private:
	FInternationalization* const I18N;

	TArray<void*> DLLHandles;
	FString ICUDataDirectory;

	TArray<FICUCultureData> AllAvailableCultures;
	TMap<FString, int32> AllAvailableCulturesMap;
	TMap<FString, TArray<int32>> AllAvailableLanguagesToSubCulturesMap;

	bool bHasInitializedCultureMappings;
	TMap<FString, FString> CultureMappings;

	bool bHasInitializedAllowedCultures;
	TSet<FString> EnabledCultures;
	TSet<FString> DisabledCultures;

	TMap<FString, FCultureRef> CachedCultures;
	FCriticalSection CachedCulturesCS;

	TUniquePtr<icu::GregorianCalendar> InvariantGregorianCalendar;
	FCriticalSection InvariantGregorianCalendarCS;

	TArray<FString> CachedPrioritizedDisplayCultureNames;

	static UBool OpenDataFile(const void* InContext, void** OutFileContext, void** OutContents, const char* InPath);
	static void CloseDataFile(const void* InContext, void* const InFileContext, void* const InContents);

	// Tidy class for storing the count of references for an ICU data file and the file's data itself.
	struct FICUCachedFileData
	{
		FICUCachedFileData(const int64 FileSize);
		FICUCachedFileData(void* ExistingBuffer);
		FICUCachedFileData(FICUCachedFileData&& Source);
		~FICUCachedFileData();

		FICUCachedFileData(const FICUCachedFileData&) = delete;
		FICUCachedFileData& operator=(const FICUCachedFileData&) = delete;

		uint32 ReferenceCount;
		void* Buffer;
	};

	// Map for associating ICU data file paths with cached file data, to prevent multiple copies of immutable ICU data files from residing in memory.
	TMap<FString, FICUCachedFileData> PathToCachedFileDataMap;
};

#endif
