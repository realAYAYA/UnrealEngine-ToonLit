// Copyright Epic Games, Inc. All Rights Reserved.

#include "Fonts/FontCacheCompositeFont.h"
#include "Fonts/FontCacheFreeType.h"
#include "SlateGlobals.h"
#include "Fonts/FontBulkData.h"
#include "Misc/FileHelper.h"
#include "Algo/BinarySearch.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Application/SlateApplicationBase.h"
#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "Trace/SlateMemoryTags.h"

static bool GAsyncFontLazyLoad = false;
FAutoConsoleVariableRef CVarAsyncLazyLoad(
	TEXT("Slate.Font.AsyncLazyLoad"),
	GAsyncFontLazyLoad,
	TEXT("Causes unloaded font faces that are lazily loaded, to be loaded asynchronusly, until then the font won't measure correctly.  Once complete the UI will invalidate."));

DECLARE_CYCLE_STAT(TEXT("Load Font"), STAT_SlateLoadFont, STATGROUP_Slate);

class FAsyncLoadFontFaceData : public FNonAbandonableTask
{
public:
	FAsyncLoadFontFaceData(const FFontData& InFontData)
		: FontData(InFontData)
	{
	}

	const FFontData& GetFontData() { return FontData; }
	TArray<uint8>& GetFontFaceData() { return FontFaceData; }

	void DoWork()
	{
		if (!FFileHelper::LoadFileToArray(FontFaceData, *FontData.GetFontFilename()))
		{
			UE_LOG(LogSlate, Warning, TEXT("FAsyncLoadFontFaceData failed to load or process '%s' with face index %d"), *FontData.GetFontFilename(), FontData.GetSubFaceIndex());
		}
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncLoadFontFaceData, STATGROUP_ThreadPoolAsyncTasks);
	}

private:
	FFontData FontData;
	TArray<uint8> FontFaceData;
};

FCachedTypefaceData::FCachedTypefaceData()
	: Typeface(nullptr)
	, CachedFontData()
	, ScalingFactor(1.0f)
{
}

FCachedTypefaceData::FCachedTypefaceData(const FTypeface& InTypeface, const float InScalingFactor)
	: Typeface(&InTypeface)
	, CachedFontData()
	, ScalingFactor(InScalingFactor)
{
	// Add all the entries from the typeface
	for (const FTypefaceEntry& TypefaceEntry : Typeface->Fonts)
	{
		CachedFontData.Emplace(TypefaceEntry.Name, &TypefaceEntry.Font);
	}

	// Sort the entries now so we can binary search them later
	CachedFontData.Sort(&FCachedFontData::SortPredicate);
}

const FFontData* FCachedTypefaceData::GetPrimaryFontData() const
{
	check(Typeface);
	return Typeface->Fonts.Num() > 0 ? &Typeface->Fonts[0].Font : nullptr;
}

const FFontData* FCachedTypefaceData::GetFontData(const FName& InName) const
{
	const int32 FoundDataIndex = Algo::BinarySearchBy(CachedFontData, InName, &FCachedFontData::BinarySearchKey, &FCachedFontData::KeySortPredicate);
	return FoundDataIndex != INDEX_NONE ? CachedFontData[FoundDataIndex].FontData : nullptr;
}

void FCachedTypefaceData::GetCachedFontData(TArray<const FFontData*>& OutFontData) const
{
	for (const FCachedFontData& CachedFontDataEntry : CachedFontData)
	{
		OutFontData.Add(CachedFontDataEntry.FontData);
	}
}


FCachedCompositeFontData::FCachedCompositeFontData()
	: CompositeFont(nullptr)
	, CachedTypefaces()
	, CachedPriorityFontRanges()
	, CachedFontRanges()
{
}

FCachedCompositeFontData::FCachedCompositeFontData(const FCompositeFont& InCompositeFont)
	: CompositeFont(&InCompositeFont)
	, CachedTypefaces()
	, CachedPriorityFontRanges()
	, CachedFontRanges()
{
	// Add all the entries from the composite font
	CachedTypefaces.Add(MakeShared<FCachedTypefaceData>(CompositeFont->DefaultTypeface));
	CachedTypefaces.Add(MakeShared<FCachedTypefaceData>(CompositeFont->FallbackTypeface.Typeface, CompositeFont->FallbackTypeface.ScalingFactor));
	for (const FCompositeSubFont& SubTypeface : CompositeFont->SubTypefaces)
	{
		TSharedPtr<FCachedTypefaceData> CachedTypeface = MakeShared<FCachedTypefaceData>(SubTypeface.Typeface, SubTypeface.ScalingFactor);
		CachedTypefaces.Add(CachedTypeface);	
	}

	RefreshFontRanges();
}

const FCachedTypefaceData* FCachedCompositeFontData::GetTypefaceForCodepoint(const UTF32CHAR InCodepoint) const
{
	const int32 CharIndex = static_cast<int32>(InCodepoint);

	auto GetTypefaceFromRange = [CharIndex](const TArray<FCachedFontRange>& InFontRanges) -> const FCachedTypefaceData*
	{
		auto GetTypefaceFromRangeIndex = [CharIndex, &InFontRanges](const int32 InRangeIndex)
		{
			return (InFontRanges.IsValidIndex(InRangeIndex) && InFontRanges[InRangeIndex].Range.Contains(CharIndex))
				? InFontRanges[InRangeIndex].CachedTypeface.Get()
				: nullptr;
		};

		// Early out if this character is outside the bounds of any range
		if (InFontRanges.Num() == 0 || CharIndex < InFontRanges[0].Range.GetLowerBoundValue() || CharIndex > InFontRanges.Last().Range.GetUpperBoundValue())
		{
			return nullptr;
		}

		// This is only searching the lower-bound of the range, so it will return either the index of the correct range (if the character searched for
		// *is* the lower-bound of the range), otherwise it will return the next range - we need to test both ranges to see which one we should use
		const int32 FoundRangeIndex = Algo::LowerBoundBy(InFontRanges, CharIndex, &FCachedFontRange::BinarySearchKey);
		
		if (const FCachedTypefaceData* RangeTypeface = GetTypefaceFromRangeIndex(FoundRangeIndex))
		{
			return RangeTypeface;
		}
		
		if (const FCachedTypefaceData* RangeTypeface = GetTypefaceFromRangeIndex(FoundRangeIndex - 1))
		{
			return RangeTypeface;
		}

		return nullptr;
	};

	if (const FCachedTypefaceData* RangeTypeface = GetTypefaceFromRange(CachedPriorityFontRanges))
	{
		return RangeTypeface;
	}

	if (const FCachedTypefaceData* RangeTypeface = GetTypefaceFromRange(CachedFontRanges))
	{
		return RangeTypeface;
	}

	return CachedTypefaces[CachedTypeface_DefaultIndex].Get();
}

void FCachedCompositeFontData::GetCachedFontData(TArray<const FFontData*>& OutFontData) const
{
	for (const auto& CachedTypeface : CachedTypefaces)
	{
		CachedTypeface->GetCachedFontData(OutFontData);
	}
}

void FCachedCompositeFontData::RefreshFontRanges()
{
	// Get the current list of prioritized cultures so we can work out whether a sub-font should be prioritized or not
	const TArray<FString> PrioritizedCultureNames = FInternationalization::Get().GetPrioritizedCultureNames(FInternationalization::Get().GetCurrentLanguage()->GetName());
	auto IsPriorityCulture = [&PrioritizedCultureNames](const FString& InCultures) -> bool
	{
		TArray<FString> CultureNames;
		InCultures.ParseIntoArray(CultureNames, TEXT(";"));

		for (const FString& CultureName : CultureNames)
		{
			if (PrioritizedCultureNames.Contains(CultureName))
			{
				return true;
			}
		}

		return false;
	};

	// Clear the lists of ranges
	CachedPriorityFontRanges.Reset();
	CachedFontRanges.Reset();

	// Build the lists of ranges
	for (int32 SubTypefaceIndex = 0; SubTypefaceIndex < CompositeFont->SubTypefaces.Num(); ++SubTypefaceIndex)
	{
		const int32 CachedTypefaceIndex = CachedTypeface_FirstSubTypefaceIndex + SubTypefaceIndex;
		if (!CachedTypefaces.IsValidIndex(CachedTypefaceIndex))
		{
			continue;
		}

		const FCompositeSubFont& SubTypeface = CompositeFont->SubTypefaces[SubTypefaceIndex];
		TSharedPtr<FCachedTypefaceData> CachedTypeface = CachedTypefaces[CachedTypefaceIndex];

		TArray<FCachedFontRange>* CachedFontRangesPtr = SubTypeface.Cultures.IsEmpty() ? &CachedFontRanges : IsPriorityCulture(SubTypeface.Cultures) ? &CachedPriorityFontRanges : nullptr;
		if (CachedFontRangesPtr)
		{
			for (const FInt32Range& Range : SubTypeface.CharacterRanges)
			{
				if (!Range.IsEmpty())
				{
					CachedFontRangesPtr->Emplace(Range, CachedTypeface);
				}
			}
		}
	}

	// Sort the font ranges into ascending order so we can binary search them later
	CachedPriorityFontRanges.Sort(&FCachedFontRange::SortPredicate);
	CachedFontRanges.Sort(&FCachedFontRange::SortPredicate);

	// Merge any contiguous ranges to minimize our search count
	auto MergeContiguousRanges = [](TArray<FCachedFontRange>& InFontRanges)
	{
		for (int32 RangeIndex = 0; RangeIndex < InFontRanges.Num() - 1; ++RangeIndex)
		{
			FCachedFontRange& ThisRange = InFontRanges[RangeIndex];
			const FCachedFontRange& NextRange = InFontRanges[RangeIndex + 1];

			check(!ThisRange.Range.IsEmpty() && !NextRange.Range.IsEmpty());

			// Can only merge ranges with the same typeface
			if (ThisRange.CachedTypeface == NextRange.CachedTypeface && (ThisRange.Range.Overlaps(NextRange.Range) || ThisRange.Range.GetUpperBoundValue() + 1 == NextRange.Range.GetLowerBoundValue()))
			{
				ThisRange.Range = FInt32Range(
					FInt32Range::BoundsType::Inclusive(ThisRange.Range.GetLowerBoundValue()), 
					FInt32Range::BoundsType::Inclusive(FMath::Max(ThisRange.Range.GetUpperBoundValue(), NextRange.Range.GetUpperBoundValue()))
					);
				InFontRanges.RemoveAt(RangeIndex + 1, 1, EAllowShrinking::No);
			}
		}
	};
	MergeContiguousRanges(CachedPriorityFontRanges);
	MergeContiguousRanges(CachedFontRanges);
}


FCompositeFontCache::FCompositeFontCache(const FFreeTypeLibrary* InFTLibrary)
	: FTLibrary(InFTLibrary)
{
	check(FTLibrary);

	FInternationalization::Get().OnCultureChanged().AddRaw(this, &FCompositeFontCache::HandleCultureChanged);
}

FCompositeFontCache::~FCompositeFontCache()
{
	FInternationalization::Get().OnCultureChanged().RemoveAll(this);

	for (FAsyncTask<FAsyncLoadFontFaceData>* LoadFontFaceTask : LoadFontFaceTasks)
	{
		LoadFontFaceTask->EnsureCompletion();
		delete LoadFontFaceTask;
	}
}


const FFontData& FCompositeFontCache::GetDefaultFontData(const FSlateFontInfo& InFontInfo)
{
	static const FFontData DummyFontData;

	const FCompositeFont* const ResolvedCompositeFont = InFontInfo.GetCompositeFont();
	const FCachedTypefaceData* const CachedTypefaceData = GetDefaultCachedTypeface(ResolvedCompositeFont);
	if (CachedTypefaceData)
	{
		// Try to find the correct font from the typeface
		const FFontData* FoundFontData = CachedTypefaceData->GetFontData(InFontInfo.TypefaceFontName);
		if (FoundFontData)
		{
			return *FoundFontData;
		}

		// Failing that, return the primary font
		FoundFontData = CachedTypefaceData->GetPrimaryFontData();
		if (FoundFontData)
		{
			return *FoundFontData;
		}
	}

	return DummyFontData;
}

const FFontData& FCompositeFontCache::GetFontDataForCodepoint(const FSlateFontInfo& InFontInfo, const UTF32CHAR InCodepoint, float& OutScalingFactor)
{
	static const FFontData DummyFontData;

	auto GetFontDataForCharacterInTypeface = [this, InCodepoint, &InFontInfo](const FCachedTypefaceData* InCachedTypefaceData, const FCachedTypefaceData* InCachedDefaultTypefaceData, const bool InSkipCharacterCheck) -> const FFontData*
	{
		// Try to find the correct font from the typeface
		const FFontData* FoundFontData = InCachedTypefaceData->GetFontData(InFontInfo.TypefaceFontName);
		if (FoundFontData && (InSkipCharacterCheck || DoesFontDataSupportCodepoint(*FoundFontData, InCodepoint)))
		{
			return FoundFontData;
		}

		// Failing that, try and find a font by the attributes of the default font with the given name
		if (InCachedDefaultTypefaceData && InCachedTypefaceData != InCachedDefaultTypefaceData)
		{
			check(InCachedDefaultTypefaceData);

			if (const FFontData* const FoundDefaultFontData = InCachedDefaultTypefaceData->GetFontData(InFontInfo.TypefaceFontName))
			{
				const TSet<FName>& DefaultFontAttributes = GetFontAttributes(*FoundDefaultFontData);
				FoundFontData = GetBestMatchFontForAttributes(InCachedTypefaceData, DefaultFontAttributes);
				if (FoundFontData && (InSkipCharacterCheck || DoesFontDataSupportCodepoint(*FoundFontData, InCodepoint)))
				{
					return FoundFontData;
				}
			}
		}

		// Failing that, try the primary font
		FoundFontData = InCachedTypefaceData->GetPrimaryFontData();
		if (FoundFontData && (InSkipCharacterCheck || DoesFontDataSupportCodepoint(*FoundFontData, InCodepoint)))
		{
			return FoundFontData;
		}

		return nullptr;
	};

	const FCompositeFont* const ResolvedCompositeFont = InFontInfo.GetCompositeFont();
	const FCachedTypefaceData* const CachedTypefaceData = GetCachedTypefaceForCodepoint(ResolvedCompositeFont, InCodepoint);
	if (CachedTypefaceData)
	{
		const FCachedTypefaceData* const CachedDefaultTypefaceData = GetDefaultCachedTypeface(ResolvedCompositeFont);
		const FCachedTypefaceData* const CachedFallbackTypefaceData = GetFallbackCachedTypeface(ResolvedCompositeFont);
		check(CachedDefaultTypefaceData && CachedFallbackTypefaceData); // If we found a typeface from the font, we should always have found the default and fallback typeface too
		
		// Check the preferred typeface first
		if (const FFontData* FoundFontData = GetFontDataForCharacterInTypeface(CachedTypefaceData, CachedDefaultTypefaceData, /*InSkipCharacterCheck*/false))
		{
			OutScalingFactor = CachedTypefaceData->GetScalingFactor();
			return *FoundFontData;
		}

		// Failing that, try the default typeface (as the sub-font may not actually support the character we need)
		if (CachedTypefaceData != CachedDefaultTypefaceData)
		{
			if (const FFontData* FoundFontData = GetFontDataForCharacterInTypeface(CachedDefaultTypefaceData, nullptr, /*InSkipCharacterCheck*/false))
			{
				OutScalingFactor = CachedDefaultTypefaceData->GetScalingFactor();
				return *FoundFontData;
			}
		}

		// Failing that, try the fallback typeface (as both the sub-font or default font may not actually support the character we need)
		if (const FFontData* FoundFontData = GetFontDataForCharacterInTypeface(CachedFallbackTypefaceData, CachedDefaultTypefaceData, /*InSkipCharacterCheck*/true))
		{
			OutScalingFactor = CachedFallbackTypefaceData->GetScalingFactor();
			return *FoundFontData;
		}

		// If we got this far then the fallback font is empty, just force it to use the default font
		if (const FFontData* FoundFontData = GetFontDataForCharacterInTypeface(CachedDefaultTypefaceData, nullptr, /*InSkipCharacterCheck*/true))
		{
			OutScalingFactor = CachedDefaultTypefaceData->GetScalingFactor();
			return *FoundFontData;
		}
	}

	OutScalingFactor = 1.0f;
	return DummyFontData;
}

TSharedPtr<FFreeTypeFace> FCompositeFontCache::GetFontFace(const FFontData& InFontData)
{
	LLM_SCOPE_BYTAG(UI_Text);

	TSharedPtr<FFreeTypeFace> FaceAndMemory = FontFaceMap.FindRef(InFontData);
	if (!FaceAndMemory.IsValid() && InFontData.HasFont())
	{
		SCOPE_CYCLE_COUNTER(STAT_SlateLoadFont);

		// IMPORTANT: Do not log from this function until the new font has been added to the FontFaceMap, as it may be the Output Log font being loaded, which would cause an infinite recursion!
		FString LoadLogMessage;

		{
			// If this font data is referencing an asset, we just need to load it from memory
			FFontFaceDataConstPtr FontFaceData = InFontData.GetFontFaceData();
			if (FontFaceData.IsValid() && FontFaceData->HasData())
			{
				FaceAndMemory = MakeShared<FFreeTypeFace>(FTLibrary, FontFaceData.ToSharedRef(), InFontData.GetSubFaceIndex(), InFontData.GetLayoutMethod());
			}
		}

		// If no asset was loaded, then we go through the normal font loading process
		if (!FaceAndMemory.IsValid())
		{
			switch (InFontData.GetLoadingPolicy())
			{
			case EFontLoadingPolicy::LazyLoad:
				{
					if (GAsyncFontLazyLoad)
					{
						FAsyncTask<FAsyncLoadFontFaceData>* AsyncFontLoadTaskPtr = new FAsyncTask<FAsyncLoadFontFaceData>(InFontData);
						LoadFontFaceTasks.Add(AsyncFontLoadTaskPtr);
						AsyncFontLoadTaskPtr->StartBackgroundTask();

						FaceAndMemory = MakeShared<FFreeTypeFace>(InFontData.GetLayoutMethod());
					}
					else
					{
						const double FontDataLoadStartTime = FPlatformTime::Seconds();

						TArray<uint8> FontFaceData;
						if (FFileHelper::LoadFileToArray(FontFaceData, *InFontData.GetFontFilename()))
						{
							const int32 FontDataSizeKB = (FontFaceData.Num() + 1023) / 1024;
							LoadLogMessage = FString::Printf(TEXT("Took %f seconds to synchronously load lazily loaded font '%s' (%dK)"), float(FPlatformTime::Seconds() - FontDataLoadStartTime), *InFontData.GetFontFilename(), FontDataSizeKB);

							FaceAndMemory = MakeShared<FFreeTypeFace>(FTLibrary, FFontFaceData::MakeFontFaceData(MoveTemp(FontFaceData)), InFontData.GetSubFaceIndex(), InFontData.GetLayoutMethod());
						}
					}
				}
				break;
			case EFontLoadingPolicy::Stream:
				{
					FaceAndMemory = MakeShared<FFreeTypeFace>(FTLibrary, InFontData.GetFontFilename(), InFontData.GetSubFaceIndex(), InFontData.GetLayoutMethod());
				}
				break;
			default:
				break;
			}
		}

		// Got a valid font?
		if (FaceAndMemory.IsValid())
		{
			FaceAndMemory->OverrideAscent(InFontData.IsAscendOverridden(), InFontData.GetAscendOverriddenValue());
			FaceAndMemory->OverrideDescent(InFontData.IsDescendOverridden(), InFontData.GetDescendOverriddenValue());

			FontFaceMap.Add(InFontData, FaceAndMemory);

			if (!LoadLogMessage.IsEmpty())
			{
				UE_LOG(LogSlate, Log, TEXT("%s"), *LoadLogMessage);
			}

#if WITH_EDITOR //Triggers texts layout regeneration only in editor, don't want to slow down things in engine.
			GSlateLayoutGeneration++;
#endif
		}
		else
		{
			FaceAndMemory.Reset();
			UE_LOG(LogSlate, Warning, TEXT("GetFontFace failed to load or process '%s' with face index %d"), *InFontData.GetFontFilename(), InFontData.GetSubFaceIndex());
		}
	}
	return FaceAndMemory;
}

const TSet<FName>& FCompositeFontCache::GetFontAttributes(const FFontData& InFontData)
{
	static const TSet<FName> DummyAttributes;

	TSharedPtr<FFreeTypeFace> FaceAndMemory = GetFontFace(InFontData);
	return (FaceAndMemory.IsValid()) ? FaceAndMemory->GetAttributes() : DummyAttributes;
}

void FCompositeFontCache::FlushCompositeFont(const FCompositeFont& InCompositeFont)
{
	CompositeFontToCachedDataMap.Remove(&InCompositeFont);
}

void FCompositeFontCache::FlushCache()
{
	CompositeFontToCachedDataMap.Empty();
	FontFaceMap.Empty();
}

void FCompositeFontCache::Update()
{
	if (LoadFontFaceTasks.Num() > 0)
	{
		bool bRefreshNeeded = false;

		for (int32 LoadTaskIndex = 0; LoadTaskIndex < LoadFontFaceTasks.Num(); LoadTaskIndex++)
		{
			FAsyncTask<FAsyncLoadFontFaceData>* LoadFontFaceTask = LoadFontFaceTasks[LoadTaskIndex];
			if (LoadFontFaceTask->IsDone())
			{
				TArray<uint8>& FontFaceData = LoadFontFaceTask->GetTask().GetFontFaceData();

				const FFontData& FontData = LoadFontFaceTask->GetTask().GetFontData();
				TSharedPtr<FFreeTypeFace> FaceAndMemory = FontFaceMap.FindRef(FontData);
				if (FaceAndMemory.IsValid())
				{
					if (FontFaceData.Num() > 0)
					{
						FaceAndMemory->CompleteAsyncLoad(FTLibrary, FFontFaceData::MakeFontFaceData(MoveTemp(FontFaceData)), FontData.GetSubFaceIndex());
					}
					else
					{
						FaceAndMemory->FailAsyncLoad();
					}

					bRefreshNeeded = true;
				}

				delete LoadFontFaceTask;
				LoadFontFaceTasks.RemoveAt(LoadTaskIndex);
				LoadTaskIndex--;
			}
		}

		if (bRefreshNeeded)
		{
			AsyncTask(ENamedThreads::GameThread, []() {
				if (FSlateApplicationBase::IsInitialized())
				{
					FSlateApplicationBase::Get().InvalidateAllWidgets(false);
					GSlateLayoutGeneration++;
				}
			});
		}
	}
}

SIZE_T FCompositeFontCache::GetFontDataAssetResidentMemory(const UObject* FontDataAsset) const
{
	SIZE_T TotalAllocatedSize = 0;
	for (const TPair<FFontData, TSharedPtr<FFreeTypeFace>>& FaceAndMemoryData : FontFaceMap)
	{
		const FFontData& ExistingFontData = FaceAndMemoryData.Key;
		if (ExistingFontData.GetFontFaceAsset() == FontDataAsset)
		{
			TotalAllocatedSize += FaceAndMemoryData.Value->GetAllocatedMemorySize();
		}
	}

	return TotalAllocatedSize;
}

const FCachedCompositeFontData* FCompositeFontCache::GetCachedCompositeFont(const FCompositeFont* const InCompositeFont)
{
	if (!InCompositeFont)
	{
		return nullptr;
	}

	TSharedPtr<FCachedCompositeFontData>* const FoundCompositeFontData = CompositeFontToCachedDataMap.Find(InCompositeFont);
	if (FoundCompositeFontData)
	{
		return FoundCompositeFontData->Get();
	}

	return CompositeFontToCachedDataMap.Add(InCompositeFont, MakeShared<FCachedCompositeFontData>(*InCompositeFont)).Get();
}

const FFontData* FCompositeFontCache::GetBestMatchFontForAttributes(const FCachedTypefaceData* const InCachedTypefaceData, const TSet<FName>& InFontAttributes)
{
	const FFontData* BestMatchFont = nullptr;
	int32 BestMatchCount = 0;

	const FTypeface& Typeface = InCachedTypefaceData->GetTypeface();
	for (const FTypefaceEntry& TypefaceEntry : Typeface.Fonts)
	{
		const TSet<FName>& FontAttributes = GetFontAttributes(TypefaceEntry.Font);

		int32 MatchCount = 0;
		for (const FName& InAttribute : InFontAttributes)
		{
			if (FontAttributes.Contains(InAttribute))
			{
				++MatchCount;
			}
		}

		if (MatchCount > BestMatchCount || !BestMatchFont)
		{
			BestMatchFont = &TypefaceEntry.Font;
			BestMatchCount = MatchCount;
		}
	}

	return BestMatchFont;
}

bool FCompositeFontCache::DoesFontDataSupportCodepoint(const FFontData& InFontData, const UTF32CHAR InCodepoint)
{
#if WITH_FREETYPE
	TSharedPtr<FFreeTypeFace> FaceAndMemory = GetFontFace(InFontData);
	if (FaceAndMemory.IsValid())
	{
		if (!FaceAndMemory->IsFaceLoading())
		{
			return FT_Get_Char_Index(FaceAndMemory->GetFace(), InCodepoint) != 0;
		}

		// If the face is still loading, assume it has the the font face, until it loads and proves otherwise.
		return true;
	}
#endif // WITH_FREETYPE

	return false;
}

void FCompositeFontCache::HandleCultureChanged()
{
	// We need to refresh the font ranges immediately as the full font cache flush may not happen for one or more frames
	for (const auto& CompositeFontToCachedDataPair : CompositeFontToCachedDataMap)
	{
		CompositeFontToCachedDataPair.Value->RefreshFontRanges();
	}
}
