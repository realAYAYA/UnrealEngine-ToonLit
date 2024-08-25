// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTraits.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h" // IWYU pragma: keep
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

class IPCGAttributeAccessor;
template <typename T> class FPCGMetadataAttribute;

struct FPCGTaggedData;

namespace PCGMetadataElementCommon
{
	PCG_API void DuplicateTaggedData(const FPCGTaggedData& InTaggedData, FPCGTaggedData& OutTaggedData, UPCGMetadata*& OutMetadata);

	/** Copies the entry to value key relationship stored in the given Metadata, including its parents */
	PCG_API void CopyEntryToValueKeyMap(const UPCGMetadata* MetadataToCopy, const FPCGMetadataAttributeBase* AttributeToCopy, FPCGMetadataAttributeBase* OutAttribute);

	/** Creates a new attribute, or clears the attribute if it already exists and is a 'T' type. If default value not provided, will take the zero value for that type. */
	template<typename T>
	FPCGMetadataAttribute<T>* ClearOrCreateAttribute(UPCGMetadata* Metadata, const FName& DestinationAttribute, T DefaultValue = PCG::Private::MetadataTraits<T>::ZeroValue())
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
	* Iterate over the full range of the keys (if Count is negative, otherwise, as many times as Count), calling the callback with values range get from the accessor.
	* ChunkSize influence the max number of values to get in one go with GetRange.
	* Callback should have a signature: void(const TArrayView<T>& View, int32 Start, int32 Range)
	* Return false if it process nothing.
	*/
	template <typename T, typename Func>
	bool ApplyOnAccessorRange(const IPCGAttributeAccessorKeys& Keys, const IPCGAttributeAccessor& Accessor, Func&& Callback, EPCGAttributeAccessorFlags Flags = EPCGAttributeAccessorFlags::StrictType, const int32 ChunkSize = DefaultChunkSize, const int32 Count = -1)
	{
		const int32 NumberOfEntries = Count < 0 ? Keys.GetNum() : Count;

		if (NumberOfEntries == 0)
		{
			return false;
		}

		TArray<T, TInlineAllocator<DefaultChunkSize>> TempValues;
		if constexpr (std::is_trivially_copyable_v<T>)
		{
			TempValues.SetNumUninitialized(ChunkSize);
		}
		else
		{
			TempValues.SetNum(ChunkSize);
		}

		const int32 NumberOfIterations = (NumberOfEntries + ChunkSize - 1) / ChunkSize;

		for (int32 i = 0; i < NumberOfIterations; ++i)
		{
			const int32 StartIndex = i * ChunkSize;
			const int32 Range = FMath::Min(NumberOfEntries - StartIndex, ChunkSize);
			TArrayView<T> View(TempValues.GetData(), Range);

			if (!Accessor.GetRange(View, StartIndex, Keys, Flags))
			{
				return false;
			}

			Callback(View, StartIndex, Range);
		}

		return true;
	}

	/**
	* Iterate over the full range of the keys (if Count is negative, otherwise, as many times as Count), calling the callback with values get from the accessor.
	* ChunkSize influence the max number of values to get in one go with GetRange.
	* Callback should have a signature: void(const T& Value, int32 Index)
	* Return false if it process nothing.
	*/
	template <typename T, typename Func>
	bool ApplyOnAccessor(const IPCGAttributeAccessorKeys& Keys, const IPCGAttributeAccessor& Accessor, Func&& InCallback, EPCGAttributeAccessorFlags Flags = EPCGAttributeAccessorFlags::StrictType, const int32 ChunkSize = DefaultChunkSize, const int32 Count = -1)
	{
		auto RangeCallback = [Callback = std::forward<Func>(InCallback)](const TArrayView<T>& View, int32 Start, int32 Range)
		{
			for (int32 j = 0; j < Range; ++j)
			{
				Callback(View[j], Start + j);
			}
		};

		return ApplyOnAccessorRange<T>(Keys, Accessor, std::move(RangeCallback), Flags, ChunkSize, Count);
	}

	/**
	* Read all the values from in accessor and write to out accessor. Iterate as many times than the out accessor keys
	*/
	struct PCG_API FCopyFromAccessorToAccessorParams
	{
		enum EIterationCount
		{
			In, // Iterate as many times than the in accessor keys
			Out, // Iterate as many times than the out accessor keys
			Min, // Iterate as many times than the min of accessor keys
			Max, // Iterate as many times than the max of accessor keys
		};

		EIterationCount IterationCount;
		const IPCGAttributeAccessor* InAccessor = nullptr;
		const IPCGAttributeAccessorKeys* InKeys = nullptr;
		IPCGAttributeAccessor* OutAccessor = nullptr;
		IPCGAttributeAccessorKeys* OutKeys = nullptr;
		EPCGAttributeAccessorFlags Flags = EPCGAttributeAccessorFlags::StrictType;
		int32 ChunkSize = DefaultChunkSize;
	};

	PCG_API bool CopyFromAccessorToAccessor(FCopyFromAccessorToAccessorParams& Params);

	/**
	* Automatically fill all preconfigured settings depending on the enum operation.
	* Can also specify explicitly values that should not be included, as metadata is not available in non-editor builds.
	*/
	template <typename EnumOperation>
	TArray<FPCGPreConfiguredSettingsInfo> FillPreconfiguredSettingsInfoFromEnum(const TSet<EnumOperation>& InValuesToSkip = {}, const FText& InOptionalPrefix = FText())
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
					PreconfiguredInfo.Emplace(Value, FText::Format(FText::FromString(TEXT("{0}{1}")), InOptionalPrefix, EnumPtr->GetDisplayNameTextByValue(Value)));
				}
			}
		}

		return PreconfiguredInfo;
	}
}
