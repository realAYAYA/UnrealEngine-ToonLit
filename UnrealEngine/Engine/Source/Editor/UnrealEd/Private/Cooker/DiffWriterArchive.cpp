// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cooker/DiffWriterArchive.h"

#include "Compression/CompressionUtil.h"
#include "Cooker/DiffWriterLinkerLoadHeader.h"
#include "Cooker/DiffWriterZenHeader.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformStackWalk.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "PackageStoreOptimizer.h"
#include "Serialization/AsyncLoading2.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/StaticMemoryReader.h"
#include "UObject/LinkerLoad.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Package.h"
#include "UObject/PropertyOptional.h"
#include "UObject/PropertyTempVal.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectThreadContext.h"

namespace UE::DiffWriter
{

static const ANSICHAR* DebugDataStackMarker = "\r\nDebugDataStack:\r\n";
const TCHAR* const IndentToken = TEXT("%DWA%    ");
const TCHAR* const NewLineToken = TEXT("%DWA%\n");

/** Logs any mismatching header data. */
void DumpPackageHeaderDiffs(
	FAccumulatorGlobals& Globals, const FPackageData& SourcePackage, const FPackageData& DestPackage,
	const FString& AssetFilename, const int32 MaxDiffsToLog, const EPackageHeaderFormat PackageHeaderFormat,
	const FMessageCallback& MessageCallback);
/** Returns a new linker for loading the specified package. */
FLinkerLoad* CreateLinkerForPackage(
	FUObjectSerializeContext* LoadContext,
	const FString& InPackageName,
	const FString& InFilename,
	const FPackageData& PackageData);


FCallstacks::FCallstackData::FCallstackData(TUniquePtr<ANSICHAR[]>&& InCallstack, UObject* InSerializedObject, FProperty* InSerializedProperty)
	: Callstack(MoveTemp(InCallstack))
	, SerializedProp(InSerializedProperty)
{
	if (InSerializedObject)
	{
		SerializedObjectName = InSerializedObject->GetFullName();
	}
	if (InSerializedProperty)
	{
		SerializedPropertyName = InSerializedProperty->GetFullName();
	}
}

FString FCallstacks::FCallstackData::ToString(const TCHAR* CallstackCutoffText) const
{
	FString HumanReadableString;

	FString StackTraceText = Callstack.Get();
	if (CallstackCutoffText != nullptr)
	{
		// If the cutoff string is provided, remove all functions starting with the one specifiec in the cutoff string
		int32 CutoffIndex = StackTraceText.Find(CallstackCutoffText, ESearchCase::CaseSensitive);
		if (CutoffIndex > 0)
		{
			CutoffIndex = StackTraceText.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromEnd, CutoffIndex - 1);
			if (CutoffIndex > 0)
			{
				StackTraceText = StackTraceText.Left(CutoffIndex + 1);
			}
		}
	}

	TArray<FString> StackLines;
	StackTraceText.ParseIntoArrayLines(StackLines);
	for (FString& StackLine : StackLines)
	{
		if (StackLine.StartsWith(TEXT("0x")))
		{
			int32 CutoffIndex = StackLine.Find(TEXT(" "), ESearchCase::CaseSensitive);
			if (CutoffIndex >= -1 && CutoffIndex < StackLine.Len() - 2)
			{
				StackLine.MidInline(CutoffIndex + 1, MAX_int32, EAllowShrinking::No);
			}
		}
		HumanReadableString += IndentToken;
		HumanReadableString += StackLine;
		HumanReadableString += NewLineToken;
	}

	if (!SerializedObjectName.IsEmpty())
	{
		HumanReadableString += NewLineToken;
		HumanReadableString += IndentToken;
		HumanReadableString += TEXT("Serialized Object: ");
		HumanReadableString += SerializedObjectName;
		HumanReadableString += NewLineToken;
	}
	if (!SerializedPropertyName.IsEmpty())
	{
		if (SerializedObjectName.IsEmpty())
		{
			HumanReadableString += NewLineToken;
		}
		HumanReadableString += IndentToken;
		HumanReadableString += TEXT("Serialized Property: ");
		HumanReadableString += SerializedPropertyName;
		HumanReadableString += NewLineToken;
	}
	return HumanReadableString;
}

FCallstacks::FCallstackData FCallstacks::FCallstackData::Clone() const
{
	TUniquePtr<ANSICHAR[]> CallstackCopy;
	if (const int32 Len = FCStringAnsi::Strlen(Callstack.Get()); Len > 0)
	{
		CallstackCopy = MakeUnique<ANSICHAR[]>(Len + 1);
		FMemory::Memcpy(CallstackCopy.Get(), Callstack.Get(), Len + 1);
	}

	FCallstackData Clone(MoveTemp(CallstackCopy), nullptr, SerializedProp);
	Clone.SerializedObjectName = SerializedObjectName;

	return Clone;
}

FCallstacks::FCallstacks()
	: bCallstacksDirty(true)
	, StackTraceSize(65535)
	, LastSerializeCallstack(nullptr)
	, EndOffset(0)
{
	StackTrace = MakeUnique<ANSICHAR[]>(StackTraceSize);
	StackTrace[0] = 0;
}

void FCallstacks::Reset()
{
	CallstackAtOffsetMap.Reset();
	UniqueCallstacks.Reset();
	bCallstacksDirty = true;
	LastSerializeCallstack = nullptr;
	StackTrace[0] = 0;
	EndOffset = 0;
}

ANSICHAR* FCallstacks::AddUniqueCallstack(bool bIsCollectingCallstacks, UObject* SerializedObject, FProperty* SerializedProperty, uint32& OutCallstackCRC)
{
	ANSICHAR* Callstack = nullptr;
	if (bIsCollectingCallstacks)
	{
		OutCallstackCRC = FCrc::StrCrc32(StackTrace.Get());

		if (FCallstackData* ExistingCallstack = UniqueCallstacks.Find(OutCallstackCRC))
		{
			Callstack = ExistingCallstack->Callstack.Get();
		}
		else
		{
			const int32 Len = FCStringAnsi::Strlen(StackTrace.Get()) + 1;
			TUniquePtr<ANSICHAR[]> NewCallstack = MakeUnique<ANSICHAR[]>(Len);
			FCStringAnsi::Strcpy(NewCallstack.Get(), Len, StackTrace.Get());
			FCallstackData& NewEntry = UniqueCallstacks.Add(OutCallstackCRC, FCallstackData(MoveTemp(NewCallstack), SerializedObject, SerializedProperty));
			Callstack = NewEntry.Callstack.Get();
		}
	}
	else
	{
		OutCallstackCRC = 0;
	}
	return Callstack;
}

void FCallstacks::Add(
	int64 Offset,
	int64 Length,
	UObject* SerializedObject,
	FProperty* SerializedProperty,
	TArrayView<const FName> DebugDataStack,
	bool bIsCollectingCallstacks,
	bool bCollectCurrentCallstack,
	int32 StackIgnoreCount)
{
	if (UE::ArchiveStackTrace::ShouldBypassDiff())
	{
		return;
	}
	++StackIgnoreCount;

	const int64 CurrentOffset = Offset;
	EndOffset = FMath::Max(EndOffset, CurrentOffset + Length); 

	const bool bShouldCollectCallstack = bIsCollectingCallstacks && bCollectCurrentCallstack && !UE::ArchiveStackTrace::ShouldIgnoreDiff();
	if (bShouldCollectCallstack)
	{
		StackTrace[0] = '\0';
		FPlatformStackWalk::StackWalkAndDump(StackTrace.Get(), StackTraceSize, StackIgnoreCount);
		//if we have a debug name stack, plaster it onto the end of the current stack buffer so that it's a part of the unique stack entry.
		if (DebugDataStack.Num() > 0)
		{
			FCStringAnsi::Strcat(StackTrace.Get(), StackTraceSize, DebugDataStackMarker);

			const FString SubIndent = FString(IndentToken) + FString(TEXT("    "));

			bool bIsIndenting = true;
			for (const auto& DebugData : DebugDataStack)
			{
				if (bIsIndenting)
				{
					FCStringAnsi::Strcat(StackTrace.Get(), StackTraceSize, TCHAR_TO_ANSI(*SubIndent));
				}

				ANSICHAR DebugName[NAME_SIZE];
				DebugData.GetPlainANSIString(DebugName);
				FCStringAnsi::Strcat(StackTrace.Get(), StackTraceSize, DebugName);

				//these are special-cased, as we assume they'll be followed by object/property names and want the names on the same line for readability's sake.
				const bool bIsPropertyLabel = (DebugData == TEXT("SerializeScriptProperties") || DebugData == TEXT("PropertySerialize") || DebugData == TEXT("SerializeTaggedProperty"));
				const ANSICHAR* const LineEnd = bIsPropertyLabel ? ": " : "\r\n";
				FCStringAnsi::Strcat(StackTrace.Get(), StackTraceSize, LineEnd);
				bIsIndenting = !bIsPropertyLabel;
			}
		}
		// Make sure we compare the new stack trace with the last one in the next if statement
		bCallstacksDirty = true;
	}

	if (LastSerializeCallstack == nullptr || (bCallstacksDirty && FCStringAnsi::Strcmp(LastSerializeCallstack, StackTrace.Get()) != 0))
	{
		uint32 CallstackCRC = 0;
		bool bSuppressLogging = UE::ArchiveStackTrace::ShouldIgnoreDiff();
		if (CallstackAtOffsetMap.Num() == 0 || CurrentOffset > CallstackAtOffsetMap.Last().Offset)
		{
			// New data serialized at the end of archive buffer
			LastSerializeCallstack = AddUniqueCallstack(bIsCollectingCallstacks, SerializedObject, SerializedProperty, CallstackCRC);
			CallstackAtOffsetMap.Add(FCallstackAtOffset { CurrentOffset, CallstackCRC, bSuppressLogging });
		}
		else
		{
			// This happens usually after Seek() so we need to find the exiting offset or insert a new one
			const int32 CallstackToUpdateIndex = GetCallstackIndexAtOffset(CurrentOffset);
			check(CallstackToUpdateIndex != -1);
			FCallstackAtOffset& CallstackToUpdate = CallstackAtOffsetMap[CallstackToUpdateIndex];
			LastSerializeCallstack = AddUniqueCallstack(bIsCollectingCallstacks, SerializedObject, SerializedProperty, CallstackCRC);
			if (CallstackToUpdate.Offset == CurrentOffset)
			{
				CallstackToUpdate.Callstack = CallstackCRC;
			}
			else
			{
				// Insert a new callstack
				check(CallstackToUpdate.Offset < CurrentOffset);
				CallstackAtOffsetMap.Insert(FCallstackAtOffset {CurrentOffset, CallstackCRC, bSuppressLogging }, CallstackToUpdateIndex + 1);
			}
		}
		check(CallstackCRC != 0 || !bShouldCollectCallstack);
	}
	else if (LastSerializeCallstack)
	{
		// Skip callstack comparison on next serialize call unless we grab a stack trace
		bCallstacksDirty = false;
	}
}

int32 FCallstacks::GetCallstackIndexAtOffset(int64 Offset, int32 MinOffsetIndex) const
{
	if (Offset < 0 || Offset >= EndOffset || MinOffsetIndex >= CallstackAtOffsetMap.Num())
	{
		return -1;
	}

	// Find the index of the offset the InOffset maps to
	int32 OffsetForCallstackIndex = -1;
	MinOffsetIndex = FMath::Max(MinOffsetIndex, 0);
	int32 MaxOffsetIndex = CallstackAtOffsetMap.Num() - 1;

	// Binary search
	for (; MinOffsetIndex <= MaxOffsetIndex; )
	{
		int32 SearchIndex = (MinOffsetIndex + MaxOffsetIndex) / 2;
		if (CallstackAtOffsetMap[SearchIndex].Offset < Offset)
		{
			MinOffsetIndex = SearchIndex + 1;
		}
		else if (CallstackAtOffsetMap[SearchIndex].Offset > Offset)
		{
			MaxOffsetIndex = SearchIndex - 1;
		}
		else
		{
			OffsetForCallstackIndex = SearchIndex;
			break;
		}
	}
	
	if (OffsetForCallstackIndex == -1)
	{
		// We didn't find the exact offset value so let's try to find the first one that is lower than the requested one
		MinOffsetIndex = FMath::Min(MinOffsetIndex, CallstackAtOffsetMap.Num() - 1);
		for (int32 FirstLowerOffsetIndex = MinOffsetIndex; FirstLowerOffsetIndex >= 0; --FirstLowerOffsetIndex)
		{
			if (CallstackAtOffsetMap[FirstLowerOffsetIndex].Offset < Offset)
			{
				OffsetForCallstackIndex = FirstLowerOffsetIndex;
				break;
			}
		}
		if (OffsetForCallstackIndex != -1)
		{
			check(CallstackAtOffsetMap[OffsetForCallstackIndex].Offset < Offset);
			check(OffsetForCallstackIndex == (CallstackAtOffsetMap.Num() - 1) || CallstackAtOffsetMap[OffsetForCallstackIndex + 1].Offset > Offset);
		}
	}

	return OffsetForCallstackIndex;
}

void FCallstacks::RemoveRange(int64 StartOffset, int64 Length)
{
	CallstackAtOffsetMap.RemoveAll([StartOffset, Length](const FCallstackAtOffset& Entry)
		{
			return StartOffset <= Entry.Offset && Entry.Offset < StartOffset + Length;
		});
}

void FCallstacks::Append(const FCallstacks& Other, int64 OtherStartOffset)
{
	for (const FCallstackAtOffset& OtherOffset : Other.CallstackAtOffsetMap)
	{
		FCallstackAtOffset& New = CallstackAtOffsetMap.Add_GetRef(OtherOffset);
		New.Offset += OtherStartOffset;
	}

	CallstackAtOffsetMap.Sort([](const FCallstackAtOffset& LHS,const FCallstackAtOffset& RHS)
	{
		return LHS.Offset < RHS.Offset;
	});

	for (const TPair<uint32, FCallstackData>& Kv : Other.UniqueCallstacks)
	{
		if (FCallstackData* Existing = UniqueCallstacks.Find(Kv.Key))
		{
			if (LastSerializeCallstack == Existing->Callstack.Get())
			{
				LastSerializeCallstack = nullptr;
			}
			UniqueCallstacks.Remove(Kv.Key);
		}
		UniqueCallstacks.Emplace(Kv.Key, Kv.Value.Clone());
	}

	EndOffset = FMath::Max(EndOffset, Other.EndOffset + OtherStartOffset);
}

struct FBreakAtOffsetSettings
{
	FString PackageToBreakOn;
	int64 OffsetToBreakOn = -1;
	bool bInitialized = false;

	void Initialize()
	{
		if (bInitialized)
		{
			return;
		}
		bInitialized = true;
		OffsetToBreakOn = -1;
		PackageToBreakOn.Empty();

		if (!FParse::Param(FCommandLine::Get(), TEXT("cooksinglepackage")) &&
			!FParse::Param(FCommandLine::Get(), TEXT("cooksinglepackagenorefs")))
		{
			return;
		}

		FString Package;
		if (!FParse::Value(FCommandLine::Get(), TEXT("map="), Package) &&
			!FParse::Value(FCommandLine::Get(), TEXT("package="), Package))
		{
			return;
		}

		int64 Offset;
		// DiffOnlyBreakOffset should be the Combined/DiffBreak Offset from the warning message
		if (!FParse::Value(FCommandLine::Get(), TEXT("diffonlybreakoffset="), Offset) || Offset <= 0)
		{
			return;
		}

		OffsetToBreakOn = Offset;
		PackageToBreakOn = TEXT("/") + FPackageName::GetShortName(Package);
	}

	bool MatchesFilename(const FString& Filename) const
	{
		int32 SubnameIndex = Filename.Find(PackageToBreakOn, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (SubnameIndex < 0)
		{
			return false;
		}
		int32 SubnameEndIndex = SubnameIndex + PackageToBreakOn.Len();
		return SubnameEndIndex == Filename.Len() || Filename[SubnameEndIndex] == TEXT('.');
	}

} GBreakAtOffsetSettings;


void FCallstacks::RecordSerialize(EOffsetFrame OffsetFrame, int64 CurrentOffset, int64 Length,
	const FAccumulator& Accumulator, FDiffArchive& Ar, int32 StackIgnoreCount)
{
	int64 LinkerOffset = -1;

	// If the writer is using postsave transforms, then we need to know what segment we are in before deciding whether
	// to allow use of the LinkerOffset for diffonlybreakoffset or for recording the LinkerOffset for each callstack.
	// The segment information is only known after the first save.
	if (!Accumulator.IsWriterUsingPostSaveTransforms() || Accumulator.bFirstSaveComplete)
	{
		switch (OffsetFrame)
		{
		case EOffsetFrame::Linker:
			LinkerOffset = CurrentOffset;
			break;
		case EOffsetFrame::Exports:
			if (Accumulator.bFirstSaveComplete)
			{
				LinkerOffset = CurrentOffset + Accumulator.HeaderSize;
			}
			break;
		default:
			checkNoEntry();
			break;
		}
	}

	++StackIgnoreCount;

	if (LinkerOffset >= 0)
	{
		if (GBreakAtOffsetSettings.OffsetToBreakOn >= 0 &&
			LinkerOffset <= GBreakAtOffsetSettings.OffsetToBreakOn && GBreakAtOffsetSettings.OffsetToBreakOn < LinkerOffset + Length)
		{
			if (GBreakAtOffsetSettings.MatchesFilename(Accumulator.Filename))
			{
				if (Accumulator.IsWriterUsingPostSaveTransforms() && OffsetFrame == EOffsetFrame::Linker &&
					LinkerOffset < Accumulator.PreTransformHeaderSize)
				{
					// Do not break at this serialize call; it's in the pre-transformed header. If the requested
					// breakoffset is not within the post-transformed header, then this offset is in the wrong segment
					// and is not a match. If the breakoffset is within the post-transformed header we break and give
					// instructions during OnFirstSaveComplete.
				}
				else
				{
					if (!UE::ArchiveStackTrace::ShouldBypassDiff() && !UE::ArchiveStackTrace::ShouldIgnoreDiff())
					{
						UE_DEBUG_BREAK();
					}
				}
			}
		}
	}

	if (Length > 0)
	{
		UObject* SerializedObject = Ar.GetSerializeContext() ? Ar.GetSerializeContext()->SerializedObject : nullptr;
		TArrayView<const FName> DebugStack = Ar.GetDebugDataStack();

		const bool bCollectingCallstacks = Accumulator.bFirstSaveComplete;
		const bool bCollectCurrentCallstack = bCollectingCallstacks && LinkerOffset >= 0 &&
			(!Accumulator.IsWriterUsingPostSaveTransforms() || LinkerOffset >= Accumulator.HeaderSize) &&
			Accumulator.DiffMap.ContainsOffset(LinkerOffset);

		Add(CurrentOffset, Length, SerializedObject, Ar.GetSerializedProperty(), DebugStack, bCollectingCallstacks,
			bCollectCurrentCallstack, StackIgnoreCount);
	}
}

FAccumulatorGlobals::FAccumulatorGlobals(ICookedPackageWriter* InnerPackageWriter)
	: PackageWriter(InnerPackageWriter)
{
}

void FAccumulatorGlobals::Initialize(EPackageHeaderFormat InFormat)
{
	if (bInitialized)
	{
		return;
	}
	bInitialized = true;
	Format = InFormat;
	switch (Format)
	{
	case EPackageHeaderFormat::PackageFileSummary:
		break;
	case EPackageHeaderFormat::ZenPackageSummary:
		FPackageStoreOptimizer::FindScriptObjects(ScriptObjectsMap);
		break;
	default:
		checkNoEntry();
		break;
	}
}

FAccumulator::FAccumulator(FAccumulatorGlobals& InGlobals, UObject* InAsset, FName InPackageName,
	int32 InMaxDiffsToLog, bool bInIgnoreHeaderDiffs, FMessageCallback&& InMessageCallback,
	EPackageHeaderFormat InPackageHeaderFormat)
	: LinkerCallstacks()
	, ExportsCallstacks()
	, Globals(InGlobals)
	, MessageCallback(MoveTemp(InMessageCallback))
	, PackageName(InPackageName)
	, Asset(InAsset)
	, MaxDiffsToLog(InMaxDiffsToLog)
	, PackageHeaderFormat(InPackageHeaderFormat)
	, bIgnoreHeaderDiffs(bInIgnoreHeaderDiffs)
{
	GBreakAtOffsetSettings.Initialize();
}

FAccumulator::~FAccumulator()
{
}

void FAccumulator::SetHeaderSize(int64 InHeaderSize)
{
	HeaderSize = InHeaderSize;
}

FName FAccumulator::GetAssetClass() const
{
	return Asset != nullptr ? Asset->GetClass()->GetFName() : NAME_None;
}

bool FAccumulator::IsWriterUsingPostSaveTransforms() const
{
	return PackageHeaderFormat != EPackageHeaderFormat::PackageFileSummary;
}

void FAccumulator::OnFirstSaveComplete(FStringView LooseFilePath, int64 InHeaderSize, int64 InPreTransformHeaderSize,
	ICookedPackageWriter::FPreviousCookedBytesData&& InPreviousPackageData)
{
	Filename = LooseFilePath;
	HeaderSize = InHeaderSize;
	PreTransformHeaderSize = InPreTransformHeaderSize;
	PreviousPackageData = MoveTemp(InPreviousPackageData);

	if (IsWriterUsingPostSaveTransforms())
	{
		// The header has been transformed, so all callstacks in it are invalid; remove them
		LinkerCallstacks.RemoveRange(0, PreTransformHeaderSize);
	}
	LinkerCallstacks.Append(ExportsCallstacks, HeaderSize);
	ExportsCallstacks.Reset();

	GenerateDiffMap();
	if (HasDifferences())
	{
		// Make a copy of the LinkerArchive for comparison in case it differs even in the second memory save
		check(LinkerArchive);
		FirstSaveLinkerData.Empty(LinkerArchive->TotalSize());
		FirstSaveLinkerData.Append(LinkerArchive->GetData(), LinkerArchive->TotalSize());
	}

	LinkerCallstacks.Reset();
	bFirstSaveComplete = true;


	if (IsWriterUsingPostSaveTransforms() &&
		GBreakAtOffsetSettings.MatchesFilename(Filename) &&
		GBreakAtOffsetSettings.OffsetToBreakOn < HeaderSize)
	{
		// The package writer used for this cook transforms the header before saving it to disk, for e.g. compression.
		// This means that we don't in general know which offsets in the pre-transform header correspond to
		// the offsets in the header on disk, and we only know the callstack for the offsets in the pre-transform
		// header. So we don't know where to break during serialization of the header.
		// If you specified settings to break at an offset in the pre-transform header, you need instead to debug
		// using the information reported by DumpPackageHeaderDiffs.
		UE_DEBUG_BREAK();
	}
}

void FAccumulator::OnSecondSaveComplete(int64 InHeaderSize)
{
	check(bFirstSaveComplete); // Should have been set in OnFirstSaveComplete
	if (HeaderSize != InHeaderSize)
	{
		MessageCallback(ELogVerbosity::Error, FString::Printf(
			TEXT("%s: Indeterministic header size. When saving the package twice into memory, first header size %d != second header size %d. Callstacks for indeterminism in the exports will be incorrect.")
			TEXT("\n\tDumping differences from first and second memory saves."),
			*this->Filename, HeaderSize, InHeaderSize));

		check(bFirstSaveComplete);
		check(FirstSaveLinkerData.Num() >= HeaderSize);
		check(LinkerArchive && LinkerArchive->TotalSize() >= InHeaderSize);

		FPackageData FirstSaveHeader{ FirstSaveLinkerData.GetData(), HeaderSize};
		FPackageData SecondSaveHeader{ LinkerArchive->GetData(), InHeaderSize };
		int32 NumHeaderDiffMessages = 0;
		DumpPackageHeaderDiffs(Globals, FirstSaveHeader, SecondSaveHeader, Filename, MaxDiffsToLog,
			PackageHeaderFormat,
			[&NumHeaderDiffMessages, this](ELogVerbosity::Type Verbosity, FStringView Message)
			{
				MessageCallback(Verbosity, Message);
				++NumHeaderDiffMessages;
			});
		if (NumHeaderDiffMessages == 0)
		{
			MessageCallback(ELogVerbosity::Warning, FString::Printf(
				TEXT("%s: headers are different, but DumpPackageHeaderDiffs does not yet implement describing the difference."),
				*Filename));
		}
	}

	if (IsWriterUsingPostSaveTransforms())
	{
		// The header has been transformed, so all callstacks in it are invalid; remove them
		LinkerCallstacks.RemoveRange(0, PreTransformHeaderSize);
	}
	LinkerCallstacks.Append(ExportsCallstacks, HeaderSize);
	ExportsCallstacks.Reset();
}

bool FAccumulator::HasDifferences() const
{
	return bHasDifferences;
}

FDiffArchive::FDiffArchive(FAccumulator& InAccumulator)
	: Accumulator(&InAccumulator)
{
	SetIsPersistent(true /* bIsPersistent */);
}

FString FDiffArchive::GetArchiveName() const
{
	return Accumulator->Filename;
}

void FDiffArchive::PushDebugDataString(const FName& DebugData)
{
	DebugDataStack.Push(DebugData);
}

void FDiffArchive::PopDebugDataString()
{
	DebugDataStack.Pop();
}

TArray<FName>& FDiffArchive::GetDebugDataStack()
{
	return DebugDataStack;
}

FDiffArchiveForLinker::FDiffArchiveForLinker(FAccumulator& InAccumulator)
	: FDiffArchive(InAccumulator)
{
	// SavePackage is supposed to destroy the archive before returning, we rely on that so that the linkerarchive
	// in the second save can use the same data that was used in the first save
	check(!Accumulator->LinkerArchive);
	Accumulator->LinkerArchive = this;
}

FDiffArchiveForLinker::~FDiffArchiveForLinker()
{
	check(Accumulator->LinkerArchive == this)
	Accumulator->LinkerArchive = nullptr;
}

void FDiffArchiveForLinker::Serialize(void* InData, int64 Num)
{
	int32 StackIgnoreCount = 1;
	int64 CurrentOffset = Tell();
	Accumulator->LinkerCallstacks.RecordSerialize(EOffsetFrame::Linker, CurrentOffset, Num, *Accumulator,
		*this, StackIgnoreCount);
	FDiffArchive::Serialize(InData, Num);
}

FDiffArchiveForExports::FDiffArchiveForExports(FAccumulator& InAccumulator)
	: FDiffArchive(InAccumulator)
{
	// SavePackage is supposed to destroy the archive before returning, we rely on that so that the exportsarchive
	// in the second save can use the same data that was used in the first save
	check(!Accumulator->ExportsArchive);
	Accumulator->ExportsArchive = this;
}

FDiffArchiveForExports::~FDiffArchiveForExports()
{
	check(Accumulator->ExportsArchive == this);
	Accumulator->ExportsArchive = nullptr;
}

void FDiffArchiveForExports::Serialize(void* InData, int64 Num)
{
	int32 StackIgnoreCount = 1;
	int64 CurrentOffset = Tell();
	Accumulator->ExportsCallstacks.RecordSerialize(EOffsetFrame::Exports, CurrentOffset, Num, *Accumulator,
		*this, StackIgnoreCount);
	FDiffArchive::Serialize(InData, Num);
}

bool ShouldDumpPropertyValueState(FProperty* Prop)
{
	if (Prop->IsA<FNumericProperty>()
		|| Prop->IsA<FStrProperty>()
		|| Prop->IsA<FBoolProperty>()
		|| Prop->IsA<FNameProperty>())
	{
		return true;
	}

	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
	{
		return ShouldDumpPropertyValueState(ArrayProp->Inner);
	}

	if (FMapProperty* MapProp = CastField<FMapProperty>(Prop))
	{
		return ShouldDumpPropertyValueState(MapProp->KeyProp) && ShouldDumpPropertyValueState(MapProp->ValueProp);
	}

	if (FSetProperty* SetProp = CastField<FSetProperty>(Prop))
	{
		return ShouldDumpPropertyValueState(SetProp->ElementProp);
	}

	if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
	{
		if (StructProp->Struct == TBaseStructure<FVector>::Get()
			|| StructProp->Struct == TBaseStructure<FGuid>::Get())
		{
			return true;
		}
	}

	if (FOptionalProperty* OptionalProp = CastField<FOptionalProperty>(Prop))
	{
		return ShouldDumpPropertyValueState(OptionalProp->GetValueProperty());
	}

	return false;
}

void FAccumulator::CompareWithPreviousForSection(const FPackageData& SourcePackage, const FPackageData& DestPackage,
	const TCHAR* CallstackCutoffText, int32& InOutDiffsLogged, TMap<FName, FArchiveDiffStats>& OutStats,
	const FString& SectionFilename)
{
	FCallstacks& Callstacks = this->LinkerCallstacks;
	const int64 SourceSize = SourcePackage.Size - SourcePackage.StartOffset;
	const int64 DestSize = DestPackage.Size - DestPackage.StartOffset;
	const int64 SizeToCompare = FMath::Min(SourceSize, DestSize);
	const FName AssetClass = GetAssetClass();
	
	if (SourceSize != DestSize)
	{
		MessageCallback(ELogVerbosity::Warning, FString::Printf(
			TEXT("%s: Size mismatch: on disk: %lld vs memory: %lld"), *SectionFilename, SourceSize, DestSize));
		int64 SizeDiff = DestPackage.Size - SourcePackage.Size;
		OutStats.FindOrAdd(AssetClass).DiffSize += SizeDiff;
	}

	FString LastDifferenceCallstackDataText;
	int32 LastDifferenceCallstackOffsetIndex = -1;
	int64 NumDiffsLocal = 0;
	int64 NumDiffsForLogStatLocal = 0;
	int64 NumDiffsLoggedLocal = 0;
	int64 FirstUnreportedDiffIndex = -1;

	for (int64 LocalOffset = 0; LocalOffset < SizeToCompare; ++LocalOffset)
	{
		const int64 SourceAbsoluteOffset = LocalOffset + SourcePackage.StartOffset;
		const int64 DestAbsoluteOffset = LocalOffset + DestPackage.StartOffset;

		const uint8 SourceByte = SourcePackage.Data[SourceAbsoluteOffset];
		const uint8 DestByte   = DestPackage  .Data[DestAbsoluteOffset];
		if (SourceByte == DestByte)
		{
			continue;
		}

		constexpr int BytesToLog = 128;
		int32 DifferenceCallstackOffsetIndex = Callstacks.GetCallstackIndexAtOffset(DestAbsoluteOffset,
			LastDifferenceCallstackOffsetIndex < 0 ? 0 : LastDifferenceCallstackOffsetIndex);

		// Skip reporting the difference if we are still within the same Serialize call, or for any bytes after
		// the first different byte if we don't know the callstack.
		if (NumDiffsLocal > 0 &&
			DifferenceCallstackOffsetIndex == LastDifferenceCallstackOffsetIndex)
		{
			continue;
		}

		// Also skip reporting the difference if it is another occurrence of the last reported callstack
		const FCallstacks::FCallstackAtOffset* CallstackAtOffsetPtr = nullptr;
		const FCallstacks::FCallstackData* DifferenceCallstackDataPtr = nullptr;
		FString DifferenceCallstackDataText;
		bool bCallstackSuppressLogging = false;
		if (DifferenceCallstackOffsetIndex >= 0)
		{
			CallstackAtOffsetPtr = &Callstacks.GetCallstack(DifferenceCallstackOffsetIndex);
			bCallstackSuppressLogging = CallstackAtOffsetPtr->bSuppressLogging;

			DifferenceCallstackDataPtr = &Callstacks.GetCallstackData(*CallstackAtOffsetPtr);
			DifferenceCallstackDataText = DifferenceCallstackDataPtr->ToString(CallstackCutoffText);
			if (NumDiffsLocal > 0 && 
				LastDifferenceCallstackDataText.Compare(DifferenceCallstackDataText, ESearchCase::CaseSensitive) == 0)
			{
				continue;
			}
		}

		// Update counter for number of existing diffs
		OutStats.FindOrAdd(AssetClass).NumDiffs++;
		NumDiffsLocal++;
		// Update LastReported fields
		LastDifferenceCallstackOffsetIndex = DifferenceCallstackOffsetIndex;
		LastDifferenceCallstackDataText = MoveTemp(DifferenceCallstackDataText);

		// Skip logging of the difference if the callstack has a suppresslogging scope 
		if (bCallstackSuppressLogging)
		{
			return;
		}
		// Skip logging of header differences if requested
		if (bIgnoreHeaderDiffs && DestAbsoluteOffset < HeaderSize)
		{
			continue;
		}

		// Update counter for number of diffs that should be reported as existing when over the limit
		// Ignored header diffs and suppressed callstacks do not contribute to this count
		NumDiffsForLogStatLocal++;

		// Skip logging of the difference if we are over the limit
		if (bCallstackSuppressLogging || (MaxDiffsToLog >= 0 && InOutDiffsLogged >= MaxDiffsToLog))
		{
			if (FirstUnreportedDiffIndex == -1)
			{
				FirstUnreportedDiffIndex = LocalOffset;
			}
			continue;
		}

		// Update counter for number of logged diffs
		InOutDiffsLogged++;
		NumDiffsLoggedLocal++;

		if (DifferenceCallstackOffsetIndex < 0)
		{
			if (IsWriterUsingPostSaveTransforms() && DestAbsoluteOffset < HeaderSize)
			{
				MessageCallback(ELogVerbosity::Warning, FString::Printf(
					TEXT("%s: Difference at offset %lld (Combined/DiffBreak Offset: %" INT64_FMT "): OnDisk %d != %d InMemory.%s")
					TEXT("Callstack is unknown because the offset is in the header and the header has been optimized. See the output of DumpPackageHeaderDiffs to debug this difference."),
					*SectionFilename, LocalOffset, DestAbsoluteOffset, SourceByte, DestByte, NewLineToken
				));
			}
			else
			{
				MessageCallback(ELogVerbosity::Warning, FString::Printf(
					TEXT("%s: Difference at offset %lld (Combined/DiffBreak Offset: %" INT64_FMT "): OnDisk %d != %d InMemory.%s")
					TEXT("Callstack is unknown."),
					*SectionFilename, LocalOffset, DestAbsoluteOffset, SourceByte, DestByte, NewLineToken
				));
			}
		}
		else
		{
			check(CallstackAtOffsetPtr && DifferenceCallstackDataPtr); // These were set up above.
			const FCallstacks::FCallstackAtOffset& CallstackAtOffset = *CallstackAtOffsetPtr;
			const FCallstacks::FCallstackData& DifferenceCallstackData = *DifferenceCallstackDataPtr;

			FString BeforePropertyVal;
			FString AfterPropertyVal;
			if (FProperty* SerProp = DifferenceCallstackData.SerializedProp)
			{
				// We don't attempt to serialize properties when we have already encountered at least one diff in the asset.
				// That is because we don't handle length differences in the source and destination data caused by the previous
				// diffs.  Those previous length differences could mean the current diff is at a position that is offset and
				// invalid to serialize the property from.  That can result in things like serializing an array or string which has
				// a negative number of elements, or other invalid situations that lead to an assert or crash.  If we must
				// sesrialize these properties, we would have to ensure that we keep an appropriate offset separate for the source
				// archive and the dest archive.
				if ((SourceSize == DestSize) && (InOutDiffsLogged < 2) && ShouldDumpPropertyValueState(SerProp))
				{
					// Walk backwards until we find a callstack which wasn't from the given property
					int64 OffsetX = DestAbsoluteOffset;
					for (;;)
					{
						if (OffsetX == 0)
						{
							break;
						}

						const int32 CallstackIndex = Callstacks.GetCallstackIndexAtOffset(OffsetX - 1, 0);
						if (CallstackIndex < 0)
						{
							break;
						}
						const FCallstacks::FCallstackAtOffset& PreviousCallstack = Callstacks.GetCallstack(CallstackIndex);
						const FCallstacks::FCallstackData& PreviousCallstackData = Callstacks.GetCallstackData(PreviousCallstack);
						if (PreviousCallstackData.SerializedProp != SerProp)
						{
							break;
						}

						--OffsetX;
					}

					FPropertyTempVal SourceVal(SerProp);
					FPropertyTempVal DestVal(SerProp);

					FStaticMemoryReader SourceReader(&SourcePackage.Data[SourceAbsoluteOffset - (DestAbsoluteOffset - OffsetX)], SourcePackage.Size - SourceAbsoluteOffset);
					FStaticMemoryReader DestReader(&DestPackage.Data[OffsetX], DestPackage.Size - DestAbsoluteOffset);

					SourceVal.Serialize(SourceReader);
					DestVal.Serialize(DestReader);

					if (!SourceReader.IsError() && !DestReader.IsError())
					{
						SourceVal.ExportText(BeforePropertyVal);
						DestVal.ExportText(AfterPropertyVal);
					}
				}
			}

			FString DiffValues;
			if (BeforePropertyVal != AfterPropertyVal)
			{
				DiffValues = FString::Printf(TEXT("\r\n%sBefore: %s\r\n%sAfter:  %s"),
					IndentToken, *BeforePropertyVal,
					IndentToken, *AfterPropertyVal);
			}

			FString DebugDataStackText;
			//check for a debug data stack as part of the unique stack entry, and log it out if we find it.
			FString FullStackText = DifferenceCallstackData.Callstack.Get();
			int32 DebugDataIndex = FullStackText.Find(ANSI_TO_TCHAR(DebugDataStackMarker), ESearchCase::CaseSensitive);
			if (DebugDataIndex > 0)
			{
				DebugDataStackText = FString::Printf(TEXT("\r\n%s"), IndentToken)
					+ FullStackText.RightChop(DebugDataIndex + 2);
			}

			MessageCallback(ELogVerbosity::Warning, FString::Printf(
				TEXT("%s: Difference at offset %lld (Combined/DiffBreak Offset: %" INT64_FMT "): OnDisk %d != %d InMemory.%s")
					TEXT("Difference occurs at index %lld within Serialize call at callstack:%s%s%s%s"),
				*SectionFilename, LocalOffset, DestAbsoluteOffset, SourceByte, DestByte, NewLineToken,
				DestAbsoluteOffset - CallstackAtOffset.Offset, NewLineToken,
				*LastDifferenceCallstackDataText, *DiffValues, *DebugDataStackText
			));
		}

		MessageCallback(ELogVerbosity::Display, FString::Printf(
			TEXT("%s: Logging %d bytes around offset: %lld (%016X) in the OnDisk package:"),
			*SectionFilename,
			BytesToLog, LocalOffset, LocalOffset
		));
		TArray<FString> HexDumpLines = FCompressionUtil::HexDumpLines(SourcePackage.Data + SourcePackage.StartOffset,
			SourcePackage.Size - SourcePackage.StartOffset,
			LocalOffset - BytesToLog / 2, LocalOffset + BytesToLog / 2);
		for (FString& Line : HexDumpLines)
		{
			MessageCallback(ELogVerbosity::Display, Line);
		}

		MessageCallback(ELogVerbosity::Display, FString::Printf(
			TEXT("%s: Logging %d bytes around offset: %lld (%016X) in the InMemory package:"),
			*SectionFilename, BytesToLog, LocalOffset, LocalOffset
		));
		HexDumpLines = FCompressionUtil::HexDumpLines(DestPackage.Data + DestPackage.StartOffset,
			DestPackage.Size - DestPackage.StartOffset,
			LocalOffset - BytesToLog / 2, LocalOffset + BytesToLog / 2);
		for (FString& Line : HexDumpLines)
		{
			MessageCallback(ELogVerbosity::Display, Line);
		}
	}

	if (MaxDiffsToLog >= 0 && NumDiffsForLogStatLocal > NumDiffsLoggedLocal)
	{
		MessageCallback(ELogVerbosity::Warning, FString::Printf(
			TEXT("%s: %lld difference(s) not logged (first at offset: %lld)."),
			*SectionFilename, NumDiffsForLogStatLocal - NumDiffsLoggedLocal, FirstUnreportedDiffIndex));
	}
}

void FAccumulator::CompareWithPrevious(const TCHAR* CallstackCutoffText, TMap<FName, FArchiveDiffStats>&OutStats)
{
	// An FDiffArchiveForLinker should have been constructed by SavePackage and should still be in memory
	check(LinkerArchive);

	Globals.Initialize(PackageHeaderFormat);

	const FName AssetClass = GetAssetClass();
	OutStats.FindOrAdd(AssetClass).NewFileTotalSize = LinkerArchive->TotalSize();
	if (PreviousPackageData.Size == 0)
	{
		MessageCallback(ELogVerbosity::Warning, FString::Printf(TEXT("New package: %s"), *Filename));
		OutStats.FindOrAdd(AssetClass).DiffSize = OutStats.FindOrAdd(AssetClass).NewFileTotalSize;
		return;
	}

	FPackageData SourcePackage;
	SourcePackage.Data = PreviousPackageData.Data.Get();
	SourcePackage.Size = PreviousPackageData.Size;
	SourcePackage.HeaderSize = PreviousPackageData.HeaderSize;
	SourcePackage.StartOffset = PreviousPackageData.StartOffset;

	FPackageData DestPackage;
	DestPackage.Data = LinkerArchive->GetData();
	DestPackage.Size = LinkerArchive->TotalSize();
	DestPackage.HeaderSize = HeaderSize;
	DestPackage.StartOffset = 0;

	MessageCallback(ELogVerbosity::Display, FString::Printf(TEXT("Comparing: %s"), *Filename));
	MessageCallback(ELogVerbosity::Warning, FString::Printf(TEXT("Asset class: %s"), *AssetClass.ToString()));

	int32 NumLoggedDiffs = 0;
	
	FPackageData SourcePackageHeader = SourcePackage;
	SourcePackageHeader.Size = SourcePackage.HeaderSize;
	SourcePackageHeader.HeaderSize = 0;
	SourcePackageHeader.StartOffset = 0;

	FPackageData DestPackageHeader = DestPackage;
	DestPackageHeader.Size = HeaderSize;
	DestPackageHeader.HeaderSize = 0;
	DestPackageHeader.StartOffset = 0;

	CompareWithPreviousForSection(SourcePackageHeader, DestPackageHeader, CallstackCutoffText,
		NumLoggedDiffs, OutStats, Filename);

	if (HeaderSize > 0 && OutStats.FindOrAdd(AssetClass).NumDiffs > 0)
	{
		int32 NumHeaderDiffMessages = 0;
		DumpPackageHeaderDiffs(Globals, SourcePackage, DestPackage, Filename, MaxDiffsToLog,
			PackageHeaderFormat,
			[&NumHeaderDiffMessages, this](ELogVerbosity::Type Verbosity, FStringView Message)
			{
				MessageCallback(Verbosity, Message);
				++NumHeaderDiffMessages;
			});
		if (NumHeaderDiffMessages == 0)
		{
			MessageCallback(ELogVerbosity::Warning, FString::Printf(
				TEXT("%s: headers are different, but DumpPackageHeaderDiffs does not yet implement describing the difference."),
				*Filename));
		}
	}

	FPackageData SourcePackageExports = SourcePackage;
	SourcePackageExports.HeaderSize = 0;
	SourcePackageExports.StartOffset = PreviousPackageData.HeaderSize;

	FPackageData DestPackageExports = DestPackage;
	DestPackageExports.HeaderSize = 0;
	DestPackageExports.StartOffset = HeaderSize;

	FString ExportsFilename;
	if (DestPackage.HeaderSize > 0)
	{
		ExportsFilename = FPaths::ChangeExtension(Filename, TEXT("uexp"));
	}
	else
	{
		ExportsFilename = Filename;
	}

	CompareWithPreviousForSection(SourcePackageExports, DestPackageExports, CallstackCutoffText,
		NumLoggedDiffs, OutStats, ExportsFilename);

	// Optionally save out any differences we detected.
	const FArchiveDiffStats& Stats = OutStats.FindOrAdd(AssetClass);
	if (Stats.NumDiffs > 0)
	{
		static struct FDiffOutputSettings
		{
			FString DiffOutputDir;

			FDiffOutputSettings()
			{
				FString Dir;
				if (!FParse::Value(FCommandLine::Get(), TEXT("diffoutputdir="), Dir))
				{
					return;
				}

				FPaths::NormalizeDirectoryName(Dir);
				DiffOutputDir = MoveTemp(Dir) + TEXT("/");
			}
		} DiffOutputSettings;

		// Only save out the differences if we have a -diffoutputdir set.
		if (!DiffOutputSettings.DiffOutputDir.IsEmpty())
		{
			FString OutputFilename = FPaths::ConvertRelativePathToFull(Filename);
			FString SavedDir       = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
			if (OutputFilename.StartsWith(SavedDir))
			{
				OutputFilename.ReplaceInline(*SavedDir, *DiffOutputSettings.DiffOutputDir);

				IFileManager& FileManager = IFileManager::Get();

				// Copy the original asset as '.before.uasset'.
				{
					TUniquePtr<FArchive> DiffUAssetArchive(FileManager.CreateFileWriter(*FPaths::SetExtension(OutputFilename, TEXT(".before.") + FPaths::GetExtension(Filename))));
					DiffUAssetArchive->Serialize(SourcePackageHeader.Data + SourcePackageHeader.StartOffset, SourcePackageHeader.Size - SourcePackageHeader.StartOffset);
				}
				{
					TUniquePtr<FArchive> DiffUExpArchive(FileManager.CreateFileWriter(*FPaths::SetExtension(OutputFilename, TEXT(".before.uexp"))));
					DiffUExpArchive->Serialize(SourcePackageExports.Data + SourcePackageExports.StartOffset, SourcePackageExports.Size - SourcePackageExports.StartOffset);
				}

				// Save out the in-memory data as '.after.uasset'.
				{
					TUniquePtr<FArchive> DiffUAssetArchive(FileManager.CreateFileWriter(*FPaths::SetExtension(OutputFilename, TEXT(".after.") + FPaths::GetExtension(Filename))));
					DiffUAssetArchive->Serialize(DestPackageHeader.Data + DestPackageHeader.StartOffset, DestPackageHeader.Size - DestPackageHeader.StartOffset);
				}
				{
					TUniquePtr<FArchive> DiffUExpArchive(FileManager.CreateFileWriter(*FPaths::SetExtension(OutputFilename, TEXT(".after.uexp"))));
					DiffUExpArchive->Serialize(DestPackageExports.Data + DestPackageExports.StartOffset, DestPackageExports.Size - DestPackageExports.StartOffset);
				}
			}
			else
			{
				MessageCallback(ELogVerbosity::Warning,
					FString::Printf(TEXT("Package '%s' doesn't seem to be writing to the Saved directory - skipping writing diff"), *OutputFilename));
			}
		}
	}
}

void FAccumulator::GenerateDiffMapForSection(const FPackageData& SourcePackage, const FPackageData& DestPackage,
	bool& bOutSectionIdentical)
{
	FCallstacks& Callstacks = this->LinkerCallstacks;
	bool bIdentical = true;
	int32 LastDifferenceCallstackOffsetIndex = -1;
	FCallstacks::FCallstackData* DifferenceCallstackData = nullptr;

	const int64 SourceSize = SourcePackage.Size - SourcePackage.StartOffset;
	const int64 DestSize = DestPackage.Size - DestPackage.StartOffset;
	const int64 SizeToCompare = FMath::Min(SourceSize, DestSize);
	
	for (int64 LocalOffset = 0; LocalOffset < SizeToCompare; ++LocalOffset)
	{
		const int64 SourceAbsoluteOffset = LocalOffset + SourcePackage.StartOffset;
		const int64 DestAbsoluteOffset = LocalOffset + DestPackage.StartOffset;
		if (SourcePackage.Data[SourceAbsoluteOffset] != DestPackage.Data[DestAbsoluteOffset])
		{
			bIdentical = false;
			if (DiffMap.Num() < MaxDiffsToLog)
			{
				const int32 DifferenceCallstackOffsetIndex = Callstacks.GetCallstackIndexAtOffset(DestAbsoluteOffset, FMath::Max<int32>(LastDifferenceCallstackOffsetIndex, 0));
				if (DifferenceCallstackOffsetIndex >= 0 && DifferenceCallstackOffsetIndex != LastDifferenceCallstackOffsetIndex)
				{
					const FCallstacks::FCallstackAtOffset& CallstackAtOffset = Callstacks.GetCallstack(DifferenceCallstackOffsetIndex);
					if (!CallstackAtOffset.bSuppressLogging)
					{
						FDiffInfo OffsetAndSize;
						OffsetAndSize.Offset = CallstackAtOffset.Offset;
						OffsetAndSize.Size = Callstacks.GetSerializedDataSizeForOffsetIndex(DifferenceCallstackOffsetIndex);
						DiffMap.Add(OffsetAndSize);
					}
				}
				LastDifferenceCallstackOffsetIndex = DifferenceCallstackOffsetIndex;
			}
		}
	}

	if (SourceSize < DestSize)
	{
		bIdentical = false;

		// Add all the remaining callstacks to the diff map
		for (int32 OffsetIndex = LastDifferenceCallstackOffsetIndex + 1; OffsetIndex < Callstacks.Num() && DiffMap.Num() < MaxDiffsToLog; ++OffsetIndex)
		{
			const FCallstacks::FCallstackAtOffset& CallstackAtOffset = Callstacks.GetCallstack(OffsetIndex);
			// Compare against the size without start offset as all callstack offsets are absolute (from the merged header + exports file)
			if (CallstackAtOffset.Offset < DestPackage.Size)
			{
				if (!CallstackAtOffset.bSuppressLogging)
				{
					FDiffInfo OffsetAndSize;
					OffsetAndSize.Offset = CallstackAtOffset.Offset;
					OffsetAndSize.Size = Callstacks.GetSerializedDataSizeForOffsetIndex(OffsetIndex);
					DiffMap.Add(OffsetAndSize);
				}
			}
			else
			{
				break;
			}
		}
	}
	else if (SourceSize > DestSize)
	{
		bIdentical = false;
	}
	bOutSectionIdentical = bIdentical;
}

void FAccumulator::GenerateDiffMap()
{
	check(MaxDiffsToLog > 0);
	// An FDiffArchiveForLinker should have been constructed by SavePackage and should still be in memory
	check(LinkerArchive != nullptr);

	bHasDifferences = true;
	DiffMap.Reset();

	FPackageData SourcePackage;
	SourcePackage.Data = PreviousPackageData.Data.Get();
	SourcePackage.Size = PreviousPackageData.Size;
	SourcePackage.HeaderSize = PreviousPackageData.HeaderSize;
	SourcePackage.StartOffset = PreviousPackageData.StartOffset;

	bool bIdentical = true;
	bool bHeaderIdentical = true;
	bool bExportsIdentical = true;

	FPackageData DestPackage;
	DestPackage.Data = LinkerArchive->GetData();
	DestPackage.Size = LinkerArchive->TotalSize();
	DestPackage.HeaderSize = HeaderSize;
	DestPackage.StartOffset = 0;

	{
		FPackageData SourcePackageHeader = SourcePackage;
		SourcePackageHeader.Size = SourcePackage.HeaderSize;
		SourcePackageHeader.HeaderSize = 0;
		SourcePackageHeader.StartOffset = 0;

		FPackageData DestPackageHeader = DestPackage;
		DestPackageHeader.Size = HeaderSize;
		DestPackageHeader.HeaderSize = 0;
		DestPackageHeader.StartOffset = 0;

		GenerateDiffMapForSection(SourcePackageHeader, DestPackageHeader, bHeaderIdentical);
	}

	{
		FPackageData SourcePackageExports = SourcePackage;
		SourcePackageExports.HeaderSize = 0;
		SourcePackageExports.StartOffset = SourcePackage.HeaderSize;

		FPackageData DestPackageExports = DestPackage;
		DestPackageExports.HeaderSize = 0;
		DestPackageExports.StartOffset = HeaderSize;

		GenerateDiffMapForSection(SourcePackageExports, DestPackageExports, bExportsIdentical);
	}

	bIdentical = bHeaderIdentical && bExportsIdentical;
	bHasDifferences = !bIdentical;
}

FLinkerLoad* CreateLinkerForPackage(FUObjectSerializeContext* LoadContext, const FString& InPackageName, const FString& InFilename, const FPackageData& PackageData)
{
	// First create a temp package to associate the linker with
	UPackage* Package = FindObjectFast<UPackage>(nullptr, *InPackageName);
	if (!Package)
	{
		Package = CreatePackage(*InPackageName);
	}
	// Create an archive for the linker. The linker will take ownership of it.
	FLargeMemoryReader* PackageReader = new FLargeMemoryReader(PackageData.Data, PackageData.Size, ELargeMemoryReaderFlags::None, *InPackageName);	
	FLinkerLoad* Linker = FLinkerLoad::CreateLinker(LoadContext, Package, FPackagePath::FromLocalPath(InFilename), LOAD_NoVerify, PackageReader);

	if (Linker && Package)
	{
		Package->SetPackageFlags(PKG_ForDiffing);
	}

	return Linker;
}

/** Structure that holds an item from the NameMap/ImportMap/ExportMap in a TSet for diffing */
template <typename T>
struct TTableItem
{
	/** The key generated for this item */
	FString Key;
	/** Pointer to the original item */
	const T* Item;
	/** Index in the original *Map (table). Only for information purposes. */
	int32 Index;

	TTableItem(FString&& InKey, const T* InItem, int32 InIndex)
		: Key(MoveTemp(InKey))
		, Item(InItem)
		, Index(InIndex)
	{
	}

	FORCENOINLINE friend uint32 GetTypeHash(const TTableItem& TableItem)
	{
		return GetTypeHash(TableItem.Key);
	}

	FORCENOINLINE friend bool operator==(const TTableItem& Lhs, const TTableItem& Rhs)
	{
		return Lhs.Key == Rhs.Key;
	}
};

/** Dumps differences between Linker tables */
template <typename T, typename ContextProvider>
static void DumpTableDifferences(
	ContextProvider& SourceContext,
	ContextProvider& DestContext,
	TConstArrayView<T> SourceTable, 
	TConstArrayView<T> DestTable,
	const TCHAR* AssetFilename,
	const TCHAR* ItemName,
	const int32 MaxDiffsToLog
)
{
	FString HumanReadableString;
	int32 LoggedDiffs = 0;
	int32 NumDiffs = 0;

	TSet<TTableItem<T>> SourceSet;
	TSet<TTableItem<T>> DestSet;

	SourceSet.Reserve(SourceTable.Num());
	DestSet.Reserve(DestTable.Num());

	for (int32 Index = 0; Index < SourceTable.Num(); ++Index)
	{
		const T& Item = SourceTable[Index];
		SourceSet.Add(TTableItem<T>(SourceContext.GetTableKey(Item), &Item, Index));
	}
	for (int32 Index = 0; Index < DestTable.Num(); ++Index)
	{
		const T& Item = DestTable[Index];
		DestSet.Add(TTableItem<T>(DestContext.GetTableKey(Item), &Item, Index));
	}

	// Determine the list of items removed from the source package and added to the dest package
	TSet<TTableItem<T>> RemovedItems = SourceSet.Difference(DestSet);
	TSet<TTableItem<T>> AddedItems   = DestSet.Difference(SourceSet);

	// Add changed items as added-and-removed
	for (const TTableItem<T>& ChangedSourceItem : SourceSet)
	{
		if (const TTableItem<T>* ChangedDestItem = DestSet.Find(ChangedSourceItem))
		{
			if (!SourceContext.CompareTableItem(DestContext, *ChangedSourceItem.Item,
				*ChangedDestItem->Item))
			{
				RemovedItems.Add(ChangedSourceItem);
				AddedItems  .Add(*ChangedDestItem);
			}
		}
	}

	// Sort all additions and removals by index
	RemovedItems.Sort([](const TTableItem<T>& Lhs, const TTableItem<T>& Rhs){ return Lhs.Index < Rhs.Index; });
	AddedItems.  Sort([](const TTableItem<T>& Lhs, const TTableItem<T>& Rhs){ return Lhs.Index < Rhs.Index; });

	// Dump all changes
	for (const TTableItem<T>& RemovedItem : RemovedItems)
	{
		HumanReadableString += IndentToken;
		HumanReadableString += FString::Printf(TEXT("-[%d] %s"), RemovedItem.Index, *SourceContext.ConvertItemToText(*RemovedItem.Item));
		HumanReadableString += NewLineToken;
	}
	for (const TTableItem<T>& AddedItem : AddedItems)
	{
		HumanReadableString += IndentToken;
		HumanReadableString += FString::Printf(TEXT("+[%d] %s"), AddedItem.Index, *DestContext.ConvertItemToText(*AddedItem.Item));
		HumanReadableString += NewLineToken;
	}

	// For now just log everything out. When this becomes too spammy, respect the MaxDiffsToLog parameter
	NumDiffs = RemovedItems.Num() + AddedItems.Num();
	LoggedDiffs = NumDiffs;

	if (NumDiffs > LoggedDiffs)
	{
		HumanReadableString += IndentToken;
		HumanReadableString += FString::Printf(TEXT("+ %d differences not logged."), (NumDiffs - LoggedDiffs));
		HumanReadableString += NewLineToken;
	}

	SourceContext.LogMessage(ELogVerbosity::Warning, FString::Printf(
		TEXT("%s: %sMap is different (%d %ss in source package vs %d %ss in dest package):%s%s"),		
		AssetFilename,
		ItemName,
		SourceTable.Num(),
		ItemName,
		DestTable.Num(),
		ItemName,
		NewLineToken,
		*HumanReadableString));
}

void DumpPackageHeaderDiffs_LinkerLoad(
	FAccumulatorGlobals& Globals,
	const FPackageData& SourcePackage,
	const FPackageData& DestPackage,
	const FString& AssetFilename,
	const int32 MaxDiffsToLog,
	const FMessageCallback& MessageCallback)
{
	FString AssetPathName = FPaths::Combine(*FPaths::GetPath(AssetFilename.Mid(AssetFilename.Find(TEXT(":"), ESearchCase::CaseSensitive) + 1)), *FPaths::GetBaseFilename(AssetFilename));
	// The root directory could have a period in it (d:/Release5.0/EngineTest/Saved/Cooked),
	// which is not a valid character for a LongPackageName. Remove it.
	for (TCHAR c : FStringView(INVALID_LONGPACKAGE_CHARACTERS))
	{
		AssetPathName.ReplaceCharInline(c, TEXT('_'), ESearchCase::CaseSensitive);
	}
	FString SourceAssetPackageName = FPaths::Combine(TEXT("/Memory"), TEXT("/SourceForDiff"), *AssetPathName);
	FString DestAssetPackageName = FPaths::Combine(TEXT("/Memory"), TEXT("/DestForDiff"), *AssetPathName);
	check(FPackageName::IsValidLongPackageName(SourceAssetPackageName, true /* bIncludeReadOnlyRoots */));

	TGuardValue<bool> GuardIsSavingPackage(GIsSavingPackage, false);
	TGuardValue<int32> GuardAllowUnversionedContentInEditor(GAllowUnversionedContentInEditor, 1);
	TGuardValue<int32> GuardAllowCookedDataInEditorBuilds(GAllowCookedDataInEditorBuilds, 1);

	FLinkerLoad* SourceLinker = nullptr;
	FLinkerLoad* DestLinker = nullptr;
	// Create linkers. Note there's no need to clean them up here since they will be removed by the package associated with them
	{
		TRefCountPtr<FUObjectSerializeContext> LinkerLoadContext(FUObjectThreadContext::Get().GetSerializeContext());
		BeginLoad(LinkerLoadContext);
		SourceLinker = CreateLinkerForPackage(LinkerLoadContext, SourceAssetPackageName, AssetFilename, SourcePackage);
		EndLoad(SourceLinker ? SourceLinker->GetSerializeContext() : LinkerLoadContext.GetReference());
	}
	
	{
		TRefCountPtr<FUObjectSerializeContext> LinkerLoadContext(FUObjectThreadContext::Get().GetSerializeContext());
		BeginLoad(LinkerLoadContext);
		DestLinker = CreateLinkerForPackage(LinkerLoadContext, DestAssetPackageName, AssetFilename, DestPackage);
		EndLoad(DestLinker ? DestLinker->GetSerializeContext() : LinkerLoadContext.GetReference());
	}

	if (SourceLinker && DestLinker)
	{
		FDiffWriterLinkerLoadHeader SourceContext(SourceLinker, MessageCallback);
		FDiffWriterLinkerLoadHeader DestContext(DestLinker, MessageCallback);

		if (SourceLinker->NameMap != DestLinker->NameMap)
		{
			DumpTableDifferences<FNameEntryId>(SourceContext, DestContext, SourceLinker->NameMap, DestLinker->NameMap,
				*AssetFilename, TEXT("Name"), MaxDiffsToLog);
		}

		if (!SourceContext.IsImportMapIdentical(DestContext))
		{
			DumpTableDifferences<FObjectImport>(SourceContext, DestContext, SourceLinker->ImportMap, DestLinker->ImportMap,
				*AssetFilename, TEXT("Import"), MaxDiffsToLog);
		}

		if (!SourceContext.IsExportMapIdentical(DestContext))
		{
			DumpTableDifferences<FObjectExport>(SourceContext, DestContext, SourceLinker->ExportMap, DestLinker->ExportMap,
				*AssetFilename, TEXT("Export"), MaxDiffsToLog);
		}
	}

	if (SourceLinker)
	{
		UE::ArchiveStackTrace::ForceKillPackageAndLinker(SourceLinker);
	}
	if (DestLinker)
	{
		UE::ArchiveStackTrace::ForceKillPackageAndLinker(DestLinker);
	}
}

void DumpPackageHeaderDiffs_ZenPackage(
	FAccumulatorGlobals& Globals,
	const FPackageData& SourcePackage,
	const FPackageData& DestPackage,
	const FString& AssetFilename,
	const int32 MaxDiffsToLog,
	const FMessageCallback& MessageCallback)
{
	FDiffWriterZenHeader SourceHeader(Globals, MessageCallback, true /* bUseZenStore */, SourcePackage, AssetFilename,
		TEXT("source"));
	FDiffWriterZenHeader DestHeader(Globals, MessageCallback, false /* bUseZenStore */, DestPackage, AssetFilename,
		TEXT("dest"));
	if (!SourceHeader.IsValid() || !DestHeader.IsValid())
	{
		// Explanation was logged by TryConstructSummary
		return;
	}

	TArray<FString> SourceNames;
	TArray<FString> DestNames;
	for (FDisplayNameEntryId Id : SourceHeader.GetPackageHeader().NameMap)
	{
		SourceNames.Add(Id.ToName(0).ToString());
	}
	for (FDisplayNameEntryId Id : DestHeader.GetPackageHeader().NameMap)
	{
		DestNames.Add(Id.ToName(0).ToString());
	}
	auto StringSortByNoCaseThenCase = [](const FString& A, const FString& B)
	{
		int32 NoCase = A.Compare(B, ESearchCase::IgnoreCase);
		if (NoCase != 0)
		{
			return NoCase < 0;
		}
		int32 Case = A.Compare(B, ESearchCase::CaseSensitive);
		return Case < 0;
	};
	Algo::Sort(SourceNames, StringSortByNoCaseThenCase);
	Algo::Sort(DestNames, StringSortByNoCaseThenCase);

	if (!SourceHeader.IsNameMapIdentical(DestHeader, SourceNames, DestNames))
	{
		DumpTableDifferences<FString>(SourceHeader, DestHeader, SourceNames, DestNames,
			*AssetFilename, TEXT("Name"), MaxDiffsToLog);
	}

	if (!SourceHeader.IsImportMapIdentical(DestHeader))
	{
		DumpTableDifferences<FPackageObjectIndex>(SourceHeader, DestHeader, SourceHeader.GetPackageHeader().ImportMap,
			DestHeader.GetPackageHeader().ImportMap, *AssetFilename, TEXT("Import"), MaxDiffsToLog);
	}

	if (!SourceHeader.IsExportMapIdentical(DestHeader))
	{
		int32 SourceNum = SourceHeader.GetPackageHeader().ExportMap.Num();
		int32 DestNum = DestHeader.GetPackageHeader().ExportMap.Num();
		TArray<FZenHeaderIndexIntoExportMap> SourceIndices;
		SourceIndices.Reserve(SourceNum);
		for (int N = 0; N < SourceNum; ++N)
		{
			SourceIndices.Add(FZenHeaderIndexIntoExportMap{ N });
		}
		TArray<FZenHeaderIndexIntoExportMap> DestIndices;
		DestIndices.Reserve(DestNum);
		for (int N = 0; N < DestNum; ++N)
		{
			DestIndices.Add(FZenHeaderIndexIntoExportMap{ N });
		}
		DumpTableDifferences<FZenHeaderIndexIntoExportMap>(SourceHeader, DestHeader, SourceIndices, DestIndices,
			*AssetFilename, TEXT("Export"), MaxDiffsToLog);
	}
}

void DumpPackageHeaderDiffs(
	FAccumulatorGlobals& Globals,
	const FPackageData& SourcePackage,
	const FPackageData& DestPackage,
	const FString& AssetFilename,
	const int32 MaxDiffsToLog,
	const EPackageHeaderFormat PackageHeaderFormat,
	const FMessageCallback& MessageCallback)
{
	switch (PackageHeaderFormat)
	{
	case EPackageHeaderFormat::PackageFileSummary:
		DumpPackageHeaderDiffs_LinkerLoad(Globals, SourcePackage, DestPackage, AssetFilename, MaxDiffsToLog, MessageCallback);
		break;
	case EPackageHeaderFormat::ZenPackageSummary:
		DumpPackageHeaderDiffs_ZenPackage(Globals, SourcePackage, DestPackage, AssetFilename, MaxDiffsToLog, MessageCallback);
		break;
	default:
		unimplemented();
	}
}

} // namespace UE::DiffWriter