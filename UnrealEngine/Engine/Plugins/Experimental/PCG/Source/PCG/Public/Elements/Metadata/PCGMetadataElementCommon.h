// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h" // IWYU pragma: keep
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

class IPCGAttributeAccessor;
template <typename T> class FPCGMetadataAttribute;

struct FPCGTaggedData;

namespace PCGMetadataElementCommon
{
	void DuplicateTaggedData(const FPCGTaggedData& InTaggedData, FPCGTaggedData& OutTaggedData, UPCGMetadata*& OutMetadata);

	/** Copies the entry to value key relationship stored in the given Metadata, including its parents */
	void CopyEntryToValueKeyMap(const UPCGMetadata* MetadataToCopy, const FPCGMetadataAttributeBase* AttributeToCopy, FPCGMetadataAttributeBase* OutAttribute);

	/** Creates a new attribute, or clears the attribute if it already exists and is a 'T' type */
	template<typename T>
	FPCGMetadataAttribute<T>* ClearOrCreateAttribute(UPCGMetadata* Metadata, const FName& DestinationAttribute, T DefaultValue)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGMetadataElementCommon::ClearOrCreateAttribute);

		if (!Metadata)
		{
			UE_LOG(LogPCG, Error, TEXT("Failed to create metadata"));
			return nullptr;
		}

		if (Metadata->HasAttribute(DestinationAttribute))
		{
			UE_LOG(LogPCG, Verbose, TEXT("Attribute %s already exists and has been overwritten"), *DestinationAttribute.ToString());
			Metadata->DeleteAttribute(DestinationAttribute);
		}

		Metadata->CreateAttribute<T>(DestinationAttribute, DefaultValue, /*bAllowsInterpolation=*/true, /*bOverrideParent=*/false);

		return static_cast<FPCGMetadataAttribute<T>*>(Metadata->GetMutableAttribute(DestinationAttribute));
	}

	constexpr int32 DefaultChunkSize = 256;

	/**
	* Iterate over the full range of the keys, calling the callback with values get from the accessor.
	* ChunkSize influence the max number of values to get in one go with GetRange.
	* Callback should have a signature: void(const T&, int32)
	* Return false if it process nothing.
	*/
	template <typename T, typename Func>
	bool ApplyOnAccessor(const IPCGAttributeAccessorKeys& Keys, const IPCGAttributeAccessor& Accessor, Func&& Callback, const int32 ChunkSize = DefaultChunkSize)
	{
		const int32 NumberOfEntries = Keys.GetNum();

		if (NumberOfEntries == 0)
		{
			return false;
		}

		TArray<T, TInlineAllocator<DefaultChunkSize>> TempValues;
		TempValues.SetNum(ChunkSize);

		const int32 NumberOfIterations = (NumberOfEntries + ChunkSize - 1) / ChunkSize;

		for (int32 i = 0; i < NumberOfIterations; ++i)
		{
			const int32 StartIndex = i * ChunkSize;
			const int32 Range = FMath::Min(NumberOfEntries - StartIndex, ChunkSize);
			TArrayView<T> View(TempValues.GetData(), Range);

			if (!Accessor.GetRange(View, StartIndex, Keys))
			{
				return false;
			}

			for (int32 j = 0; j < Range; ++j)
			{
				Callback(TempValues[j], StartIndex + j);
			}
		}

		return true;
	}

	/**
	* Automatically fill all preconfigured settings depending on the enum operation.
	* Can also specify explicitly values that should not be included, as metadata is not available in non-editor builds.
	*/
	template <typename EnumOperation>
	TArray<FPCGPreConfiguredSettingsInfo> FillPreconfiguredSettingsInfoFromEnum(const TSet<EnumOperation>& InValuesToSkip = {})
	{
		TArray<FPCGPreConfiguredSettingsInfo> PreconfiguredInfo;

		if (const UEnum* EnumPtr = StaticEnum<EnumOperation>())
		{
			PreconfiguredInfo.Reserve(EnumPtr->NumEnums());
			for (int32 i = 0; i < EnumPtr->NumEnums(); ++i)
			{
				int64 Value = EnumPtr->GetValueByIndex(i);

				if (Value != EnumPtr->GetMaxEnumValue() && !InValuesToSkip.Contains(EnumOperation(Value)))
				{
					PreconfiguredInfo.Emplace(Value, EnumPtr->GetDisplayNameTextByValue(Value));
				}
			}
		}

		return PreconfiguredInfo;
	}
}
