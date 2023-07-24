// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteControlProtocolBinding.h"

#include "CborWriter.h"
#include "Factories/IRemoteControlMaskingFactory.h"
#include "IRemoteControlModule.h"
#include "RemoteControlPreset.h"
#include "RemoteControlSettings.h"
#include "Algo/MinElement.h"
#include "Algo/Sort.h"
#include "Backends/CborStructDeserializerBackend.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/StructOnScope.h"
#include "UObject/TextProperty.h"

#define LOCTEXT_NAMESPACE "RemoteControl"

namespace EntityInterpolation
{
	/** 
	 * The function is taking the map with the range buffer pointer and convert it to the map with value pointers instead. 
	 * Range keys stay the same.
	 * For example, we have an input map TArray<TPair<int32, uint8*>> where uint8* points to the float buffer (4 bytes)
	 * Then we need to convert it to value buffer where we convert uint8* to value pointer. In this case, is float*
	 * 
	 * But in cases when we are dealing with Struct or any containers (Array, Map, Structs) inner elements we need to convert the container pointer to value pointer
	 * And that means if there no FProperty Outer wh should convert uint8* to ValueType* directly, 
	 * but in case with Outer != nullptr we need to convert with InProperty->ContainerPtrToValuePtr<ValueType>
	 */
	template <typename ValueType, typename PropertyType, typename ProtocolValueType>
	TArray<FRemoteControlProtocolEntity::TRangeMappingData<ProtocolValueType, ValueType>> ContainerPtrMapToValuePtrMap(PropertyType* InProperty, FProperty* Outer, TArray<FRemoteControlProtocolEntity::FRangeMappingData>& InRangeMappingBuffers, int32 InArrayIndex)
	{
		TArray<FRemoteControlProtocolEntity::TRangeMappingData<ProtocolValueType, ValueType>> ValueMap;
		ValueMap.Reserve(InRangeMappingBuffers.Num());

		int32 MappingIndex = 0;
		for (FRemoteControlProtocolEntity::FRangeMappingData& RangePair : InRangeMappingBuffers)
		{
			FRemoteControlProtocolEntity::TRangeMappingData<ProtocolValueType, ValueType> TypedPair;
			TypedPair.Range = *reinterpret_cast<ProtocolValueType*>(RangePair.Range.GetData());

			if(Outer)
			{
				if(Outer->GetClass() == FArrayProperty::StaticClass())
				{
					FScriptArrayHelper Helper(CastField<FArrayProperty>(Outer), RangePair.Mapping.GetData());
					TypedPair.Mapping = *reinterpret_cast<ValueType*>(Helper.GetRawPtr(InArrayIndex));
				}
				else
				{
					TypedPair.Mapping = *InProperty->template ContainerPtrToValuePtr<ValueType>(RangePair.Mapping.GetData(), InArrayIndex);	
				}
			}
			else
			{
				TArray<uint8> Buffer;
				RemoteControlPropertyUtilities::FRCPropertyVariant Src{InProperty, RangePair.Mapping};
				RemoteControlPropertyUtilities::FRCPropertyVariant Dst{InProperty, Buffer};
				RemoteControlPropertyUtilities::Deserialize<PropertyType>(Src, Dst);
				TypedPair.Mapping = *Dst.GetPropertyValue<ValueType>();
			}

			ValueMap.Add(TypedPair);
			MappingIndex++;
		}

		return ValueMap;
	}

	/** Wraps FMath::Lerp, allowing Protocol Binding specific specialization */
	template <class T, class U>
	static T Lerp(const T& A, const T& B, const U& Alpha)
	{
		return FMath::Lerp(A, B, Alpha);
	}

	/** Specialization for bool, toggles at 0.5 Alpha instead of 1.0 */
	template <>
	bool Lerp(const bool& A, const bool& B, const float& Alpha)
	{
		return Alpha > 0.5f ? B : A;
	}

	/** Specialization for FString, toggles at 0.5 Alpha instead of 1.0 */
	template <>
	FString Lerp(const FString& A, const FString& B, const float& Alpha)
	{
		return Alpha > 0.5f ? B : A;
	}

	/** Specialization for FName, toggles at 0.5 Alpha instead of 1.0 */
	template <>
	FName Lerp(const FName& A, const FName& B, const float& Alpha)
	{
		return Alpha > 0.5f ? B : A;
	}

	/** Specialization for FText, toggles at 0.5 Alpha instead of 1.0 */
	template <>
	FText Lerp(const FText& A, const FText& B, const float& Alpha)
	{
		return Alpha > 0.5f ? B : A;
	}

	template <typename ValueType, typename PropertyType, typename ProtocolValueType>
	bool InterpolateValue(PropertyType* InProperty, FProperty* Outer, TArray<FRemoteControlProtocolEntity::FRangeMappingData>& InRangeMappingBuffers, ProtocolValueType InProtocolValue, ValueType& OutResultValue, int32 ArrayIndex)
	{
		TArray<FRemoteControlProtocolEntity::TRangeMappingData<ProtocolValueType, ValueType>> ValueMap = ContainerPtrMapToValuePtrMap<ValueType, PropertyType, ProtocolValueType>(InProperty, Outer, InRangeMappingBuffers, ArrayIndex);

		// sort by input protocol value
		Algo::SortBy(ValueMap, [](const FRemoteControlProtocolEntity::TRangeMappingData<ProtocolValueType, ValueType>& Item) -> ProtocolValueType
		{
			return Item.Range;
		});

		// clamp to min and max mapped values
		ProtocolValueType ClampProtocolValue = FMath::Clamp(InProtocolValue, ValueMap[0].Range, ValueMap.Last().Range);

		FRemoteControlProtocolEntity::TRangeMappingData<ProtocolValueType*, ValueType*> RangeMin{nullptr, nullptr};
		FRemoteControlProtocolEntity::TRangeMappingData<ProtocolValueType*, ValueType*> RangeMax{nullptr, nullptr};

		for (int32 RangeIdx = 0; RangeIdx < ValueMap.Num(); ++RangeIdx)
		{
			FRemoteControlProtocolEntity::TRangeMappingData<ProtocolValueType, ValueType>& Range = ValueMap[RangeIdx];
			if (ClampProtocolValue > Range.Range || RangeMin.Mapping == nullptr)
			{
				RangeMin.Range = &Range.Range;
				RangeMin.Mapping = &Range.Mapping;
			}
			else if (ClampProtocolValue <= Range.Range)
			{
				RangeMax.Range = &Range.Range;
				RangeMax.Mapping = &Range.Mapping;
				// Max found, no need to continue
				break;
			}
		}

		if (RangeMax.Mapping == nullptr || RangeMin.Mapping == nullptr)
		{
			return ensure(false);
		}
		if (RangeMax.Range == nullptr || RangeMin.Range == nullptr)
		{
			return ensure(false);
		}
		else if (*RangeMax.Range == *RangeMin.Range)
		{
			UE_LOG(LogRemoteControl, Warning, TEXT("Range input values are the same."));
			return true;
		}

		// Normalize value to range for interpolation
		const float Percentage = static_cast<float>(ClampProtocolValue - *RangeMin.Range) / static_cast<float>(*RangeMax.Range - *RangeMin.Range);

		OutResultValue = Lerp(*RangeMin.Mapping, *RangeMax.Mapping, Percentage);

		return true;
	}

	// Writes a property value to the serialization output.
	template <typename ValueType>
	void WritePropertyValue(FCborWriter& InCborWriter, FProperty* InProperty, const ValueType& Value, bool bWriteName = true)
	{
		if (bWriteName)
		{
			InCborWriter.WriteValue(InProperty->GetName());
		}
		InCborWriter.WriteValue(Value);
	}

	// Specialization for FName that converts to FString
	template <>
	void WritePropertyValue<FName>(FCborWriter& InCborWriter, FProperty* InProperty, const FName& Value, bool bWriteName)
	{
		if (bWriteName)
		{
			InCborWriter.WriteValue(InProperty->GetName());
		}
		InCborWriter.WriteValue(Value.ToString());
	}

	// Specialization for FText that converts to FString
	template <>
	void WritePropertyValue<FText>(FCborWriter& InCborWriter, FProperty* InProperty, const FText& Value, bool bWriteName)
	{
		if (bWriteName)
		{
			InCborWriter.WriteValue(InProperty->GetName());
		}
		InCborWriter.WriteValue(Value.ToString());
	}

	template <typename ProtocolValueType>
	bool WriteProperty(FProperty* InProperty, FProperty* OuterProperty, TArray<FRemoteControlProtocolEntity::FRangeMappingData>& InRangeMappingBuffers, ProtocolValueType InProtocolValue, FCborWriter& InCborWriter, int32 InArrayIndex = 0)
	{
		// Value nested in Array/Set (except single element) or map as array or as root
		const bool bIsInArray = OuterProperty != nullptr
								&& (InProperty->ArrayDim > 1
									|| OuterProperty->GetClass() == FArrayProperty::StaticClass()
									|| OuterProperty->GetClass() == FSetProperty::StaticClass()
									|| OuterProperty->GetClass() == FMapProperty::StaticClass());


		bool bSuccess = false;
		if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(InProperty))
		{
			bool bBoolValue = false;
			bSuccess = InterpolateValue(BoolProperty, OuterProperty, InRangeMappingBuffers, InProtocolValue, bBoolValue, InArrayIndex);			
			bBoolValue = StaticCast<uint8>(bBoolValue) > 0; // Ensure 0 or 1, can be different if property was packed.
			WritePropertyValue(InCborWriter, InProperty, bBoolValue, !bIsInArray);
		}
		else if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(InProperty))
		{
			if (CastField<FFloatProperty>(InProperty))
			{
				float FloatValue = 0.f;
				bSuccess = InterpolateValue(NumericProperty, OuterProperty, InRangeMappingBuffers, InProtocolValue, FloatValue, InArrayIndex);
				WritePropertyValue(InCborWriter, InProperty, FloatValue, !bIsInArray);
			}
			else if (CastField<FDoubleProperty>(InProperty))
			{
				double DoubleValue = 0.0;
				bSuccess = InterpolateValue(NumericProperty, OuterProperty, InRangeMappingBuffers, InProtocolValue, DoubleValue, InArrayIndex);
				WritePropertyValue(InCborWriter, InProperty, DoubleValue, !bIsInArray);
			}
			else if (NumericProperty->IsInteger() && !NumericProperty->IsEnum())
			{
				if (FByteProperty* ByteProperty = CastField<FByteProperty>(InProperty))
				{
					uint8 IntValue = 0;
					bSuccess = InterpolateValue(NumericProperty, OuterProperty, InRangeMappingBuffers, InProtocolValue, IntValue, InArrayIndex);
					WritePropertyValue(InCborWriter, InProperty, static_cast<int64>(IntValue), !bIsInArray);
				}
				else if (FIntProperty* IntProperty = CastField<FIntProperty>(InProperty))
				{
					int IntValue = 0;
					bSuccess = InterpolateValue(NumericProperty, OuterProperty, InRangeMappingBuffers, InProtocolValue, IntValue, InArrayIndex);
					WritePropertyValue(InCborWriter, InProperty, static_cast<int64>(IntValue), !bIsInArray);
				}
				else if (FUInt32Property* UInt32Property = CastField<FUInt32Property>(InProperty))
				{
					uint32 IntValue = 0;
					bSuccess = InterpolateValue(NumericProperty, OuterProperty, InRangeMappingBuffers, InProtocolValue, IntValue, InArrayIndex);
					WritePropertyValue(InCborWriter, InProperty, static_cast<int64>(IntValue), !bIsInArray);
				}
				else if (FInt16Property* Int16Property = CastField<FInt16Property>(InProperty))
				{
					int16 IntValue = 0;
					bSuccess = InterpolateValue(NumericProperty, OuterProperty, InRangeMappingBuffers, InProtocolValue, IntValue, InArrayIndex);
					WritePropertyValue(InCborWriter, InProperty, static_cast<int64>(IntValue), !bIsInArray);
				}
				else if (FUInt16Property* FInt16Property = CastField<FUInt16Property>(InProperty))
				{
					uint16 IntValue = 0;
					bSuccess = InterpolateValue(NumericProperty, OuterProperty, InRangeMappingBuffers, InProtocolValue, IntValue, InArrayIndex);
					WritePropertyValue(InCborWriter, InProperty, static_cast<int64>(IntValue), !bIsInArray);
				}
				else if (FInt64Property* Int64Property = CastField<FInt64Property>(InProperty))
				{
					int64 IntValue = 0;
					bSuccess = InterpolateValue(NumericProperty, OuterProperty, InRangeMappingBuffers, InProtocolValue, IntValue, InArrayIndex);
					WritePropertyValue(InCborWriter, InProperty, static_cast<int64>(IntValue), !bIsInArray);
				}
				else if (FUInt64Property* FInt64Property = CastField<FUInt64Property>(InProperty))
				{
					uint64 IntValue = 0;
					bSuccess = InterpolateValue(NumericProperty, OuterProperty, InRangeMappingBuffers, InProtocolValue, IntValue, InArrayIndex);
					WritePropertyValue(InCborWriter, InProperty, static_cast<int64>(IntValue), !bIsInArray);
				}
				else if (FInt8Property* Int8Property = CastField<FInt8Property>(InProperty))
				{
					int8 IntValue = 0;
					bSuccess = InterpolateValue(NumericProperty, OuterProperty, InRangeMappingBuffers, InProtocolValue, IntValue, InArrayIndex);
					WritePropertyValue(InCborWriter, InProperty, static_cast<int64>(IntValue), !bIsInArray);
				}
			}
		}
		else if (FStructProperty* StructProperty = CastField<FStructProperty>(InProperty))
		{
			if(!bIsInArray)
			{
				InCborWriter.WriteValue(StructProperty->GetName());	
			}
			
			InCborWriter.WriteContainerStart(ECborCode::Map, -1/*Indefinite*/);

			bool bStructSuccess = true;
			for (TFieldIterator<FProperty> It(StructProperty->Struct); It; ++It)
			{
				TArray<FRemoteControlProtocolEntity::FRangeMappingData> RangeMappingBuffers;
				RangeMappingBuffers.Reserve(InRangeMappingBuffers.Num());

				FProperty* InnerProperty = *It;
				for (const FRemoteControlProtocolEntity::FRangeMappingData& RangePair : InRangeMappingBuffers)
				{
					const uint8* DataInContainer = StructProperty->ContainerPtrToValuePtr<const uint8>(RangePair.Mapping.GetData(), InArrayIndex);
					const uint8* DataInStruct = InnerProperty->ContainerPtrToValuePtr<const uint8>(DataInContainer);
					RangeMappingBuffers.Emplace(RangePair.Range, DataInStruct, InnerProperty->GetSize(), 1);
				}

				bStructSuccess &= WriteProperty(InnerProperty, StructProperty, RangeMappingBuffers, InProtocolValue, InCborWriter, InArrayIndex);
			}

			bSuccess = bStructSuccess;
			InCborWriter.WriteContainerEnd();
		}

		else if (FStrProperty* StrProperty = CastField<FStrProperty>(InProperty))
		{
			FString StringValue;
			bSuccess = InterpolateValue(StrProperty, OuterProperty, InRangeMappingBuffers, InProtocolValue, StringValue, InArrayIndex);
			WritePropertyValue(InCborWriter, InProperty, StringValue, !bIsInArray);
		}
		else if (FNameProperty* NameProperty = CastField<FNameProperty>(InProperty))
		{
			FName NameValue;
			bSuccess = InterpolateValue(NameProperty, OuterProperty, InRangeMappingBuffers, InProtocolValue, NameValue, InArrayIndex);
			WritePropertyValue(InCborWriter, InProperty, NameValue, !bIsInArray);
		}
		else if (FTextProperty* TextProperty = CastField<FTextProperty>(InProperty))
		{
			FText TextValue;
			bSuccess = InterpolateValue(TextProperty, OuterProperty, InRangeMappingBuffers, InProtocolValue, TextValue, InArrayIndex);
			WritePropertyValue(InCborWriter, InProperty, TextValue, !bIsInArray);
		}

#if !UE_BUILD_SHIPPING && UE_BUILD_DEBUG
		if (!bSuccess)
		{
			if (RemoteControlTypeUtilities::IsSupportedMappingType(InProperty))
			{
				UE_LOG(LogRemoteControl, Error, TEXT("Property type %s is supported for mapping, but unhandled in EntityInterpolation::WriteProperty"), *InProperty->GetClass()->GetName());
			}
		}
#endif

		return bSuccess;
	}

	template <typename ProtocolValueType>
	bool ApplyProtocolValueToProperty(FProperty* InProperty, ProtocolValueType InProtocolValue, TArray<FRemoteControlProtocolEntity::FRangeMappingData>& InRangeMappingBuffers, FCborWriter& InCborWriter)
	{
		// Structures
		if (FStructProperty* StructProperty = CastField<FStructProperty>(InProperty))
		{
			UScriptStruct* ScriptStruct = StructProperty->Struct;

			InCborWriter.WriteValue(StructProperty->GetName());
			InCborWriter.WriteContainerStart(ECborCode::Map, -1/*Indefinite*/);

			bool bStructSuccess = true;
			for (TFieldIterator<FProperty> It(ScriptStruct); It; ++It)
			{
				bStructSuccess &= WriteProperty(*It, StructProperty, InRangeMappingBuffers, InProtocolValue, InCborWriter);
			}

			InCborWriter.WriteContainerEnd();

			return bStructSuccess;
		}

		// @note: temporarily disabled - array of primitives supported, array of structs not
		// Dynamic arrays
		else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InProperty))
		{
			FProperty* InnerProperty = ArrayProperty->Inner;

			InCborWriter.WriteValue(ArrayProperty->GetName());
			InCborWriter.WriteContainerStart(ECborCode::Array, -1/*Indefinite*/);

			bool bArraySuccess = true;

			// Get minimum item count
			const FRemoteControlProtocolEntity::FRangeMappingData* ElementWithSmallestNum = Algo::MinElementBy(InRangeMappingBuffers, [](const FRemoteControlProtocolEntity::FRangeMappingData& RangePair)
			{
				return RangePair.NumElements;
			});

			// No elements in array
			if (ElementWithSmallestNum == nullptr)
			{
				return false;
			}

			for (auto ArrayIndex = 0; ArrayIndex < ElementWithSmallestNum->NumElements; ++ArrayIndex)
			{
				bArraySuccess &= WriteProperty(InnerProperty, ArrayProperty, InRangeMappingBuffers, InProtocolValue, InCborWriter, ArrayIndex);
			}

			InCborWriter.WriteContainerEnd();

			return bArraySuccess;
		}

		// Maps
		else if (FMapProperty* MapProperty = CastField<FMapProperty>(InProperty))
		{
			UE_LOG(LogRemoteControl, Warning, TEXT("MapProperty not supported"));
			return false;
		}

		// Sets
		else if (FSetProperty* SetProperty = CastField<FSetProperty>(InProperty))
		{
			UE_LOG(LogRemoteControl, Warning, TEXT("SetProperty not supported"));
			return false;
		}

			// Static arrays
		else if (InProperty->ArrayDim > 1)
		{
			UE_LOG(LogRemoteControl, Warning, TEXT("Static arrays not supported"));
			return false;
		}

			// All other properties
		else
		{
			return WriteProperty(InProperty, nullptr, InRangeMappingBuffers, InProtocolValue, InCborWriter);
		}
	}
}

// Optionally supplying the MappingId is used by the undo system
FRemoteControlProtocolMapping::FRemoteControlProtocolMapping(FProperty* InProperty, uint8 InRangeValueSize, const FGuid& InMappingId)
	: Id(InMappingId)
{
	if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(InProperty))
	{
		InterpolationMappingPropertyData.AddZeroed(sizeof(bool));
	}
	else if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(InProperty))
	{
		InterpolationMappingPropertyData.AddZeroed(NumericProperty->ElementSize);
	}
	else if (FStructProperty* StructProperty = CastField<FStructProperty>(InProperty))
	{
		UScriptStruct* ScriptStruct = StructProperty->Struct;

		InterpolationMappingPropertyData.AddZeroed(ScriptStruct->GetStructureSize());
		ScriptStruct->InitializeStruct(InterpolationMappingPropertyData.GetData());
	}
	else
	{
		InterpolationMappingPropertyData.AddZeroed(1);
	}

	InterpolationRangePropertyData.SetNumZeroed(InRangeValueSize);

	BoundPropertyPath = InProperty;
}

bool FRemoteControlProtocolMapping::operator==(const FRemoteControlProtocolMapping& InProtocolMapping) const
{
	return Id == InProtocolMapping.Id;
}

bool FRemoteControlProtocolMapping::operator==(FGuid InProtocolMappingId) const
{
	return Id == InProtocolMappingId;
}

TSharedPtr<FStructOnScope> FRemoteControlProtocolMapping::GetMappingPropertyAsStructOnScope()
{
	if (FStructProperty* StructProperty = CastField<FStructProperty>(BoundPropertyPath.Get()))
	{
		UScriptStruct* ScriptStruct = StructProperty->Struct;
		check(InterpolationMappingPropertyData.Num() && InterpolationMappingPropertyData.Num() == ScriptStruct->GetStructureSize());

		return MakeShared<FStructOnScope>(ScriptStruct, InterpolationMappingPropertyData.GetData());
	}

	ensure(false);
	return nullptr;
}

bool FRemoteControlProtocolMapping::PropertySizeMatchesData(const TArray<uint8>& InSource, const FName& InPropertyTypeName)
{
	const int32 SourceNum = InSource.Num();
	if(InPropertyTypeName == NAME_ByteProperty
		|| InPropertyTypeName == NAME_BoolProperty)
	{
		return SourceNum == sizeof(uint8);
	}
	else if(InPropertyTypeName == NAME_UInt16Property
		|| InPropertyTypeName == NAME_Int16Property)
	{
		return SourceNum == sizeof(uint16);
	}
	else if(InPropertyTypeName == NAME_UInt32Property
		|| InPropertyTypeName == NAME_Int32Property
		|| InPropertyTypeName == NAME_FloatProperty)
	{
		return SourceNum == sizeof(uint32);
	}
	else if(InPropertyTypeName == NAME_UInt64Property
		|| InPropertyTypeName == NAME_Int64Property
		|| InPropertyTypeName == NAME_DoubleProperty)
	{
		return SourceNum == sizeof(uint64);
	}

	// @note: only the above types are expected
	return false;
}

void FRemoteControlProtocolMapping::RefreshCachedData(const FName& InRangePropertyTypeName)
{
	// Opportunity to write a different representation of the range and mapping properties to be used at runtime
	// (persisted range might be a 1byte uint8, but the property 4byte uint32)
	// (persisted mapping property might be serialized as text)

	// @note: range type seems to be always interpreted from a 4byte value, so just check for that
	if(InRangePropertyTypeName != NAME_None)
	{
		if(!PropertySizeMatchesData(InterpolationRangePropertyData, InRangePropertyTypeName))
		{
			if(InRangePropertyTypeName.ToString().Contains(TEXT("Int")))
			{
				// Only handles UInt32 range types when theres a size mismatch
				if(InRangePropertyTypeName == NAME_UInt32Property)
				{
					uint32 CachedRangeValue = 0;
					if(InterpolationRangePropertyData.Num() == 1)
					{
						uint8* StoredRangeValue = reinterpret_cast<uint8*>(InterpolationRangePropertyData.GetData());
						if(StoredRangeValue != nullptr)
						{
							CachedRangeValue = static_cast<uint32>(*StoredRangeValue);				
						}
					}
					else if(InterpolationRangePropertyData.Num() == 2)
					{
						uint16* StoredRangeValue = reinterpret_cast<uint16*>(InterpolationRangePropertyData.GetData());
						if(StoredRangeValue != nullptr)
						{
							CachedRangeValue = static_cast<uint32>(*StoredRangeValue);
						}
					}
					else if(InterpolationRangePropertyData.Num() == 8)
					{
						uint64* StoredRangeValue = reinterpret_cast<uint64*>(InterpolationRangePropertyData.GetData());
						if(StoredRangeValue != nullptr)
						{
							CachedRangeValue = static_cast<uint32>(*StoredRangeValue);
						}
					}

					InterpolationRangePropertyDataCache.SetNumZeroed(sizeof(uint32));
					*reinterpret_cast<uint32*>(InterpolationRangePropertyDataCache.GetData()) = CachedRangeValue;
				}
			}
		}
		else
		{
			InterpolationRangePropertyDataCache = InterpolationRangePropertyData;
		}
	}

	FProperty* Property = CastField<FProperty>(BoundPropertyPath.Get());
	if(const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		const RemoteControlPropertyUtilities::FRCPropertyVariant Src{StructProperty, InterpolationMappingPropertyData, InterpolationMappingPropertyElementNum};
		RemoteControlPropertyUtilities::FRCPropertyVariant Dst{StructProperty, InterpolationMappingPropertyDataCache};
		RemoteControlPropertyUtilities::Deserialize<FStructProperty>(Src, Dst);
	}
	else if(const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		const RemoteControlPropertyUtilities::FRCPropertyVariant Src{ArrayProperty, InterpolationMappingPropertyData, InterpolationMappingPropertyElementNum};
		RemoteControlPropertyUtilities::FRCPropertyVariant Dst{ArrayProperty, InterpolationMappingPropertyDataCache};
		RemoteControlPropertyUtilities::Deserialize<FArrayProperty>(Src, Dst);
	}
	else
	{
		InterpolationMappingPropertyDataCache = InterpolationMappingPropertyData;
	}
}

void FRemoteControlProtocolEntity::Init(URemoteControlPreset* InOwner, FGuid InPropertyId)
{
	Owner = InOwner;
	PropertyId = MoveTemp(InPropertyId);
}

uint8 FRemoteControlProtocolEntity::GetRangePropertySize() const
{
	if (const EName* PropertyType = GetRangePropertyName().ToEName())
	{
		switch (*PropertyType)
		{
		case NAME_Int8Property:
			return sizeof(int8);

		case NAME_Int16Property:
			return sizeof(int16);

		case NAME_IntProperty:
			return sizeof(int32);

		case NAME_Int64Property:
			return sizeof(int64);

		case NAME_ByteProperty:
			return sizeof(uint8);

		case NAME_UInt16Property:
			return sizeof(uint16);

		case NAME_UInt32Property:
			return sizeof(uint32);

		case NAME_UInt64Property:
			return sizeof(uint64);

		case NAME_FloatProperty:
			return sizeof(float);

		case NAME_DoubleProperty:
			return sizeof(double);

		default:
			break;
		}
	}

	checkNoEntry();
	return 0;
}

const FString& FRemoteControlProtocolEntity::GetRangePropertyMaxValue() const
{
	// returns an empty string by default, so the max value isn't clamped.
	static FString Empty = "";
	return Empty;
}

bool FRemoteControlProtocolEntity::ApplyProtocolValueToProperty(double InProtocolValue)
{
	if (Mappings.Num() <= 1)
	{
		UE_LOG(LogRemoteControl, Warning, TEXT("Binding doesn't container any range mappings."));
		return true;
	}

	URemoteControlPreset* Preset = Owner.Get();
	if (!Preset)
	{
		return false;
	}

	TSharedPtr<FRemoteControlProperty> RemoteControlProperty = Preset->GetExposedEntity<FRemoteControlProperty>(PropertyId).Pin();
	if (!RemoteControlProperty.IsValid())
	{
		return false;
	}

	if(!RemoteControlProperty->IsBound())
	{
		UE_LOG(LogRemoteControl, Warning, TEXT("Entity isn't bound to any objects."));
		return true;
	}

	if (RemoteControlProperty->GetActiveMasks() == ERCMask::NoMask)
	{
		return true;
	}

	FProperty* Property = RemoteControlProperty->GetProperty();
	if (!Property)
	{
		return false;
	}

	if (!RemoteControlTypeUtilities::IsSupportedMappingType(Property))
	{
		UE_LOG(LogRemoteControl, Warning, TEXT("Property type %s is unsupported for mapping."), *Property->GetClass()->GetName());
		return true;
	}

	FRCObjectReference ObjectRef;
	ObjectRef.Property = Property;
	ObjectRef.Access = ERCAccess::WRITE_ACCESS;

	const URemoteControlSettings* RemoteControlSettings = GetDefault<URemoteControlSettings>();
	if (RemoteControlSettings->bProtocolsGenerateTransactions)
	{
		ObjectRef.Access = ERCAccess::WRITE_TRANSACTION_ACCESS;
	}

	ObjectRef.PropertyPathInfo = RemoteControlProperty->FieldPathInfo.ToString();

	bool bSuccess = true;
	for (UObject* Object : RemoteControlProperty->GetBoundObjects())
	{
		IRemoteControlModule::Get().ResolveObjectProperty(ObjectRef.Access, Object, ObjectRef.PropertyPathInfo, ObjectRef);

		TSharedPtr<FRCMaskingOperation> MaskingOperation = MakeShared<FRCMaskingOperation>(ObjectRef.PropertyPathInfo, Object);

		if (OverridenMasks == ERCMask::NoMask)
		{
			MaskingOperation->Masks = RemoteControlProperty->GetActiveMasks();
		}
		else
		{
			MaskingOperation->Masks = OverridenMasks;
		}

		// Cache the values.
		IRemoteControlModule::Get().PerformMasking(MaskingOperation.ToSharedRef());

		// Set properties after interpolation
		TArray<uint8> InterpolatedBuffer;
		if (GetInterpolatedPropertyBuffer(Property, InProtocolValue, InterpolatedBuffer))
		{
			FMemoryReader MemoryReader(InterpolatedBuffer);
			FCborStructDeserializerBackend CborStructDeserializerBackend(MemoryReader);
			bSuccess &= IRemoteControlModule::Get().SetObjectProperties(ObjectRef, CborStructDeserializerBackend, ERCPayloadType::Cbor, InterpolatedBuffer);

			// Apply the masked the values.
			IRemoteControlModule::Get().PerformMasking(MaskingOperation.ToSharedRef());
		}
	}

	return bSuccess;
}

ERCBindingStatus FRemoteControlProtocolEntity::ToggleBindingStatus()
{
	if (BindingStatus == ERCBindingStatus::Awaiting)
	{
		BindingStatus = ERCBindingStatus::Bound;
	}
	else if (BindingStatus == ERCBindingStatus::Bound || BindingStatus == ERCBindingStatus::Unassigned)
	{
		BindingStatus = ERCBindingStatus::Awaiting;
	}

	return BindingStatus;
}

void FRemoteControlProtocolEntity::ResetDefaultBindingState()
{
	BindingStatus = ERCBindingStatus::Unassigned;
}

void FRemoteControlProtocolEntity::ClearMask(ERCMask InMaskBit)
{
	OverridenMasks &= ~InMaskBit;
}

void FRemoteControlProtocolEntity::EnableMask(ERCMask InMaskBit)
{
	OverridenMasks |= InMaskBit;
}

bool FRemoteControlProtocolEntity::HasMask(ERCMask InMaskBit) const
{
	return (OverridenMasks & InMaskBit) != ERCMask::NoMask;
}

#if WITH_EDITOR

const FName FRemoteControlProtocolEntity::GetPropertyName(const FName& ForColumnName)
{
	RegisterProperties();

	if (const FName* PropertyName = ColumnsToProperties.Find(ForColumnName))
	{
		return *PropertyName;
	}

	return NAME_None;
}

#endif // WITH_EDITOR

bool FRemoteControlProtocolEntity::GetInterpolatedPropertyBuffer(FProperty* InProperty, double InProtocolValue, TArray<uint8>& OutBuffer)
{
	OutBuffer.Empty();

	TArray<FRemoteControlProtocolEntity::FRangeMappingData> RangeMappingBuffers = GetRangeMappingBuffers();
	bool bSuccess = false;

	// Write interpolated properties to Cbor buffer
	FMemoryWriter MemoryWriter(OutBuffer);
	FCborWriter CborWriter(&MemoryWriter);
	CborWriter.WriteContainerStart(ECborCode::Map, -1/*Indefinite*/);

	// Normalize before apply
	switch (*GetRangePropertyName().ToEName())
	{
	case NAME_Int8Property:
		bSuccess = EntityInterpolation::ApplyProtocolValueToProperty(InProperty, static_cast<int8>(InProtocolValue), RangeMappingBuffers, CborWriter);
		break;
	case NAME_Int16Property:
		bSuccess = EntityInterpolation::ApplyProtocolValueToProperty(InProperty, static_cast<int16>(InProtocolValue), RangeMappingBuffers, CborWriter);
		break;
	case NAME_IntProperty:
		bSuccess = EntityInterpolation::ApplyProtocolValueToProperty(InProperty, static_cast<int32>(InProtocolValue), RangeMappingBuffers, CborWriter);
		break;
	case NAME_Int64Property:
		bSuccess = EntityInterpolation::ApplyProtocolValueToProperty(InProperty, static_cast<int64>(InProtocolValue), RangeMappingBuffers, CborWriter);
		break;
	case NAME_ByteProperty:
		bSuccess = EntityInterpolation::ApplyProtocolValueToProperty(InProperty, static_cast<uint8>(InProtocolValue), RangeMappingBuffers, CborWriter);
		break;
	case NAME_UInt16Property:
		bSuccess = EntityInterpolation::ApplyProtocolValueToProperty(InProperty, static_cast<uint16>(InProtocolValue), RangeMappingBuffers, CborWriter);
		break;
	case NAME_UInt32Property:
		bSuccess = EntityInterpolation::ApplyProtocolValueToProperty(InProperty, static_cast<uint32>(InProtocolValue), RangeMappingBuffers, CborWriter);
		break;
	case NAME_UInt64Property:
		bSuccess = EntityInterpolation::ApplyProtocolValueToProperty(InProperty, static_cast<uint64>(InProtocolValue), RangeMappingBuffers, CborWriter);
		break;
	case NAME_FloatProperty:
		bSuccess = EntityInterpolation::ApplyProtocolValueToProperty(InProperty, static_cast<float>(InProtocolValue), RangeMappingBuffers, CborWriter);
		break;
	case NAME_DoubleProperty:
		bSuccess = EntityInterpolation::ApplyProtocolValueToProperty(InProperty, static_cast<double>(InProtocolValue), RangeMappingBuffers, CborWriter);
		break;
	default:
		checkNoEntry();
	}

	CborWriter.WriteContainerEnd();

	return bSuccess;
}

TArray<FRemoteControlProtocolEntity::FRangeMappingData> FRemoteControlProtocolEntity::GetRangeMappingBuffers()
{
	TArray<FRemoteControlProtocolEntity::FRangeMappingData> RangeMappingBuffers;
	RangeMappingBuffers.Reserve(Mappings.Num());

	for (FRemoteControlProtocolMapping& Mapping : Mappings)
	{
		if (Mapping.InterpolationMappingPropertyDataCache.Num() == 0
			|| Mapping.InterpolationRangePropertyDataCache.Num() == 0)
		{
			Mapping.RefreshCachedData(GetRangePropertyName());
		}

		RangeMappingBuffers.Emplace(
			Mapping.InterpolationRangePropertyDataCache,
			Mapping.InterpolationMappingPropertyDataCache,
			Mapping.InterpolationMappingPropertyElementNum);
	}

	return RangeMappingBuffers;
}

// Optionally supplying the BindingId is used by the undo system
FRemoteControlProtocolBinding::FRemoteControlProtocolBinding(const FName InProtocolName, const FGuid& InPropertyId, TSharedPtr<TStructOnScope<FRemoteControlProtocolEntity>> InRemoteControlProtocolEntityPtr, const FGuid& InBindingId)
	: Id(InBindingId)
	, ProtocolName(InProtocolName)
	, PropertyId(InPropertyId)
	, RemoteControlProtocolEntityPtr(InRemoteControlProtocolEntityPtr)
{}

bool FRemoteControlProtocolBinding::operator==(const FRemoteControlProtocolBinding& InProtocolBinding) const
{
	return Id == InProtocolBinding.Id;
}

bool FRemoteControlProtocolBinding::operator==(FGuid InProtocolBindingId) const
{
	return Id == InProtocolBindingId;
}

int32 FRemoteControlProtocolBinding::RemoveMapping(const FGuid& InMappingId)
{
	if (FRemoteControlProtocolEntity* ProtocolEntity = GetRemoteControlProtocolEntity())
	{
		return ProtocolEntity->Mappings.RemoveByHash(GetTypeHash(InMappingId), InMappingId);
	}

	return ensure(0);
}

void FRemoteControlProtocolBinding::ClearMappings()
{
	if (FRemoteControlProtocolEntity* ProtocolEntity = GetRemoteControlProtocolEntity())
	{
		ProtocolEntity->Mappings.Empty();
		return;
	}

	ensure(false);
}

void FRemoteControlProtocolBinding::AddMapping(const FRemoteControlProtocolMapping& InMappingsData)
{
	if (FRemoteControlProtocolEntity* ProtocolEntity = GetRemoteControlProtocolEntity())
	{
		ProtocolEntity->Mappings.Add(InMappingsData);
		return;
	}

	ensure(false);
}

void FRemoteControlProtocolBinding::ForEachMapping(FGetProtocolMappingCallback InCallback)
{
	if (FRemoteControlProtocolEntity* ProtocolEntity = GetRemoteControlProtocolEntity())
	{
		for (FRemoteControlProtocolMapping& Mapping : ProtocolEntity->Mappings)
		{
			InCallback(Mapping);
		}
	}
}

bool FRemoteControlProtocolBinding::SetPropertyDataToMapping(const FGuid& InMappingId, const void* InPropertyValuePtr)
{
	if (FRemoteControlProtocolMapping* Mapping = FindMapping(InMappingId))
	{
		FMemory::Memcpy(Mapping->InterpolationMappingPropertyData.GetData(), InPropertyValuePtr, Mapping->InterpolationMappingPropertyData.Num());
		return true;
	}

	return false;
}

FRemoteControlProtocolMapping* FRemoteControlProtocolBinding::FindMapping(const FGuid& InMappingId)
{
	if (FRemoteControlProtocolEntity* ProtocolEntity = GetRemoteControlProtocolEntity())
	{
		return ProtocolEntity->Mappings.FindByHash(GetTypeHash(InMappingId), InMappingId);
	}

	ensure(false);
	return nullptr;
}

TSharedPtr<FStructOnScope> FRemoteControlProtocolBinding::GetStructOnScope() const
{
	return RemoteControlProtocolEntityPtr;
}

FRemoteControlProtocolEntity* FRemoteControlProtocolBinding::GetRemoteControlProtocolEntity()
{
	if (RemoteControlProtocolEntityPtr.IsValid())
	{
		return RemoteControlProtocolEntityPtr->Get();
	}
	return nullptr;
}

bool FRemoteControlProtocolBinding::Serialize(FArchive& Ar)
{
	if (Ar.IsLoading() || Ar.IsSaving())
	{
		Ar << *this;
	}

	return true;
}

FArchive& operator<<(FArchive& Ar, FRemoteControlProtocolBinding& InProtocolBinding)
{
	UScriptStruct* ScriptStruct = FRemoteControlProtocolBinding::StaticStruct();

	ScriptStruct->SerializeTaggedProperties(Ar, (uint8*)&InProtocolBinding, ScriptStruct, nullptr);

	// Serialize TStructOnScope
	if (Ar.IsLoading())
	{
		InProtocolBinding.RemoteControlProtocolEntityPtr = MakeShared<TStructOnScope<FRemoteControlProtocolEntity>>();
		Ar << *InProtocolBinding.RemoteControlProtocolEntityPtr;
	}
	else if (Ar.IsSaving())
	{
		TSharedPtr<TStructOnScope<FRemoteControlProtocolEntity>> EntityPtr = InProtocolBinding.RemoteControlProtocolEntityPtr;

		if (FRemoteControlProtocolEntity* ProtocolEntity = InProtocolBinding.GetRemoteControlProtocolEntity())
		{
			Ar << *EntityPtr;
		}
	}

	return Ar;
}

uint32 GetTypeHash(const FRemoteControlProtocolMapping& InProtocolMapping)
{
	return GetTypeHash(InProtocolMapping.Id);
}

uint32 GetTypeHash(const FRemoteControlProtocolBinding& InProtocolBinding)
{
	return GetTypeHash(InProtocolBinding.Id);
}

#undef LOCTEXT_NAMESPACE
