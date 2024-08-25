// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataElementCommon.h"

#include "PCGParamData.h"
#include "Data/PCGSpatialData.h"

namespace PCGMetadataElementCommon
{
	void DuplicateTaggedData(const FPCGTaggedData& InTaggedData, FPCGTaggedData& OutTaggedData, UPCGMetadata*& OutMetadata)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGMetadataElementCommon::DuplicateTaggedData);
		if (InTaggedData.Data)
		{
			UPCGData* NewData = InTaggedData.Data->DuplicateData();
			check(NewData);
			OutTaggedData.Data = NewData;
			OutMetadata = NewData->MutableMetadata();
		}
	}

	void CopyEntryToValueKeyMap(const UPCGMetadata* MetadataToCopy, const FPCGMetadataAttributeBase* AttributeToCopy, FPCGMetadataAttributeBase* OutAttribute)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGMetadataElementCommon::CopyEntryToValueKeyMap);

		if (!OutAttribute)
		{
			UE_LOG(LogPCG, Error, TEXT("Failed to create output attribute"));
			return;
		}

		const PCGMetadataEntryKey EntryKeyCount = MetadataToCopy->GetItemCountForChild();
		for (PCGMetadataEntryKey EntryKey = 0; EntryKey < EntryKeyCount; ++EntryKey)
		{
			const PCGMetadataValueKey ValueKey = AttributeToCopy->GetValueKey(EntryKey);
			OutAttribute->SetValueFromValueKey(EntryKey, ValueKey);
		}
	}

	bool CopyFromAccessorToAccessor(FCopyFromAccessorToAccessorParams& Params)
	{
		check(Params.InAccessor && Params.OutAccessor && Params.InKeys && Params.OutKeys);

		int32 Count = 0;
		switch (Params.IterationCount)
		{
		case FCopyFromAccessorToAccessorParams::EIterationCount::In:
			Count = Params.InKeys->GetNum();
			break;
		case FCopyFromAccessorToAccessorParams::EIterationCount::Out:
			Count = Params.OutKeys->GetNum();
			break;
		case FCopyFromAccessorToAccessorParams::EIterationCount::Min:
			Count = FMath::Min(Params.InKeys->GetNum(), Params.OutKeys->GetNum());
			break;
		case FCopyFromAccessorToAccessorParams::EIterationCount::Max:
			Count = FMath::Max(Params.InKeys->GetNum(), Params.OutKeys->GetNum());
			break;
		default:
			checkNoEntry();
			return false;
		}

		// Early out - contrary to ApplyOnAccessorRange, having a zero count here is not an error
		if (Count == 0)
		{
			return true;
		}

		auto Operation = [&Params, Count](auto Dummy)
		{
			using OutputType = decltype(Dummy);

			auto SetToAccessor = [&Params](const TArrayView<OutputType>& View, const int32 Start, const int32 Range)
			{
				Params.OutAccessor->SetRange<OutputType>(View, Start, *Params.OutKeys, Params.Flags);
			};

			return PCGMetadataElementCommon::ApplyOnAccessorRange<OutputType>(*Params.InKeys, *Params.InAccessor, SetToAccessor, Params.Flags, Params.ChunkSize, Count);
		};

		return PCGMetadataAttribute::CallbackWithRightType(Params.OutAccessor->GetUnderlyingType(), Operation);
	}
}
