// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureProfiler.h"

#include "Misc/CoreDelegates.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "RHIDefinitions.h"
#include "RHIResources.h"

#if TEXTURE_PROFILER_ENABLED

static int32 ToMB(uint64 Value)
{
	const size_t MB = 1024 * 1024;
	uint64 Value64 = (Value + (MB - 1)) / MB;
	int32 Value32 = static_cast<int32>(Value64);
	check(Value64 == Value32);
	return Value32;
}

static int32 ToKB(uint64 Value)
{
	const size_t KB = 1024;
	uint64 Value64 = (Value + (KB - 1)) / KB;
	int32 Value32 = static_cast<int32>(Value64);
	check(Value64 == Value32);
	return Value32;
}

CSV_DEFINE_CATEGORY(RenderTargetProfiler, true);
CSV_DEFINE_CATEGORY(RenderTargetWasteProfiler, true);
CSV_DEFINE_CATEGORY(TextureProfiler, true);
CSV_DEFINE_CATEGORY(TextureWasteProfiler, true);

static TAutoConsoleVariable<int32> CVarTextureProfilerMinTextureSizeMB(
	TEXT("r.TextureProfiler.MinTextureSizeMB"),
	16,
	TEXT("The minimum size for any texture to be reported.  All textures below this threshold will be reported as Other."));

static TAutoConsoleVariable<int32> CVarTextureProfilerMinRenderTargetSizeMB(
	TEXT("r.TextureProfiler.MinRenderTargetSizeMB"),
	0,
	TEXT("The minimum combined size for render targets to be reported.  All combined sizes less than this threshold will be reported as Other."));

static TAutoConsoleVariable<bool> CVarTextureProfilerEnableTextureCSV(
	TEXT("r.TextureProfiler.EnableTextureCSV"),
	true,
	TEXT("True to enable csv profiler output for all textures.  Does not include render targets."));

static TAutoConsoleVariable<bool> CVarTextureProfilerEnableRenderTargetCSV(
	TEXT("r.TextureProfiler.EnableRenderTargetCSV"),
	true,
	TEXT("True to enable csv profiler output for all Render Targets."));

static FAutoConsoleCommand CmdTextureProfilerDumpRenderTargets(
	TEXT("r.TextureProfiler.DumpRenderTargets"),
	TEXT("Dumps all render targets allocated by the RHI.\n")
	TEXT("Arguments:\n")
	TEXT("-CombineTextureNames    Combines all textures of the same name into a single line of output\n")
	TEXT("-CSV                    Produces CSV ready output"),

	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray< FString >& Args, UWorld* InWorld, FOutputDevice& Ar)
		{
			bool AsCSV = Args.Contains(TEXT("-CSV"));
			bool CombineTextureNames = Args.Contains(TEXT("-CombineTextureNames"));
			FTextureProfiler::Get()->DumpTextures(true, CombineTextureNames, AsCSV, Ar);
		})
	);

static FAutoConsoleCommand CmdTextureProfilerDumpTextures(
	TEXT("r.TextureProfiler.DumpTextures"),
	TEXT("Dumps all textures allocated by the RHI.  Does not include render targets\n")
	TEXT("Arguments:\n")
	TEXT("-CombineTextureNames    Combines all textures of the same name into a single line of output\n")
	TEXT("-CSV                    Produces CSV ready output"),

	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray< FString >& Args, UWorld* InWorld, FOutputDevice& Ar)
		{
			bool AsCSV = Args.Contains(TEXT("-CSV"));
			bool CombineTextureNames = Args.Contains(TEXT("-CombineTextureNames"));
			FTextureProfiler::Get()->DumpTextures(false, CombineTextureNames, AsCSV, Ar);
		})
	);

FTextureProfiler* FTextureProfiler::Instance = nullptr;

FTextureProfiler::FTextureDetails::FTextureDetails(FRHITexture* Texture, size_t InSize, uint32 InAlign, size_t InAllocationWaste)
	: Size(InSize)
	, PeakSize(0)
	, Align(InAlign)
	, AllocationWaste(InAllocationWaste)
	, IsRenderTarget(EnumHasAnyFlags(Texture->GetFlags(), TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable))
	, TextureName(Texture->GetName().IsValid() ? Texture->GetName() : FName("None"))
{
	SetName(TextureName);
}

void FTextureProfiler::FTextureDetails::SetName(FName InTextureName)
{
	TextureName = InTextureName;
	TextureName.SetNumber(0);

	// Do not delete the texture name string, it is not owned by this object
	//delete TextureNameString;

	TextureNameString = FTextureProfiler::Get()->GetTextureNameString(InTextureName);
}

void FTextureProfiler::FTextureDetails::ResetPeakSize()
{
	PeakSize = Size;
}

FTextureProfiler::FTextureDetails::~FTextureDetails()
{
	// Do not delete the texture name string, it is not owned by this object
	//delete TextureNameString;
}

FTextureProfiler::FTextureDetails& FTextureProfiler::FTextureDetails::operator+=(const FTextureProfiler::FTextureDetails& Other)
{
	Size += Other.Size;
	PeakSize = FMath::Max(PeakSize, Size);
	AllocationWaste += Other.AllocationWaste;
	Align = 0;
	Count += Other.Count;
	return *this;
}

FTextureProfiler::FTextureDetails& FTextureProfiler::FTextureDetails::operator-=(const FTextureProfiler::FTextureDetails& Other)
{
	Size -= Other.Size;
	AllocationWaste -= Other.AllocationWaste;
	Align = 0;
	Count -= Other.Count;
	return *this;
}

void FTextureProfiler::Init()
{
	FCoreDelegates::OnEndFrameRT.AddRaw(this, &FTextureProfiler::Update);

#if CSVPROFILERTRACE_ENABLED
	FCsvProfilerTrace::OutputInlineStat("Total", CSV_CATEGORY_INDEX(RenderTargetProfiler));
	FCsvProfilerTrace::OutputInlineStat("Total", CSV_CATEGORY_INDEX(RenderTargetWasteProfiler));
	FCsvProfilerTrace::OutputInlineStat("Other", CSV_CATEGORY_INDEX(RenderTargetProfiler));
	FCsvProfilerTrace::OutputInlineStat("Other", CSV_CATEGORY_INDEX(RenderTargetWasteProfiler));

	FCsvProfilerTrace::OutputInlineStat("Total", CSV_CATEGORY_INDEX(TextureProfiler));
	FCsvProfilerTrace::OutputInlineStat("Total", CSV_CATEGORY_INDEX(TextureWasteProfiler));
	FCsvProfilerTrace::OutputInlineStat("Other", CSV_CATEGORY_INDEX(TextureProfiler));
	FCsvProfilerTrace::OutputInlineStat("Other", CSV_CATEGORY_INDEX(TextureWasteProfiler));
#endif //CSVPROFILERTRACE_ENABLED
}

FTextureProfiler* FTextureProfiler::Get()
{
	if (Instance == nullptr)
	{
		Instance = new FTextureProfiler;
	}
	return Instance;
}

void FTextureProfiler::AddTextureAllocation(FRHITexture* UniqueTexturePtr, size_t Size, uint32 Alignment, size_t AllocationWaste)
{
	FScopeLock Lock(&TextureMapCS);

	FTextureDetails AddedDetails(
		UniqueTexturePtr,
		Size,
		Alignment,
		AllocationWaste);

	AddedDetails.Count = 1;

	FTextureDetails& TotalValue = AddedDetails.IsRenderTarget ? TotalRenderTargetSize : TotalTextureSize;
	TotalValue += AddedDetails;

	// Make sure we have a sane value
	check(ToMB(TotalValue.Size) >= 0);

	FTextureDetails& NamedValue = AddedDetails.IsRenderTarget ? CombinedRenderTargetSizes.FindOrAdd(AddedDetails.GetTextureName()) : CombinedTextureSizes.FindOrAdd(AddedDetails.GetTextureName());
	NamedValue += AddedDetails;

	if (NamedValue.GetTextureName().IsNone() || !NamedValue.GetTextureName().IsValid())
	{
		NamedValue.SetName(AddedDetails.GetTextureName());

#if CSVPROFILERTRACE_ENABLED
		if (AddedDetails.IsRenderTarget)
		{
			FCsvProfilerTrace::OutputInlineStat(NamedValue.GetTextureNameString(), CSV_CATEGORY_INDEX(RenderTargetProfiler));
			FCsvProfilerTrace::OutputInlineStat(NamedValue.GetTextureNameString(), CSV_CATEGORY_INDEX(RenderTargetWasteProfiler));
		}
		else
		{
			FCsvProfilerTrace::OutputInlineStat(NamedValue.GetTextureNameString(), CSV_CATEGORY_INDEX(TextureProfiler));
			FCsvProfilerTrace::OutputInlineStat(NamedValue.GetTextureNameString(), CSV_CATEGORY_INDEX(TextureWasteProfiler));
		}
#endif //CSVPROFILERTRACE_ENABLED
	}

	TexturesMap.Add(UniqueTexturePtr, AddedDetails);
}

void FTextureProfiler::UpdateTextureAllocation(FRHITexture* UniqueTexturePtr, size_t Size, uint32 Alignment, size_t AllocationWaste)
{
	FScopeLock Lock(&TextureMapCS);

	FTextureDetails UpdatedDetails(
		UniqueTexturePtr,
		Size,
		Alignment,
		AllocationWaste);

	FTextureDetails& ExistingTexture = TexturesMap[UniqueTexturePtr];
	FTextureDetails& TotalValue = UpdatedDetails.IsRenderTarget ? TotalRenderTargetSize : TotalTextureSize;

	TotalValue -= ExistingTexture;
	TotalValue += UpdatedDetails;

	check(ToMB(TotalValue.Size) >= 0);

	FTextureDetails& NamedValue = UpdatedDetails.IsRenderTarget ? CombinedRenderTargetSizes.FindOrAdd(UpdatedDetails.GetTextureName()) : CombinedTextureSizes.FindOrAdd(UpdatedDetails.GetTextureName());
	NamedValue -= ExistingTexture;
	NamedValue += UpdatedDetails;

	ExistingTexture = UpdatedDetails;
}

void FTextureProfiler::RemoveTextureAllocation(FRHITexture* UniqueTexturePtr)
{
	FScopeLock Lock(&TextureMapCS);
	FTextureDetails Details = TexturesMap.FindAndRemoveChecked(UniqueTexturePtr);
	
	FTextureDetails& TotalValue = Details.IsRenderTarget ? TotalRenderTargetSize : TotalTextureSize;
	TotalValue -= Details;

	FTextureDetails& NamedValue = Details.IsRenderTarget ? CombinedRenderTargetSizes[Details.GetTextureName()] : CombinedTextureSizes[Details.GetTextureName()];
	NamedValue -= Details;
}

void FTextureProfiler::UpdateTextureName(FRHITexture* UniqueTexturePtr)
{
	FScopeLock Lock(&TextureMapCS);

	FTextureDetails* DetailsPtr = TexturesMap.Find(UniqueTexturePtr);
	if (DetailsPtr == nullptr)
	{
		return;
	}

	FTextureDetails& Details = *DetailsPtr;
	FName OldName = Details.GetTextureName();
	Details.SetName(UniqueTexturePtr->GetName());

	FTextureDetails& NewCombinedValue = Details.IsRenderTarget ? CombinedRenderTargetSizes.FindOrAdd(Details.GetTextureName()) : CombinedTextureSizes.FindOrAdd(Details.GetTextureName());
	FTextureDetails& OldCombinedValue = Details.IsRenderTarget ? CombinedRenderTargetSizes[OldName] : CombinedTextureSizes[OldName];

	if (NewCombinedValue.GetTextureName().IsNone() || !NewCombinedValue.GetTextureName().IsValid())
	{
		NewCombinedValue.SetName(Details.GetTextureName());
	}

	OldCombinedValue -= Details;
	NewCombinedValue += Details;
}

const char* FTextureProfiler::GetTextureNameString(FName TextureName)
{
	const char*& TextureNameString = TextureNameStrings.FindOrAdd(TextureName, nullptr);

	if (TextureNameString == nullptr)
	{
		if(TextureName.GetDisplayNameEntry()->IsWide())
		{
			WIDECHAR NewTextureNameString[NAME_SIZE];
			TextureName.GetPlainWIDEString(NewTextureNameString);
			const auto ConvertedTextureNameString = StringCast<ANSICHAR>(NewTextureNameString);
			int Length = ConvertedTextureNameString.Length();
			TextureNameString = new char[Length + 1];
			FCStringAnsi::Strcpy(const_cast<char*>(TextureNameString), Length + 1, ConvertedTextureNameString.Get());
		}
		else
		{
			char NewTextureNameString[NAME_SIZE];
			TextureName.GetPlainANSIString(NewTextureNameString);
			int Length = FCStringAnsi::Strlen(NewTextureNameString);
			TextureNameString = new char[Length + 1];
			FCStringAnsi::Strcpy(const_cast<char*>(TextureNameString), Length + 1, NewTextureNameString);
		}
	}

	return TextureNameString;
}

#if CSVPROFILERTRACE_ENABLED

static void ReportTextureStat(const TAutoConsoleVariable<bool>& EnableVar, const char* StatName, uint32 CategoryIndex, int32 Size, ECsvCustomStatOp Op)
{
	if (EnableVar.GetValueOnRenderThread())
	{
		FCsvProfiler::RecordCustomStat(StatName, CategoryIndex, Size, Op);
	}
}

#endif //CSVPROFILERTRACE_ENABLED

void FTextureProfiler::Update()
{
#if CSVPROFILERTRACE_ENABLED

	check(IsInRenderingThread());

	if (FCsvProfiler::Get()->IsCapturing_Renderthread() && !CVarTextureProfilerEnableRenderTargetCSV.GetValueOnAnyThread() && !CVarTextureProfilerEnableTextureCSV.GetValueOnAnyThread())
	{
		return;
	}

	FScopeLock Lock(&TextureMapCS);
	
	size_t MinTextureSize = (size_t)CVarTextureProfilerMinTextureSizeMB.GetValueOnAnyThread() * 1024LL * 1024LL;
	size_t MinRenderTargetSize = (size_t)CVarTextureProfilerMinRenderTargetSizeMB.GetValueOnAnyThread() * 1024LL * 1024LL;

	// Keep track of all stats that have been added this round.  the CVS trace profiler wants to know what stats have been added, but only once per frame
	// Keep one set per category
	struct ECategoryIndex
	{
		enum
		{
			RenderTarget = 0,
			RenderTargetWaste,
			Texture,
			TextureWaste,
			Count
		};
	};
	
	FTextureDetails OtherRenderTargetSizes;
	for (auto& Pair : CombinedRenderTargetSizes)
	{
		// Don't report empty sizes or sizes less than the minimum
		if (Pair.Value.PeakSize == 0 || Pair.Value.PeakSize < MinRenderTargetSize)
		{
			OtherRenderTargetSizes += Pair.Value;
			continue;
		}

		ReportTextureStat(CVarTextureProfilerEnableRenderTargetCSV, Pair.Value.GetTextureNameString(), CSV_CATEGORY_INDEX(RenderTargetProfiler), ToMB(Pair.Value.PeakSize), ECsvCustomStatOp::Set);
		ReportTextureStat(CVarTextureProfilerEnableRenderTargetCSV, Pair.Value.GetTextureNameString(), CSV_CATEGORY_INDEX(RenderTargetWasteProfiler), ToMB(Pair.Value.AllocationWaste), ECsvCustomStatOp::Set);

		Pair.Value.ResetPeakSize();
	}

	FTextureDetails OtherTextureSizes;
	for (auto& Pair : CombinedTextureSizes)
	{
		// Don't report empty sizes or sizes less than the minimum
		if (Pair.Value.PeakSize == 0 || Pair.Value.PeakSize < MinTextureSize)
		{
			OtherTextureSizes += Pair.Value;
			continue;
		}

		ReportTextureStat(CVarTextureProfilerEnableTextureCSV, Pair.Value.GetTextureNameString(), CSV_CATEGORY_INDEX(TextureProfiler), ToMB(Pair.Value.PeakSize), ECsvCustomStatOp::Set);
		ReportTextureStat(CVarTextureProfilerEnableTextureCSV, Pair.Value.GetTextureNameString(), CSV_CATEGORY_INDEX(TextureWasteProfiler), ToMB(Pair.Value.AllocationWaste), ECsvCustomStatOp::Set);

		Pair.Value.ResetPeakSize();
	}

	ReportTextureStat(CVarTextureProfilerEnableRenderTargetCSV, "Total", CSV_CATEGORY_INDEX(RenderTargetProfiler), ToMB(TotalRenderTargetSize.PeakSize), ECsvCustomStatOp::Set);
	ReportTextureStat(CVarTextureProfilerEnableRenderTargetCSV, "Total", CSV_CATEGORY_INDEX(RenderTargetWasteProfiler), ToMB(TotalRenderTargetSize.AllocationWaste), ECsvCustomStatOp::Set);
	ReportTextureStat(CVarTextureProfilerEnableRenderTargetCSV, "Other", CSV_CATEGORY_INDEX(RenderTargetProfiler), ToMB(OtherRenderTargetSizes.PeakSize), ECsvCustomStatOp::Set);
	ReportTextureStat(CVarTextureProfilerEnableRenderTargetCSV, "Other", CSV_CATEGORY_INDEX(RenderTargetWasteProfiler), ToMB(OtherRenderTargetSizes.AllocationWaste), ECsvCustomStatOp::Set);

	ReportTextureStat(CVarTextureProfilerEnableTextureCSV, "Total", CSV_CATEGORY_INDEX(TextureProfiler), ToMB(TotalTextureSize.PeakSize), ECsvCustomStatOp::Set);
	ReportTextureStat(CVarTextureProfilerEnableTextureCSV, "Total", CSV_CATEGORY_INDEX(TextureWasteProfiler), ToMB(TotalTextureSize.AllocationWaste), ECsvCustomStatOp::Set);
	ReportTextureStat(CVarTextureProfilerEnableTextureCSV, "Other", CSV_CATEGORY_INDEX(TextureProfiler), ToMB(OtherTextureSizes.PeakSize), ECsvCustomStatOp::Set);
	ReportTextureStat(CVarTextureProfilerEnableTextureCSV, "Other", CSV_CATEGORY_INDEX(TextureWasteProfiler), ToMB(OtherTextureSizes.AllocationWaste), ECsvCustomStatOp::Set);

	TotalRenderTargetSize.ResetPeakSize();
	TotalTextureSize.ResetPeakSize();

#endif //CSVPROFILERTRACE_ENABLED
}

void FTextureProfiler::DumpTextures(bool RenderTargets, bool CombineTextureNames, bool AsCSV, FOutputDevice& OutputDevice)
{
	FScopeLock Lock(&TextureMapCS);
	const TCHAR Sep = AsCSV ? TEXT(',') : TEXT('\t');

	if (!AsCSV)
	{
		if (RenderTargets)
		{
			OutputDevice.Log(TEXT("Dumping all allocated RHI render targets"));
		}
		else
		{
			OutputDevice.Log(TEXT("Dumping all allocated RHI textures"));
		}
	}

	FTextureDetails& TotalValue = RenderTargets ? TotalRenderTargetSize : TotalTextureSize;

	if (CombineTextureNames)
	{
		OutputDevice.Logf(TEXT("%s%c%s%c%s%c%s"), TEXT("Texture"), Sep, TEXT("Combined Size(MB)"), Sep, TEXT("Count"), Sep, TEXT("Combined Wasted(KB)"));

		size_t MinTextureSize = CVarTextureProfilerMinTextureSizeMB.GetValueOnAnyThread() * 1024 * 1024;
		size_t MinRenderTargetSize = CVarTextureProfilerMinRenderTargetSizeMB.GetValueOnAnyThread() * 1024 * 1024;

		size_t MinSize = RenderTargets ? MinRenderTargetSize : MinTextureSize;

		const TMap<FName, FTextureDetails>& CombinedSizes = RenderTargets ? CombinedRenderTargetSizes : CombinedTextureSizes;

		TArray<const FTextureDetails*> CombinedSizesSorted;
		CombinedSizesSorted.Reserve(CombinedSizes.Num());

		using FSortTextureDetails = FTextureDetails;
		for (const auto& Pair : CombinedSizes)
		{
			int32 InsertIndex = Algo::LowerBound(CombinedSizesSorted, &Pair.Value,
				[](const FTextureDetails* Value1, const FTextureDetails* Value2)
				{
					return Value1->Size >= Value2->Size;
				});
			CombinedSizesSorted.Insert(&Pair.Value, InsertIndex);
		}

		FTextureDetails OtherTextureSizes;
		for (const FTextureDetails* TextureDetails : CombinedSizesSorted)
		{
			// Don't report empty sizes or sizes less than the minimum
			if (TextureDetails->Size == 0 || TextureDetails->Size < MinSize)
			{
				OtherTextureSizes += *TextureDetails;
				continue;
			}

			FString NameString = TextureDetails->GetTextureName().ToString();
			OutputDevice.Logf(TEXT("%s%c%d%c%d%c%d"), *NameString, Sep, ToMB(TextureDetails->Size), Sep, TextureDetails->Count, Sep, ToKB(TextureDetails->AllocationWaste));
		}

		if (OtherTextureSizes.Size > 0)
		{
			OutputDevice.Logf(TEXT("%s%c%d%c%d%c%d"), TEXT("Other"), Sep, ToMB(OtherTextureSizes.Size), Sep, OtherTextureSizes.Count, Sep, ToKB(OtherTextureSizes.AllocationWaste));
		}
		OutputDevice.Logf(TEXT("%s%c%d%c%d%c%d"), TEXT("Total"), Sep, ToMB(TotalValue.Size), Sep, TotalValue.Count, Sep, ToKB(TotalValue.AllocationWaste));
	}
	else
	{
		TArray<const FTextureDetails*> TexuteSizesSorted;
		TexuteSizesSorted.Reserve(TexturesMap.Num());

		using FSortTextureDetails = FTextureDetails;
		for (const auto& Pair : TexturesMap)
		{
			int32 InsertIndex = Algo::LowerBound(TexuteSizesSorted, &Pair.Value,
				[](const FTextureDetails* Value1, const FTextureDetails* Value2)
				{
					return Value1->Size >= Value2->Size;
				});
			TexuteSizesSorted.Insert(&Pair.Value, InsertIndex);
		}

		OutputDevice.Logf(TEXT("%s%c%s%c%s%c%s"), TEXT("Texture"), Sep, TEXT("Size(MB)"), Sep, TEXT("Wasted(KB)"), Sep, TEXT("Alignment"));
		for (const FTextureDetails* TextureDetails : TexuteSizesSorted)
		{
			if (TextureDetails->IsRenderTarget != RenderTargets)
			{
				continue;
			}

			FString NameString = TextureDetails->GetTextureName().ToString();
			OutputDevice.Logf(TEXT("%s%c%d%c%d%c%p"), *NameString, Sep, ToMB(TextureDetails->Size), Sep, ToKB(TextureDetails->AllocationWaste), Sep, TextureDetails->Align);
		}

		OutputDevice.Logf(TEXT("%s%c%d%c%d%c%p"), TEXT("Total"), Sep, ToMB(TotalValue.Size), Sep, ToKB(TotalValue.AllocationWaste), Sep, TotalValue.Align);
	}
}

#endif //TEXTURE_PROFILER_ENABLED
