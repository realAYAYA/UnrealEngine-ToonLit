// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieScenePropertySystemTypes.h"
#include "EntitySystem/MovieSceneOperationalTypeConversions.h"
#include "MovieSceneCommonHelpers.h"


namespace UE
{
namespace MovieScene
{

/**
 * Property accessor traits that talk directly to the reflected UObject property type
 */
template<typename ...MetaDataTypes>
struct TPropertyMetaData
{
	static constexpr int32 Num = sizeof...(MetaDataTypes);
};


/**
 * Property accessor traits that talk directly to the reflected UObject property type
 */
template<typename UObjectPropertyType, bool bInIsComposite = true>
struct TDirectPropertyTraits
{
	static constexpr bool bIsComposite = bInIsComposite;

	using StorageType  = UObjectPropertyType;
	using MetaDataType = TPropertyMetaData<>;
	using TraitsType   = TDirectPropertyTraits<UObjectPropertyType>;

	using ParamType = typename TCallTraits<UObjectPropertyType>::ParamType;

	/** Property Value Getters  */
	static void GetObjectPropertyValue(const UObject* InObject, const FCustomPropertyAccessor& BaseCustomAccessor, StorageType& OutValue)
	{
		const TCustomPropertyAccessor<TraitsType>& CustomAccessor = static_cast<const TCustomPropertyAccessor<TraitsType>&>(BaseCustomAccessor);
		OutValue = (*CustomAccessor.Functions.Getter)(InObject);
	}
	static void GetObjectPropertyValue(const UObject* InObject, uint16 PropertyOffset, StorageType& OutValue)
	{
		const UObjectPropertyType* PropertyAddress = reinterpret_cast<const UObjectPropertyType*>( reinterpret_cast<const uint8*>(InObject) + PropertyOffset );
		OutValue = *PropertyAddress;
	}
	static void GetObjectPropertyValue(const UObject* InObject, FTrackInstancePropertyBindings* PropertyBindings, StorageType& OutValue)
	{
		OutValue = PropertyBindings->GetCurrentValue<UObjectPropertyType>(*InObject);
	}
	static void GetObjectPropertyValue(const UObject* InObject, const FName& PropertyPath, StorageType& OutValue)
	{
		TOptional<UObjectPropertyType> Property = FTrackInstancePropertyBindings::StaticValue<UObjectPropertyType>(InObject, *PropertyPath.ToString());
		if (Property)
		{
			OutValue = MoveTemp(Property.GetValue());
		}
	}

	/** Property Value Setters  */
	static void SetObjectPropertyValue(UObject* InObject, const FCustomPropertyAccessor& BaseCustomAccessor, ParamType InValue)
	{
		const TCustomPropertyAccessor<TraitsType>& CustomAccessor = static_cast<const TCustomPropertyAccessor<TraitsType>&>(BaseCustomAccessor);
		(*CustomAccessor.Functions.Setter)(InObject, InValue);
	}
	static void SetObjectPropertyValue(UObject* InObject, uint16 PropertyOffset, ParamType InValue)
	{
		UObjectPropertyType* PropertyAddress = reinterpret_cast<UObjectPropertyType*>( reinterpret_cast<uint8*>(InObject) + PropertyOffset );
		*PropertyAddress = InValue;
	}
	static void SetObjectPropertyValue(UObject* InObject, FTrackInstancePropertyBindings* PropertyBindings, ParamType InValue)
	{
		PropertyBindings->CallFunction<UObjectPropertyType>(*InObject, InValue);
	}

	template<typename ...T>
	static StorageType CombineComposites(T&&... InComposites)
	{
		return StorageType{ Forward<T>(InComposites)... };
	}
};


/**
 * Property accessor traits that talk directly to the reflected UObject property type
 */
template<typename UObjectPropertyType, typename InMemoryType, bool bInIsComposite = true>
struct TIndirectPropertyTraits
{
	static constexpr bool bIsComposite = bInIsComposite;

	using StorageType  = InMemoryType;
	using MetaDataType = TPropertyMetaData<>;
	using TraitsType   = TIndirectPropertyTraits<UObjectPropertyType, InMemoryType>;

	using PropertyParamType    = typename TCallTraits<UObjectPropertyType>::ParamType;
	using OperationalParamType = typename TCallTraits<StorageType>::ParamType;

	/** Property Value Getters  */
	static void GetObjectPropertyValue(const UObject* InObject, const FCustomPropertyAccessor& BaseCustomAccessor, StorageType& OutValue)
	{
		const TCustomPropertyAccessor<TraitsType>& CustomAccessor = static_cast<const TCustomPropertyAccessor<TraitsType>&>(BaseCustomAccessor);

		OutValue = (*CustomAccessor.Functions.Getter)(InObject);
	}
	static void GetObjectPropertyValue(const UObject* InObject, uint16 PropertyOffset, StorageType& OutValue)
	{
		const UObjectPropertyType* PropertyAddress = reinterpret_cast<const UObjectPropertyType*>( reinterpret_cast<const uint8*>(InObject) + PropertyOffset );
		ConvertOperationalProperty(*PropertyAddress, OutValue);
	}
	static void GetObjectPropertyValue(const UObject* InObject, FTrackInstancePropertyBindings* PropertyBindings, StorageType& OutValue)
	{
		UObjectPropertyType Value = PropertyBindings->GetCurrentValue<UObjectPropertyType>(*InObject);
		ConvertOperationalProperty(Value, OutValue);
	}

	static void GetObjectPropertyValue(const UObject* InObject, const FName& PropertyPath, StorageType& OutValue)
	{
		TOptional<UObjectPropertyType> Property = FTrackInstancePropertyBindings::StaticValue<UObjectPropertyType>(InObject, *PropertyPath.ToString());
		if (Property)
		{
			ConvertOperationalProperty(Property.GetValue(), OutValue);
		}
	}

	/** Property Value Setters  */
	static void SetObjectPropertyValue(UObject* InObject, const FCustomPropertyAccessor& BaseCustomAccessor, OperationalParamType InValue)
	{
		const TCustomPropertyAccessor<TraitsType>& CustomAccessor = static_cast<const TCustomPropertyAccessor<TraitsType>&>(BaseCustomAccessor);
		(*CustomAccessor.Functions.Setter)(InObject, InValue);
	}
	static void SetObjectPropertyValue(UObject* InObject, uint16 PropertyOffset, OperationalParamType InValue)
	{
		UObjectPropertyType* PropertyAddress = reinterpret_cast<UObjectPropertyType*>( reinterpret_cast<uint8*>(InObject) + PropertyOffset );
		ConvertOperationalProperty(InValue, *PropertyAddress);
	}
	static void SetObjectPropertyValue(UObject* InObject, FTrackInstancePropertyBindings* PropertyBindings, OperationalParamType InValue)
	{
		UObjectPropertyType NewValue{};
		ConvertOperationalProperty(InValue, NewValue);

		PropertyBindings->CallFunction<UObjectPropertyType>(*InObject, NewValue);
	}

	template<typename ...T>
	static StorageType CombineComposites(T&&... InComposites)
	{
		return StorageType{ Forward<T>(InComposites)... };
	}
};




/**
 * Property accessor traits that do not know the underlying UObjectPropertyType until runtime
 */
template<typename RuntimeType, typename ...MetaDataTypes>
struct TRuntimePropertyTraits
{
	using StorageType  = RuntimeType;
	using MetaDataType = TPropertyMetaData<MetaDataTypes...>;

	using OperationalParamType = typename TCallTraits<StorageType>::ParamType;

	/** Property Value Getters  */
	static void GetObjectPropertyValue(const UObject* InObject, const FCustomPropertyAccessor& BaseCustomAccessor, typename TCallTraits<MetaDataTypes>::ParamType... MetaData, StorageType& OutValue)
	{}

	static void GetObjectPropertyValue(const UObject* InObject, uint16 PropertyOffset, typename TCallTraits<MetaDataTypes>::ParamType... MetaData, StorageType& OutValue)
	{}

	static void GetObjectPropertyValue(const UObject* InObject, FTrackInstancePropertyBindings* PropertyBindings, typename TCallTraits<MetaDataTypes>::ParamType... MetaData, StorageType& OutValue)
	{}

	static void GetObjectPropertyValue(const UObject* InObject, const FName& PropertyPath, StorageType& OutValue)
	{}

	/** Property Value Setters  */
	static void SetObjectPropertyValue(UObject* InObject, const FCustomPropertyAccessor& BaseCustomAccessor, typename TCallTraits<MetaDataTypes>::ParamType... MetaData, OperationalParamType InValue)
	{}

	static void SetObjectPropertyValue(UObject* InObject, uint16 PropertyOffset, typename TCallTraits<MetaDataTypes>::ParamType... MetaData, OperationalParamType InValue)
	{}

	static void SetObjectPropertyValue(UObject* InObject, FTrackInstancePropertyBindings* PropertyBindings, typename TCallTraits<MetaDataTypes>::ParamType... MetaData, OperationalParamType InValue)
	{}
};


} // namespace MovieScene
} // namespace UE


