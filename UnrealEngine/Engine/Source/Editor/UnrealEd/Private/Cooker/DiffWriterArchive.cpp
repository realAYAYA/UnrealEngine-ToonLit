// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cooker/DiffWriterArchive.h"

#include "Compression/CompressionUtil.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformStackWalk.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/StaticMemoryReader.h"
#include "UObject/LinkerLoad.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Package.h"
#include "UObject/PropertyTempVal.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectThreadContext.h"

static const ANSICHAR* DebugDataStackMarker = "\r\nDebugDataStack:\r\n";
namespace UE::DiffWriterArchive
{

const TCHAR* const IndentToken = TEXT("%DWA%    ");
const TCHAR* const NewLineToken = TEXT("%DWA%\n");

}

FDiffWriterCallstacks::FCallstackData::FCallstackData(TUniquePtr<ANSICHAR[]>&& InCallstack, UObject* InSerializedObject, FProperty* InSerializedProperty)
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

FString FDiffWriterCallstacks::FCallstackData::ToString(const TCHAR* CallstackCutoffText) const
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
				StackLine.MidInline(CutoffIndex + 1, MAX_int32, false);
			}
		}
		HumanReadableString += UE::DiffWriterArchive::IndentToken;
		HumanReadableString += StackLine;
		HumanReadableString += UE::DiffWriterArchive::NewLineToken;
	}

	if (!SerializedObjectName.IsEmpty())
	{
		HumanReadableString += UE::DiffWriterArchive::NewLineToken;
		HumanReadableString += UE::DiffWriterArchive::IndentToken;
		HumanReadableString += TEXT("Serialized Object: ");
		HumanReadableString += SerializedObjectName;
		HumanReadableString += UE::DiffWriterArchive::NewLineToken;
	}
	if (!SerializedPropertyName.IsEmpty())
	{
		if (SerializedObjectName.IsEmpty())
		{
			HumanReadableString += UE::DiffWriterArchive::NewLineToken;
		}
		HumanReadableString += UE::DiffWriterArchive::IndentToken;
		HumanReadableString += TEXT("Serialized Property: ");
		HumanReadableString += SerializedPropertyName;
		HumanReadableString += UE::DiffWriterArchive::NewLineToken;
	}
	return HumanReadableString;
}

FDiffWriterCallstacks::FCallstackData FDiffWriterCallstacks::FCallstackData::Clone() const
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

FDiffWriterCallstacks::FDiffWriterCallstacks(UObject* InAsset)
	: Asset(InAsset)
	, bCallstacksDirty(true)
	, StackTraceSize(65535)
	, LastSerializeCallstack(nullptr)
	, TotalSize(0)
{
	StackTrace = MakeUnique<ANSICHAR[]>(StackTraceSize);
	StackTrace[0] = 0;
}

FName FDiffWriterCallstacks::GetAssetClass() const
{
	return Asset != nullptr ? Asset->GetClass()->GetFName() : NAME_None;
}

ANSICHAR* FDiffWriterCallstacks::AddUniqueCallstack(bool bIsCollectingCallstacks, UObject* SerializedObject, FProperty* SerializedProperty, uint32& OutCallstackCRC)
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

void FDiffWriterCallstacks::Add(
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

	const int64 CurrentOffset = Offset;
	TotalSize = FMath::Max(TotalSize, CurrentOffset + Length); 

	const bool bShouldCollectCallstack = bIsCollectingCallstacks && bCollectCurrentCallstack && !UE::ArchiveStackTrace::ShouldIgnoreDiff();
	if (bShouldCollectCallstack)
	{
		StackTrace[0] = '\0';
		FPlatformStackWalk::StackWalkAndDump(StackTrace.Get(), StackTraceSize, StackIgnoreCount);
		//if we have a debug name stack, plaster it onto the end of the current stack buffer so that it's a part of the unique stack entry.
		if (DebugDataStack.Num() > 0)
		{
			FCStringAnsi::Strcat(StackTrace.Get(), StackTraceSize, DebugDataStackMarker);

			const FString SubIndent = FString(UE::DiffWriterArchive::IndentToken) + FString(TEXT("    "));

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
		if (CallstackAtOffsetMap.Num() == 0 || CurrentOffset > CallstackAtOffsetMap.Last().Offset)
		{
			// New data serialized at the end of archive buffer
			LastSerializeCallstack = AddUniqueCallstack(bIsCollectingCallstacks, SerializedObject, SerializedProperty, CallstackCRC);
			CallstackAtOffsetMap.Add(FCallstackAtOffset {CurrentOffset, CallstackCRC, UE::ArchiveStackTrace::ShouldIgnoreDiff()});
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
				CallstackAtOffsetMap.Insert(FCallstackAtOffset {CurrentOffset, CallstackCRC, UE::ArchiveStackTrace::ShouldIgnoreDiff()}, CallstackToUpdateIndex + 1);
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

int32 FDiffWriterCallstacks::GetCallstackIndexAtOffset(int64 Offset, int32 MinOffsetIndex) const
{
	if (Offset < 0 || Offset > TotalSize || MinOffsetIndex < 0 || MinOffsetIndex >= CallstackAtOffsetMap.Num())
	{
		return -1;
	}

	// Find the index of the offset the InOffset maps to
	int32 OffsetForCallstackIndex = -1;
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
		check(OffsetForCallstackIndex != -1);
		check(CallstackAtOffsetMap[OffsetForCallstackIndex].Offset < Offset);
		check(OffsetForCallstackIndex == (CallstackAtOffsetMap.Num() - 1) || CallstackAtOffsetMap[OffsetForCallstackIndex + 1].Offset > Offset);
	}

	return OffsetForCallstackIndex;
}

void FDiffWriterCallstacks::Append(const FDiffWriterCallstacks& Other, int64 Offset)
{
	for (const FCallstackAtOffset& OtherOffset : Other.CallstackAtOffsetMap)
	{
		FCallstackAtOffset& New = CallstackAtOffsetMap.Add_GetRef(OtherOffset);
		New.Offset += Offset;
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

	TotalSize = FMath::Max(TotalSize, Other.TotalSize);
}

FDiffWriterArchiveWriter::FDiffWriterArchiveWriter(
	FArchive& InInner,
	FDiffWriterCallstacks& InCallstacks,
	const FDiffWriterDiffMap* InDiffMap,
	int64 InDiffMapOffset)
		: FArchiveProxy(InInner)
		, Callstacks(InCallstacks)
		, DiffMap(InDiffMap)
		, DiffMapOffset(InDiffMapOffset)
		, bInnerArchiveDisabled(false)
{
}

FDiffWriterArchiveWriter::~FDiffWriterArchiveWriter() = default;

void FDiffWriterArchiveWriter::Serialize(void* Data, int64 Length)
{
	static struct FBreakAtOffsetSettings
	{
		FString PackageToBreakOn;
		int64 OffsetToBreakOn;

		FBreakAtOffsetSettings()
			: OffsetToBreakOn(-1)
		{
			if (!FParse::Param(FCommandLine::Get(), TEXT("cooksinglepackage")))
			{
				return;
			}

			FString Package;
			if (!FParse::Value(FCommandLine::Get(), TEXT("map="), Package))
			{
				return;
			}

			int64 Offset;
			if (!FParse::Value(FCommandLine::Get(), TEXT("diffonlybreakoffset="), Offset) || Offset <= 0)
			{
				return;
			}

			OffsetToBreakOn = Offset;
			PackageToBreakOn = TEXT("/") + FPackageName::GetShortName(Package);
		}
	} BreakAtOffsetSettings;

	const int64 CurrentOffset = DiffMapOffset + Tell();

	if (BreakAtOffsetSettings.OffsetToBreakOn >= 0 && BreakAtOffsetSettings.OffsetToBreakOn >= CurrentOffset && BreakAtOffsetSettings.OffsetToBreakOn < CurrentOffset + Length)
	{
		FString ArcName = GetArchiveName();
		int32 SubnameIndex = ArcName.Find(BreakAtOffsetSettings.PackageToBreakOn, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (SubnameIndex >= 0)
		{
			int32 SubnameEndIndex = SubnameIndex + BreakAtOffsetSettings.PackageToBreakOn.Len();
			if (SubnameEndIndex == ArcName.Len() || ArcName[SubnameEndIndex] == TEXT('.'))
			{
				UE_DEBUG_BREAK();
			}
		}
	}

	if (Length > 0)
	{
		UObject* SerializedObject = SerializeContext ? SerializeContext->SerializedObject : nullptr;
		TArrayView<const FName> DebugStack;
		DebugStack = DebugDataStack;

		const bool bIsCollectingCallstacks = DiffMap != nullptr;
		const bool bCollectCurrentCallstack = bIsCollectingCallstacks && DiffMap->ContainsOffset(CurrentOffset);

		Callstacks.Add(
			CurrentOffset,
			Length,
			SerializedObject,
			GetSerializedProperty(),
			DebugStack,
			bIsCollectingCallstacks,
			bCollectCurrentCallstack,
			StackIgnoreCount);
	}

	if (bInnerArchiveDisabled == false)
	{
		InnerArchive.Serialize(Data, Length);
	}
}

void FDiffWriterArchiveWriter::SetSerializeContext(FUObjectSerializeContext* Context)
{
	SerializeContext = Context;

	if (bInnerArchiveDisabled == false)
	{
		InnerArchive.SetSerializeContext(Context);
	}
}

FUObjectSerializeContext* FDiffWriterArchiveWriter::GetSerializeContext()
{
	return SerializeContext;
}

FDiffWriterArchiveMemoryWriter::FDiffWriterArchiveMemoryWriter(
	FDiffWriterCallstacks& Callstacks,
	const FDiffWriterDiffMap* DiffMap,
	const int64 DiffMapOffset,
	const int64 PreAllocateBytes,
	bool bIsPersistent,
	const TCHAR* Filename)
		: FLargeMemoryWriter(PreAllocateBytes, bIsPersistent, Filename)
		, StackTraceWriter(*this, Callstacks, DiffMap, DiffMapOffset)
{
	// Hack to prevent recursive calls to serialize when passing in this.
	StackTraceWriter.SetDisableInnerArchive(true);
	StackTraceWriter.SetStackIgnoreCount(StackTraceWriter.GetStackIgnoreCount() + 1);
}

void FDiffWriterArchiveMemoryWriter::Serialize(void* Memory, int64 Length)
{
	StackTraceWriter.Serialize(Memory, Length);
	FLargeMemoryWriter::Serialize(Memory, Length);
}

void FDiffWriterArchiveMemoryWriter::SetSerializeContext(FUObjectSerializeContext* Context)
{
	StackTraceWriter.SetSerializeContext(Context);
}

FUObjectSerializeContext* FDiffWriterArchiveMemoryWriter::GetSerializeContext()
{
	return StackTraceWriter.GetSerializeContext();
}

FDiffWriterArchive::FDiffWriterArchive(UObject* InAsset, const TCHAR* InFilename, UE::DiffWriterArchive::FMessageCallback&& InMessageCallback,
	bool bInCollectCallstacks, const FDiffWriterDiffMap* InDiffMap)
	: FLargeMemoryWriter(0, false, InFilename)
	, Callstacks(InAsset)
	, StackTraceWriter(*this, Callstacks, InDiffMap, 0)
	, MessageCallback(MoveTemp(InMessageCallback))
{
	// Hack to prevent recursive calls to serialize when passing in this.
	StackTraceWriter.SetDisableInnerArchive(true);
	StackTraceWriter.SetStackIgnoreCount(StackTraceWriter.GetStackIgnoreCount() + 1);
}

FDiffWriterArchive::~FDiffWriterArchive()
{
}

void FDiffWriterArchive::Serialize(void* Memory, int64 Length)
{
	StackTraceWriter.Serialize(Memory, Length);
	FLargeMemoryWriter::Serialize(Memory, Length);
}

void FDiffWriterArchive::SetSerializeContext(FUObjectSerializeContext* Context)
{
	StackTraceWriter.SetSerializeContext(Context);
}

FUObjectSerializeContext* FDiffWriterArchive::GetSerializeContext()
{
	return StackTraceWriter.GetSerializeContext();
}

namespace
{
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

		return false;
	}
}

void FDiffWriterArchiveWriter::Compare(
	const FPackageData& SourcePackage,
	const FPackageData& DestPackage,
	const FDiffWriterCallstacks& Callstacks,
	const FDiffWriterDiffMap& DiffMap,
	const TCHAR* AssetFilename,
	const TCHAR* CallstackCutoffText,
	const int64 MaxDiffsToLog,
	int32& InOutDiffsLogged,
	TMap<FName, FArchiveDiffStats>& OutStats,
	const UE::DiffWriterArchive::FMessageCallback& MessageCallback,
	bool bSuppressLogging)
{
	const int64 SourceSize = SourcePackage.Size - SourcePackage.StartOffset;
	const int64 DestSize = DestPackage.Size - DestPackage.StartOffset;
	const int64 SizeToCompare = FMath::Min(SourceSize, DestSize);
	const FName AssetClass = Callstacks.GetAssetClass();
	
	if (SourceSize != DestSize)
	{
		if (!bSuppressLogging)
		{
			MessageCallback(ELogVerbosity::Warning, FString::Printf(
				TEXT("%s: Size mismatch: on disk: %lld vs memory: %lld"), AssetFilename, SourceSize, DestSize));
		}
		int64 SizeDiff = DestPackage.Size - SourcePackage.Size;
		OutStats.FindOrAdd(AssetClass).DiffSize += SizeDiff;
	}

	FString LastDifferenceCallstackDataText;
	int32 LastDifferenceCallstackOffsetIndex = -1;
	int64 NumDiffsLocal = 0;
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

		bool bDifferenceLogged = false;
		ON_SCOPE_EXIT
		{
			if (bDifferenceLogged)
			{
				InOutDiffsLogged++;
				NumDiffsLoggedLocal++;
			}
		};

		if (DiffMap.ContainsOffset(DestAbsoluteOffset))
		{
			int32 DifferenceCallstackoffsetIndex = Callstacks.GetCallstackIndexAtOffset(DestAbsoluteOffset, FMath::Max(LastDifferenceCallstackOffsetIndex, 0));
			ON_SCOPE_EXIT
			{
				LastDifferenceCallstackOffsetIndex = DifferenceCallstackoffsetIndex;
			};

			if (DifferenceCallstackoffsetIndex < 0)
			{
				if (!bSuppressLogging)
				{
					MessageCallback(ELogVerbosity::Warning, FString::Printf(
						TEXT("%s: Difference at offset %lld (absolute offset: %lld), unknown callstack"), AssetFilename, LocalOffset, DestAbsoluteOffset));
				}
				continue;
			}

			if (DifferenceCallstackoffsetIndex == LastDifferenceCallstackOffsetIndex)
			{
				continue;
			}

			const FDiffWriterCallstacks::FCallstackAtOffset& CallstackAtOffset = Callstacks.GetCallstack(DifferenceCallstackoffsetIndex);
			const FDiffWriterCallstacks::FCallstackData& DifferenceCallstackData = Callstacks.GetCallstackData(CallstackAtOffset);
			FString DifferenceCallstackDataText = DifferenceCallstackData.ToString(CallstackCutoffText);
			if (LastDifferenceCallstackDataText.Compare(DifferenceCallstackDataText, ESearchCase::CaseSensitive) == 0)
			{
				continue;
			}
			ON_SCOPE_EXIT
			{
				LastDifferenceCallstackDataText = MoveTemp(DifferenceCallstackDataText);
			};

			if (!CallstackAtOffset.bIgnore && (MaxDiffsToLog < 0 || InOutDiffsLogged < MaxDiffsToLog))
			{
				FString BeforePropertyVal;
				FString AfterPropertyVal;
				if (FProperty* SerProp = DifferenceCallstackData.SerializedProp)
				{
					if (SourceSize == DestSize && ShouldDumpPropertyValueState(SerProp))
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
							const FDiffWriterCallstacks::FCallstackAtOffset& PreviousCallstack = Callstacks.GetCallstack(CallstackIndex);
							const FDiffWriterCallstacks::FCallstackData& PreviousCallstackData = Callstacks.GetCallstackData(PreviousCallstack);
							if (PreviousCallstackData.SerializedProp != SerProp)
							{
								break;
							}

							--OffsetX;
						}

						FPropertyTempVal SourceVal(SerProp);
						FPropertyTempVal DestVal  (SerProp);

						FStaticMemoryReader SourceReader(&SourcePackage.Data[SourceAbsoluteOffset - (DestAbsoluteOffset - OffsetX)], SourcePackage.Size - SourceAbsoluteOffset);
						FStaticMemoryReader DestReader(&DestPackage.Data[OffsetX], DestPackage.Size - DestAbsoluteOffset);

						SourceVal.Serialize(SourceReader);
						DestVal  .Serialize(DestReader);

									if (!SourceReader.IsError() && !DestReader.IsError())
									{
										SourceVal.ExportText(BeforePropertyVal);
										DestVal  .ExportText(AfterPropertyVal);
									}
								}
							}

				FString DiffValues;
				if (BeforePropertyVal != AfterPropertyVal)
				{
					DiffValues = FString::Printf(TEXT("\r\n%sBefore: %s\r\n%sAfter:  %s"),
						UE::DiffWriterArchive::IndentToken, *BeforePropertyVal,
						UE::DiffWriterArchive::IndentToken, *AfterPropertyVal);
				}

				FString DebugDataStackText;
				//check for a debug data stack as part of the unique stack entry, and log it out if we find it.
				FString FullStackText = DifferenceCallstackData.Callstack.Get();
				int32 DebugDataIndex = FullStackText.Find(ANSI_TO_TCHAR(DebugDataStackMarker), ESearchCase::CaseSensitive);
				if (DebugDataIndex > 0)
				{
					DebugDataStackText = FString::Printf(TEXT("\r\n%s"),
						UE::DiffWriterArchive::IndentToken)
						+ FullStackText.RightChop(DebugDataIndex + 2);
				}

				if (!bSuppressLogging)
				{
					MessageCallback(ELogVerbosity::Warning, FString::Printf(
						TEXT("%s: Difference at offset %lld%s (absolute offset: %lld): byte %d on disk, byte %d in memory, callstack:%s%s%s%s%s"),
						AssetFilename,
						CallstackAtOffset.Offset - DestPackage.StartOffset,
						DestAbsoluteOffset > CallstackAtOffset.Offset ? *FString::Printf(TEXT("(+%lld)"), DestAbsoluteOffset - CallstackAtOffset.Offset) : TEXT(""),
						DestAbsoluteOffset,
						SourceByte, DestByte,
						UE::DiffWriterArchive::NewLineToken,
						UE::DiffWriterArchive::NewLineToken,
						*DifferenceCallstackDataText,
						*DiffValues,
						*DebugDataStackText
					));
				}

				const int BytesToLog = 128;
				if (!bSuppressLogging)
				{
					MessageCallback(ELogVerbosity::Display, FString::Printf(
						TEXT("%s: Logging %d bytes around absolute offset: %lld (%016X) in the on disk (existing) package, (which corresponds to offset %lld (%016X) in the in-memory package)"),
						AssetFilename,
						BytesToLog,
						SourceAbsoluteOffset,
						SourceAbsoluteOffset,
						DestAbsoluteOffset,
						DestAbsoluteOffset
					));
				}
				FCompressionUtil::LogHexDump(SourcePackage.Data, SourcePackage.Size, SourceAbsoluteOffset - BytesToLog / 2, SourceAbsoluteOffset + BytesToLog / 2);

				if (!bSuppressLogging)
				{
					MessageCallback(ELogVerbosity::Display, FString::Printf(
						TEXT("%s: Logging %d bytes around absolute offset: %lld (%016X) in the in memory (new) package"),
						AssetFilename,
						BytesToLog,
						DestAbsoluteOffset,
						DestAbsoluteOffset
					));
				}
				FCompressionUtil::LogHexDump(DestPackage.Data, DestPackage.Size, DestAbsoluteOffset - BytesToLog / 2, DestAbsoluteOffset + BytesToLog / 2);

				bDifferenceLogged = true;
			}
			else if (FirstUnreportedDiffIndex == -1)
			{
				FirstUnreportedDiffIndex = DestAbsoluteOffset;
			}
			OutStats.FindOrAdd(AssetClass).NumDiffs++;
			NumDiffsLocal++;
		}
		else
		{
			// Each byte will count as a difference but without callstack data there's no way around it
			OutStats.FindOrAdd(AssetClass).NumDiffs++;
			NumDiffsLocal++;
			if (FirstUnreportedDiffIndex == -1)
			{
				FirstUnreportedDiffIndex = DestAbsoluteOffset;
			}
		}
		OutStats.FindOrAdd(AssetClass).DiffSize++;
	}

	if (MaxDiffsToLog >= 0 && NumDiffsLocal > NumDiffsLoggedLocal)
	{
		if (FirstUnreportedDiffIndex != -1)
		{
			if (!bSuppressLogging)
			{
				MessageCallback(ELogVerbosity::Warning, FString::Printf(
					TEXT("%s: %lld difference(s) not logged (first at absolute offset: %lld)."),
					AssetFilename, NumDiffsLocal - NumDiffsLoggedLocal, FirstUnreportedDiffIndex));
			}
		}
		else
		{
			if (!bSuppressLogging)
			{
				MessageCallback(ELogVerbosity::Warning, FString::Printf(
					TEXT("%s: %lld difference(s) not logged."), AssetFilename, NumDiffsLocal - NumDiffsLoggedLocal));
			}
		}
	}
}

void FDiffWriterArchive::CompareWith(const TCHAR* InFilename, const int64 TotalHeaderSize, const TCHAR* CallstackCutoffText, const int32 MaxDiffsToLog, TMap<FName, FArchiveDiffStats>& OutStats)
{
	TUniquePtr<uint8> SourcePackageBytes;
	FPackageData SourcePackage;
	UE::ArchiveStackTrace::LoadPackageIntoMemory(InFilename, SourcePackage, SourcePackageBytes);
	CompareWith(SourcePackage, InFilename, TotalHeaderSize, CallstackCutoffText, MaxDiffsToLog, OutStats);
}

void FDiffWriterArchive::CompareWith(const FPackageData& SourcePackage, const TCHAR* FileDisplayName, const int64 TotalHeaderSize,
	const TCHAR* CallstackCutoffText, const int32 MaxDiffsToLog, TMap<FName, FArchiveDiffStats>&OutStats,
	const FDiffWriterArchiveWriter::EPackageHeaderFormat PackageHeaderFormat /* = FDiffWriterArchiveWriter::EPackageHeaderFormat::PackageFileSummary */)
{
	const FName AssetClass = Callstacks.GetAssetClass();
	OutStats.FindOrAdd(AssetClass).NewFileTotalSize = TotalSize();
	if (SourcePackage.Size == 0)
	{
		MessageCallback(ELogVerbosity::Warning, FString::Printf(TEXT("New package: %s"), *GetArchiveName()));
		OutStats.FindOrAdd(AssetClass).DiffSize = OutStats.FindOrAdd(AssetClass).NewFileTotalSize;
		return;
	}

	FPackageData DestPackage;
	DestPackage.Data = GetData();
	DestPackage.Size = TotalSize();
	DestPackage.HeaderSize = TotalHeaderSize;
	DestPackage.StartOffset = 0;

	MessageCallback(ELogVerbosity::Display, FString::Printf(TEXT("Comparing: %s"), *GetArchiveName()));
	MessageCallback(ELogVerbosity::Warning, FString::Printf(TEXT("Asset class: %s"), *AssetClass.ToString()));

	int32 NumLoggedDiffs = 0;

	FPackageData SourcePackageHeader = SourcePackage;
	SourcePackageHeader.Size = SourcePackageHeader.HeaderSize;
	SourcePackageHeader.HeaderSize = 0;
	SourcePackageHeader.StartOffset = 0;

	FPackageData DestPackageHeader = DestPackage;
	DestPackageHeader.Size = TotalHeaderSize;
	DestPackageHeader.HeaderSize = 0;
	DestPackageHeader.StartOffset = 0;

	FDiffWriterArchiveWriter::Compare(SourcePackageHeader, DestPackageHeader, Callstacks,
		StackTraceWriter.GetDiffMap(), FileDisplayName, CallstackCutoffText, MaxDiffsToLog, NumLoggedDiffs,
		OutStats, MessageCallback);

	if (TotalHeaderSize > 0 && OutStats.FindOrAdd(AssetClass).NumDiffs > 0)
	{
		FDiffWriterArchiveWriter::DumpPackageHeaderDiffs(SourcePackage, DestPackage, FileDisplayName, MaxDiffsToLog,
			PackageHeaderFormat, MessageCallback);
	}

	FPackageData SourcePackageExports = SourcePackage;
	SourcePackageExports.HeaderSize = 0;
	SourcePackageExports.StartOffset = SourcePackage.HeaderSize;

	FPackageData DestPackageExports = DestPackage;
	DestPackageExports.HeaderSize = 0;
	DestPackageExports.StartOffset = TotalHeaderSize;

	FString AssetName;
	if (DestPackage.HeaderSize > 0)
	{
		AssetName = FPaths::ChangeExtension(FileDisplayName, TEXT("uexp"));
	}
	else
	{
		AssetName = FileDisplayName;
	}

	FDiffWriterArchiveWriter::Compare(SourcePackageExports, DestPackageExports, Callstacks,
		StackTraceWriter.GetDiffMap(), *AssetName, CallstackCutoffText, MaxDiffsToLog, NumLoggedDiffs,
		OutStats, MessageCallback);

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
			FString OutputFilename = FPaths::ConvertRelativePathToFull(FileDisplayName);
			FString SavedDir       = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
			if (OutputFilename.StartsWith(SavedDir))
			{
				OutputFilename.ReplaceInline(*SavedDir, *DiffOutputSettings.DiffOutputDir);

				IFileManager& FileManager = IFileManager::Get();

				// Copy the original asset as '.before.uasset'.
				{
					TUniquePtr<FArchive> DiffUAssetArchive(FileManager.CreateFileWriter(*FPaths::SetExtension(OutputFilename, TEXT(".before.") + FPaths::GetExtension(FileDisplayName))));
					DiffUAssetArchive->Serialize(SourcePackageHeader.Data + SourcePackageHeader.StartOffset, SourcePackageHeader.Size - SourcePackageHeader.StartOffset);
				}
				{
					TUniquePtr<FArchive> DiffUExpArchive(FileManager.CreateFileWriter(*FPaths::SetExtension(OutputFilename, TEXT(".before.uexp"))));
					DiffUExpArchive->Serialize(SourcePackageExports.Data + SourcePackageExports.StartOffset, SourcePackageExports.Size - SourcePackageExports.StartOffset);
				}

				// Save out the in-memory data as '.after.uasset'.
				{
					TUniquePtr<FArchive> DiffUAssetArchive(FileManager.CreateFileWriter(*FPaths::SetExtension(OutputFilename, TEXT(".after.") + FPaths::GetExtension(FileDisplayName))));
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

bool FDiffWriterArchiveWriter::GenerateDiffMap(const FPackageData& SourcePackage, const FPackageData& DestPackage, const FDiffWriterCallstacks& Callstacks, int32 MaxDiffsToFind, FDiffWriterDiffMap& OutDiffMap)
{
	bool bIdentical = true;
	int32 LastDifferenceCallstackOffsetIndex = -1;
	FDiffWriterCallstacks::FCallstackData* DifferenceCallstackData = nullptr;

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
			if (OutDiffMap.Num() < MaxDiffsToFind)
			{
				const int32 DifferenceCallstackOffsetIndex = Callstacks.GetCallstackIndexAtOffset(DestAbsoluteOffset, FMath::Max<int32>(LastDifferenceCallstackOffsetIndex, 0));
				if (DifferenceCallstackOffsetIndex >= 0 && DifferenceCallstackOffsetIndex != LastDifferenceCallstackOffsetIndex)
				{
					const FDiffWriterCallstacks::FCallstackAtOffset& CallstackAtOffset = Callstacks.GetCallstack(DifferenceCallstackOffsetIndex);
					if (!CallstackAtOffset.bIgnore)
					{
						FDiffWriterDiffInfo OffsetAndSize;
						OffsetAndSize.Offset = CallstackAtOffset.Offset;
						OffsetAndSize.Size = Callstacks.GetSerializedDataSizeForOffsetIndex(DifferenceCallstackOffsetIndex);
						OutDiffMap.Add(OffsetAndSize);
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
		for (int32 OffsetIndex = LastDifferenceCallstackOffsetIndex + 1; OffsetIndex < Callstacks.Num() && OutDiffMap.Num() < MaxDiffsToFind; ++OffsetIndex)
		{
			const FDiffWriterCallstacks::FCallstackAtOffset& CallstackAtOffset = Callstacks.GetCallstack(OffsetIndex);
			// Compare against the size without start offset as all callstack offsets are absolute (from the merged header + exports file)
			if (CallstackAtOffset.Offset < DestPackage.Size)
			{
				if (!CallstackAtOffset.bIgnore)
				{
					FDiffWriterDiffInfo OffsetAndSize;
					OffsetAndSize.Offset = CallstackAtOffset.Offset;
					OffsetAndSize.Size = Callstacks.GetSerializedDataSizeForOffsetIndex(OffsetIndex);
					OutDiffMap.Add(OffsetAndSize);
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
	return bIdentical;
}

bool FDiffWriterArchive::GenerateDiffMap(const TCHAR* InFilename, int64 TotalHeaderSize, int32 MaxDiffsToFind, FDiffWriterDiffMap& OutDiffMap)
{
	TUniquePtr<uint8> SourcePackageBytes;
	FPackageData SourcePackage;
	if (!UE::ArchiveStackTrace::LoadPackageIntoMemory(InFilename, SourcePackage, SourcePackageBytes))
	{
		return false;
	}
	return GenerateDiffMap(SourcePackage, TotalHeaderSize, MaxDiffsToFind, OutDiffMap);
}

bool FDiffWriterArchive::GenerateDiffMap(const FPackageData& SourcePackage, int64 TotalHeaderSize, int32 MaxDiffsToFind, FDiffWriterDiffMap& OutDiffMap)
{
	check(MaxDiffsToFind > 0);

	bool bIdentical = true;
	bool bHeaderIdentical = true;
	bool bExportsIdentical = true;

	FPackageData DestPackage;
	DestPackage.Data = GetData();
	DestPackage.Size = TotalSize();
	DestPackage.HeaderSize = TotalHeaderSize;
	DestPackage.StartOffset = 0;

	{
		FPackageData SourcePackageHeader = SourcePackage;
		SourcePackageHeader.Size = SourcePackageHeader.HeaderSize;
		SourcePackageHeader.HeaderSize = 0;
		SourcePackageHeader.StartOffset = 0;

		FPackageData DestPackageHeader = DestPackage;
		DestPackageHeader.Size = TotalHeaderSize;
		DestPackageHeader.HeaderSize = 0;
		DestPackageHeader.StartOffset = 0;

		bHeaderIdentical = FDiffWriterArchiveWriter::GenerateDiffMap(SourcePackageHeader, DestPackageHeader, Callstacks, MaxDiffsToFind, OutDiffMap);
	}

	{
		FPackageData SourcePackageExports = SourcePackage;
		SourcePackageExports.HeaderSize = 0;
		SourcePackageExports.StartOffset = SourcePackage.HeaderSize;

		FPackageData DestPackageExports = DestPackage;
		DestPackageExports.HeaderSize = 0;
		DestPackageExports.StartOffset = TotalHeaderSize;

		bExportsIdentical = FDiffWriterArchiveWriter::GenerateDiffMap(SourcePackageExports, DestPackageExports, Callstacks, MaxDiffsToFind, OutDiffMap);
	}

	bIdentical = bHeaderIdentical && bExportsIdentical;

	return bIdentical;
}

bool FDiffWriterArchive::IsIdentical(const TCHAR* InFilename, int64 BufferSize, const uint8* BufferData)
{
	TUniquePtr<uint8> SourcePackageBytes;
	FPackageData SourcePackage;
	if (!UE::ArchiveStackTrace::LoadPackageIntoMemory(InFilename, SourcePackage, SourcePackageBytes))
	{
		return false;
	}

	return IsIdentical(SourcePackage, BufferSize, BufferData);
}

bool FDiffWriterArchive::IsIdentical(const FPackageData& SourcePackage, int64 BufferSize, const uint8* BufferData)
{
	bool bIdentical = false;
	if (BufferSize == SourcePackage.Size)
	{
		bIdentical = (FMemory::Memcmp(SourcePackage.Data, BufferData, BufferSize) == 0);
	}
	else
	{
		bIdentical = false;
	}
	return bIdentical;
}

FLinkerLoad* FDiffWriterArchiveWriter::CreateLinkerForPackage(FUObjectSerializeContext* LoadContext, const FString& InPackageName, const FString& InFilename, const FPackageData& PackageData)
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

static FString GetTableKey(const FLinkerLoad* Linker, const FObjectExport& Export)
{
	FName ClassName = Export.ClassIndex.IsNull() ? FName(NAME_Class) : Linker->ImpExp(Export.ClassIndex).ObjectName;
	return FString::Printf(TEXT("%s %s.%s"),
		*ClassName.ToString(),
		!Export.OuterIndex.IsNull() ? *Linker->ImpExp(Export.OuterIndex).ObjectName.ToString() : *FPackageName::GetShortName(Linker->LinkerRoot),
		*Export.ObjectName.ToString());
}

static FString GetTableKey(const FLinkerLoad* Linker, const FObjectImport& Import)
{
	return FString::Printf(TEXT("%s %s.%s"),
		*Import.ClassName.ToString(),
		!Import.OuterIndex.IsNull() ? *Linker->ImpExp(Import.OuterIndex).ObjectName.ToString() : TEXT("NULL"),
		*Import.ObjectName.ToString());
}

static inline FString GetTableKey(const FLinkerLoad* Linker, const FName& Name)
{
	return *Name.ToString();
}

static inline FString GetTableKey(const FLinkerLoad* Linker, FNameEntryId Id)
{
	return FName::GetEntry(Id)->GetPlainNameString();
}

static inline FString GetTableKeyForIndex(const FLinkerLoad* Linker, FPackageIndex Index)
{
	if (Index.IsNull())
	{
		return TEXT("NULL");
	}
	else if (Index.IsExport())
	{
		return GetTableKey(Linker, Linker->Exp(Index));
	}
	else
	{
		return GetTableKey(Linker, Linker->Imp(Index));
	}
}

static bool ComparePackageIndices(FLinkerLoad* SourceLinker, FLinkerLoad* DestLinker, const FPackageIndex& SourceIndex,
	const FPackageIndex& DestIndex, const UE::DiffWriterArchive::FMessageCallback& MessageCallback);

static bool CompareTableItem(FLinkerLoad* SourceLinker, FLinkerLoad* DestLinker,
	const FName& SourceName, const FName& DestName, const UE::DiffWriterArchive::FMessageCallback& MessageCallback)
{
	return SourceName == DestName;
}

static bool CompareTableItem(FLinkerLoad* SourceLinker, FLinkerLoad* DestLinker,
	FNameEntryId SourceName, FNameEntryId DestName, const UE::DiffWriterArchive::FMessageCallback& MessageCallback)
{
	return SourceName == DestName;
}

static FString ConvertItemToText(const FName& Name, FLinkerLoad* Linker)
{
	return Name.ToString();
}

static FString ConvertItemToText(FNameEntryId Id, FLinkerLoad* Linker)
{
	return FName::GetEntry(Id)->GetPlainNameString();
}

static bool CompareTableItem(FLinkerLoad* SourceLinker, FLinkerLoad* DestLinker, const FObjectImport& SourceImport,
	const FObjectImport& DestImport, const UE::DiffWriterArchive::FMessageCallback& MessageCallback)
{
	if (SourceImport.ObjectName != DestImport.ObjectName ||
		SourceImport.ClassName != DestImport.ClassName ||
		SourceImport.ClassPackage != DestImport.ClassPackage ||
		!ComparePackageIndices(SourceLinker, DestLinker, SourceImport.OuterIndex,
			DestImport.OuterIndex, MessageCallback))
	{
		return false;
	}
	else
	{
		return true;
	}
}

static FString ConvertItemToText(const FObjectImport& Import, FLinkerLoad* Linker)
{
	return FString::Printf(
		TEXT("%s ClassPackage: %s"),
		*GetTableKey(Linker, Import),
		*Import.ClassPackage.ToString()
	);
}

static bool CompareTableItem(FLinkerLoad* SourceLinker, FLinkerLoad* DestLinker, const FObjectExport& SourceExport,
	const FObjectExport& DestExport, const UE::DiffWriterArchive::FMessageCallback& MessageCallback)
{
	if (SourceExport.ObjectName != DestExport.ObjectName ||
		SourceExport.PackageFlags != DestExport.PackageFlags ||
		SourceExport.ObjectFlags != DestExport.ObjectFlags ||
		SourceExport.SerialSize != DestExport.SerialSize ||
		SourceExport.bForcedExport != DestExport.bForcedExport ||
		SourceExport.bNotForClient != DestExport.bNotForClient ||
		SourceExport.bNotForServer != DestExport.bNotForServer ||
		SourceExport.bNotAlwaysLoadedForEditorGame != DestExport.bNotAlwaysLoadedForEditorGame ||
		SourceExport.bIsAsset != DestExport.bIsAsset ||
		SourceExport.bIsInheritedInstance != DestExport.bIsInheritedInstance ||
		SourceExport.bGeneratePublicHash != DestExport.bGeneratePublicHash ||
		!ComparePackageIndices(SourceLinker, DestLinker, SourceExport.TemplateIndex,
			DestExport.TemplateIndex, MessageCallback) ||
		!ComparePackageIndices(SourceLinker, DestLinker, SourceExport.OuterIndex,
			DestExport.OuterIndex, MessageCallback) ||
		!ComparePackageIndices(SourceLinker, DestLinker, SourceExport.ClassIndex,
			DestExport.ClassIndex, MessageCallback) ||
		!ComparePackageIndices(SourceLinker, DestLinker, SourceExport.SuperIndex, 
			DestExport.SuperIndex, MessageCallback))
	{
		return false;
	}
	else
	{
		return true;
	}
}

static bool IsImportMapIdentical(FLinkerLoad* SourceLinker, FLinkerLoad* DestLinker,
	const UE::DiffWriterArchive::FMessageCallback& MessageCallback)
{
	bool bIdentical = (SourceLinker->ImportMap.Num() == DestLinker->ImportMap.Num());
	if (bIdentical)
	{
		for (int32 ImportIndex = 0; ImportIndex < SourceLinker->ImportMap.Num(); ++ImportIndex)
		{
			if (!CompareTableItem(SourceLinker, DestLinker, SourceLinker->ImportMap[ImportIndex],
				DestLinker->ImportMap[ImportIndex], MessageCallback))
			{
				bIdentical = false;
				break;
			}
		}
	}
	return bIdentical;
}

static bool ComparePackageIndices(FLinkerLoad* SourceLinker, FLinkerLoad* DestLinker, const FPackageIndex& SourceIndex,
	const FPackageIndex& DestIndex, const UE::DiffWriterArchive::FMessageCallback& MessageCallback)
{
	if (SourceIndex.IsNull() && DestIndex.IsNull())
	{
		return true;
	}

	if (SourceIndex.IsExport() && DestIndex.IsExport())
	{
		int32 SourceArrayIndex = SourceIndex.ToExport();
		int32 DestArrayIndex   = DestIndex  .ToExport();

		if (!SourceLinker->ExportMap.IsValidIndex(SourceArrayIndex) || !DestLinker->ExportMap.IsValidIndex(DestArrayIndex))
		{
			MessageCallback(ELogVerbosity::Warning, FString::Printf(
				TEXT("Invalid export indices found, source: %d (of %d), dest: %d (of %d)"),
				SourceArrayIndex, SourceLinker->ExportMap.Num(), DestArrayIndex, DestLinker->ExportMap.Num()));
			return false;
		}

		const FObjectExport& SourceOuterExport = SourceLinker->Exp(SourceIndex);
		const FObjectExport& DestOuterExport   = DestLinker  ->Exp(DestIndex);

		FString SourceOuterExportKey = GetTableKey(SourceLinker, SourceOuterExport);
		FString DestOuterExportKey   = GetTableKey(DestLinker,   DestOuterExport);

		return SourceOuterExportKey == DestOuterExportKey;
	}

	if (SourceIndex.IsImport() && DestIndex.IsImport())
	{
		int32 SourceArrayIndex = SourceIndex.ToImport();
		int32 DestArrayIndex   = DestIndex  .ToImport();

		if (!SourceLinker->ImportMap.IsValidIndex(SourceArrayIndex) || !DestLinker->ImportMap.IsValidIndex(DestArrayIndex))
		{
			MessageCallback(ELogVerbosity::Warning, FString::Printf(
				TEXT("Invalid import indices found, source: %d (of %d), dest: %d (of %d)"),
				SourceArrayIndex, SourceLinker->ImportMap.Num(), DestArrayIndex, DestLinker->ImportMap.Num()));
			return false;
		}

		const FObjectImport& SourceOuterImport = SourceLinker->Imp(SourceIndex);
		const FObjectImport& DestOuterImport   = DestLinker  ->Imp(DestIndex);

		FString SourceOuterImportKey = GetTableKey(SourceLinker, SourceOuterImport);
		FString DestOuterImportKey   = GetTableKey(DestLinker,   DestOuterImport);

		return SourceOuterImportKey == DestOuterImportKey;
	}

	return false;
}

static FString ConvertItemToText(const FObjectExport& Export, FLinkerLoad* Linker)
{
	FName ClassName = Export.ClassIndex.IsNull() ? FName(NAME_Class) : Linker->ImpExp(Export.ClassIndex).ObjectName;
	return FString::Printf(TEXT("%s Super: %s, Template: %s, Flags: %d, Size: %lld, PackageFlags: %d, ForcedExport: %d, NotForClient: %d, NotForServer: %d, NotAlwaysLoadedForEditorGame: %d, IsAsset: %d, IsInheritedInstance: %d, GeneratePublicHash: %d"),
		*GetTableKey(Linker, Export),
		*GetTableKeyForIndex(Linker, Export.SuperIndex),
		*GetTableKeyForIndex(Linker, Export.TemplateIndex),
		(int32)Export.ObjectFlags,
		Export.SerialSize,
		Export.PackageFlags,
		Export.bForcedExport,
		Export.bNotForClient,
		Export.bNotForServer,
		Export.bNotAlwaysLoadedForEditorGame,
		Export.bIsAsset,
		Export.bIsInheritedInstance,
		Export.bGeneratePublicHash);
}

static bool IsExportMapIdentical(FLinkerLoad* SourceLinker, FLinkerLoad* DestLinker,
	const UE::DiffWriterArchive::FMessageCallback& MessageCallback)
{
	bool bIdentical = (SourceLinker->ExportMap.Num() == DestLinker->ExportMap.Num());
	if (bIdentical)
	{
		for (int32 ExportIndex = 0; ExportIndex < SourceLinker->ExportMap.Num(); ++ExportIndex)
		{
			if (!CompareTableItem(SourceLinker, DestLinker, SourceLinker->ExportMap[ExportIndex],
				DestLinker->ExportMap[ExportIndex], MessageCallback))
			{
				bIdentical = false;
				break;
			}
		}
	}
	return bIdentical;
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
template <typename T>
static void DumpTableDifferences(
	FLinkerLoad* SourceLinker, 
	FLinkerLoad* DestLinker, 
	TArray<T>& SourceTable, 
	TArray<T>& DestTable,
	const TCHAR* AssetFilename,
	const TCHAR* ItemName,
	const int32 MaxDiffsToLog,
	const UE::DiffWriterArchive::FMessageCallback& MessageCallback
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
		SourceSet.Add(TTableItem<T>(GetTableKey(SourceLinker, Item), &Item, Index));
	}
	for (int32 Index = 0; Index < DestTable.Num(); ++Index)
	{
		const T& Item = DestTable[Index];
		DestSet.Add(TTableItem<T>(GetTableKey(DestLinker, Item), &Item, Index));
	}

	// Determine the list of items removed from the source package and added to the dest package
	TSet<TTableItem<T>> RemovedItems = SourceSet.Difference(DestSet);
	TSet<TTableItem<T>> AddedItems   = DestSet.Difference(SourceSet);

	// Add changed items as added-and-removed
	for (const TTableItem<T>& ChangedSourceItem : SourceSet)
	{
		if (const TTableItem<T>* ChangedDestItem = DestSet.Find(ChangedSourceItem))
		{
			if (!CompareTableItem(SourceLinker, DestLinker, *ChangedSourceItem.Item,
				*ChangedDestItem->Item, MessageCallback))
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
		HumanReadableString += UE::DiffWriterArchive::IndentToken;
		HumanReadableString += FString::Printf(TEXT("-[%d] %s"), RemovedItem.Index, *ConvertItemToText(*RemovedItem.Item, SourceLinker));
		HumanReadableString += UE::DiffWriterArchive::NewLineToken;
	}
	for (const TTableItem<T>& AddedItem : AddedItems)
	{
		HumanReadableString += UE::DiffWriterArchive::IndentToken;
		HumanReadableString += FString::Printf(TEXT("+[%d] %s"), AddedItem.Index, *ConvertItemToText(*AddedItem.Item, DestLinker));
		HumanReadableString += UE::DiffWriterArchive::NewLineToken;
	}

	// For now just log everything out. When this becomes too spammy, respect the MaxDiffsToLog parameter
	NumDiffs = RemovedItems.Num() + AddedItems.Num();
	LoggedDiffs = NumDiffs;

	if (NumDiffs > LoggedDiffs)
	{
		HumanReadableString += UE::DiffWriterArchive::IndentToken;
		HumanReadableString += FString::Printf(TEXT("+ %d differences not logged."), (NumDiffs - LoggedDiffs));
		HumanReadableString += UE::DiffWriterArchive::NewLineToken;
	}

	MessageCallback(ELogVerbosity::Warning, FString::Printf(
		TEXT("%s: %sMap is different (%d %ss in source package vs %d %ss in dest package):%s%s"),		
		AssetFilename,
		ItemName,
		SourceTable.Num(),
		ItemName,
		DestTable.Num(),
		ItemName,
		UE::DiffWriterArchive::NewLineToken,
		*HumanReadableString));
}

static void DumpPackageHeaderDiffs_LinkerLoad(
	const FDiffWriterArchiveWriter::FPackageData& SourcePackage,
	const FDiffWriterArchiveWriter::FPackageData& DestPackage,
	const FString& AssetFilename,
	const int32 MaxDiffsToLog,
	const UE::DiffWriterArchive::FMessageCallback& MessageCallback)
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
		SourceLinker = FDiffWriterArchiveWriter::CreateLinkerForPackage(LinkerLoadContext, SourceAssetPackageName, AssetFilename, SourcePackage);
		EndLoad(SourceLinker ? SourceLinker->GetSerializeContext() : LinkerLoadContext.GetReference());
	}
	
	{
		TRefCountPtr<FUObjectSerializeContext> LinkerLoadContext(FUObjectThreadContext::Get().GetSerializeContext());
		BeginLoad(LinkerLoadContext);
		DestLinker = FDiffWriterArchiveWriter::CreateLinkerForPackage(LinkerLoadContext, DestAssetPackageName, AssetFilename, DestPackage);
		EndLoad(DestLinker ? DestLinker->GetSerializeContext() : LinkerLoadContext.GetReference());
	}

	if (SourceLinker && DestLinker)
	{
		if (SourceLinker->NameMap != DestLinker->NameMap)
		{
			DumpTableDifferences<FNameEntryId>(SourceLinker, DestLinker, SourceLinker->NameMap, DestLinker->NameMap,
				*AssetFilename, TEXT("Name"), MaxDiffsToLog, MessageCallback);
		}

		if (!IsImportMapIdentical(SourceLinker, DestLinker, MessageCallback))
		{
			DumpTableDifferences<FObjectImport>(SourceLinker, DestLinker, SourceLinker->ImportMap, DestLinker->ImportMap,
				*AssetFilename, TEXT("Import"), MaxDiffsToLog, MessageCallback);
		}

		if (!IsExportMapIdentical(SourceLinker, DestLinker, MessageCallback))
		{
			DumpTableDifferences<FObjectExport>(SourceLinker, DestLinker, SourceLinker->ExportMap, DestLinker->ExportMap,
				*AssetFilename, TEXT("Export"), MaxDiffsToLog, MessageCallback);
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

static void DumpPackageHeaderDiffs_ZenPackage(
	const FDiffWriterArchiveWriter::FPackageData& SourcePackage,
	const FDiffWriterArchiveWriter::FPackageData& DestPackage,
	const FString& AssetFilename,
	const int32 MaxDiffsToLog,
	const UE::DiffWriterArchive::FMessageCallback& MessageCallback)
{
	// TODO: Fill in detailed diffing of Zen Package Summary
}

void FDiffWriterArchiveWriter::DumpPackageHeaderDiffs(
	const FPackageData& SourcePackage,
	const FPackageData& DestPackage,
	const FString& AssetFilename,
	const int32 MaxDiffsToLog,
	const EPackageHeaderFormat PackageHeaderFormat,
	const UE::DiffWriterArchive::FMessageCallback& MessageCallback)
{
	switch (PackageHeaderFormat)
	{
	case EPackageHeaderFormat::PackageFileSummary:
		DumpPackageHeaderDiffs_LinkerLoad(SourcePackage, DestPackage, AssetFilename, MaxDiffsToLog, MessageCallback);
		break;
	case EPackageHeaderFormat::ZenPackageSummary:
		DumpPackageHeaderDiffs_ZenPackage(SourcePackage, DestPackage, AssetFilename, MaxDiffsToLog, MessageCallback);
		break;
	default:
		unimplemented();
	}
}
