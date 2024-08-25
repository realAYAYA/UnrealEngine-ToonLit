// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGModule.h"
#include "PCGParamData.h"
#include "PCGSettings.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/PCGMetadataAttributeTraits.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

class UPCGComponent;
class UPCGNode;
class UPCGPin;
struct FPCGDataCollection;

namespace PCGSettingsHelpers
{
	/** Utility function to get the value of type T from a param data or a default value
	* @param InName - Attribute to get from the param
	* @param InValue - Default value to return if the param doesn't have the given attribute
	* @param InParams - ParamData to get the value from.
	* @param InKey - Metadata Entry Key to get the value from.
	*/
	template<typename T, typename TEnableIf<!TIsEnumClass<T>::Value>::Type* = nullptr>
	UE_DEPRECATED(5.2, "GetValue is deprecated as overrides are now automatically applied on the settings. You should just query the settings value.")
	T GetValue(const FName& InName, const T& InValue, const UPCGParamData* InParams, PCGMetadataEntryKey InKey)
	{

		if (InParams && InParams->Metadata)
		{
			const FPCGMetadataAttributeBase* MatchingAttribute = InParams->Metadata->GetConstAttribute(InName);

			if (!MatchingAttribute)
			{
				return InValue;
			}

			auto GetTypedValue = [MatchingAttribute, &InValue, InKey](auto DummyValue) -> T
			{
				using AttributeType = decltype(DummyValue);

				const FPCGMetadataAttribute<AttributeType>* ParamAttribute = static_cast<const FPCGMetadataAttribute<AttributeType>*>(MatchingAttribute);

				if constexpr (std::is_same_v<T, AttributeType>)
				{
					return ParamAttribute->GetValueFromItemKey(InKey);
				}
				else if constexpr (std::is_constructible_v<T, AttributeType>)
				{
					UE_LOG(LogPCG, Verbose, TEXT("[GetAttributeValue] Matching attribute was found, but not the right type. Implicit conversion done (%d vs %d)"), MatchingAttribute->GetTypeId(), PCG::Private::MetadataTypes<T>::Id);
					return T(ParamAttribute->GetValueFromItemKey(InKey));
				}
				else
				{
					UE_LOG(LogPCG, Error, TEXT("[GetAttributeValue] Matching attribute was found, but not the right type. %d vs %d"), MatchingAttribute->GetTypeId(), PCG::Private::MetadataTypes<T>::Id);
					return InValue;
				}
			};

			return PCGMetadataAttribute::CallbackWithRightType(MatchingAttribute->GetTypeId(), GetTypedValue);
		}
		else
		{
			return InValue;
		}
	}

PRAGMA_DISABLE_DEPRECATION_WARNINGS

	template<typename T, typename TEnableIf<!TIsEnumClass<T>::Value>::Type* = nullptr>
	UE_DEPRECATED(5.2, "GetValue is deprecated as overrides are now automatically applied on the settings. You should just query the settings value.")
	T GetValue(const FName& InName, const T& InValue, const UPCGParamData* InParams)
	{
		return GetValue(InName, InValue, InParams, 0);
	}

	template<typename T, typename TEnableIf<!TIsEnumClass<T>::Value>::Type* = nullptr>
	UE_DEPRECATED(5.2, "GetValue is deprecated as overrides are now automatically applied on the settings. You should just query the settings value.")
	T GetValue(const FName& InName, const T& InValue, const UPCGParamData* InParams, const FName& InParamName)
	{
		if (InParams && InParamName != NAME_None)
		{
			return GetValue(InName, InValue, InParams, InParams->FindMetadataKey(InParamName));
		}
		else
		{
			return InValue;
		}
	}

	/** Specialized versions for enums */
	template<typename T, typename TEnableIf<TIsEnumClass<T>::Value>::Type* = nullptr>
	UE_DEPRECATED(5.2, "GetValue is deprecated as overrides are now automatically applied on the settings. You should just query the settings value.")
	T GetValue(const FName& InName, const T& InValue, const UPCGParamData* InParams, PCGMetadataEntryKey InKey)
	{
		return static_cast<T>(GetValue(InName, static_cast<__underlying_type(T)>(InValue), InParams, InKey));
	}

	template<typename T, typename TEnableIf<TIsEnumClass<T>::Value>::Type* = nullptr>
	UE_DEPRECATED(5.2, "GetValue is deprecated as overrides are now automatically applied on the settings. You should just query the settings value.")
	T GetValue(const FName& InName, const T& InValue, const UPCGParamData* InParams)
	{
		return static_cast<T>(GetValue(InName, static_cast<__underlying_type(T)>(InValue), InParams));
	}

	template<typename T, typename TEnableIf<TIsEnumClass<T>::Value>::Type* = nullptr>
	UE_DEPRECATED(5.2, "GetValue is deprecated as overrides are now automatically applied on the settings. You should just query the settings value.")
	T GetValue(const FName& InName, const T& InValue, const UPCGParamData* InParams, const FName& InParamName)
	{
		return static_cast<T>(GetValue(InName, static_cast<__underlying_type(T)>(InValue), InParams, InParamName));
	}

	/** Specialized version for names */
	template<>
	UE_DEPRECATED(5.2, "GetValue is deprecated as overrides are now automatically applied on the settings. You should just query the settings value.")
	FORCEINLINE FName GetValue(const FName& InName, const FName& InValue, const UPCGParamData* InParams, PCGMetadataEntryKey InKey)
	{
		return FName(GetValue(InName, InValue.ToString(), InParams, InKey));
	}

	template<>
	UE_DEPRECATED(5.2, "GetValue is deprecated as overrides are now automatically applied on the settings. You should just query the settings value.")
	FORCEINLINE FName GetValue(const FName& InName, const FName& InValue, const UPCGParamData* InParams)
	{
		return FName(GetValue(InName, InValue.ToString(), InParams));
	}

	template<>
	UE_DEPRECATED(5.2, "GetValue is deprecated as overrides are now automatically applied on the settings. You should just query the settings value.")
	FORCEINLINE FName GetValue(const FName& InName, const FName& InValue, const UPCGParamData* InParams, const FName& InParamName)
	{
		return FName(GetValue(InName, InValue.ToString(), InParams, InParamName));
	}

	/** Sets data from the params to a given property, matched on a name basis */
	void SetValue(UPCGParamData* Params, UObject* Object, FProperty* Property);

	UE_DEPRECATED(5.2, "ComputeSeedWithOverride is deprecated as overrides are now automatically applied on the settings. You should just query the settings value.")
	int ComputeSeedWithOverride(const UPCGSettings* InSettings, const UPCGComponent* InComponent, UPCGParamData* InParams);

	UE_DEPRECATED(5.2, "ComputeSeedWithOverride is deprecated as overrides are now automatically applied on the settings. You should just query the settings value.")
	FORCEINLINE int ComputeSeedWithOverride(const UPCGSettings* InSettings, TWeakObjectPtr<UPCGComponent> InComponent, UPCGParamData* InParams)
	{
		return ComputeSeedWithOverride(InSettings, InComponent.Get(), InParams);
	}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** Utility to call from before-node-update deprecation. A dedicated pin for params will be added when the pins are updated. Here we detect any params
	*   connections to the In pin and disconnect them, and move the first params connection to a new params pin.
	*/
	void DeprecationBreakOutParamsToNewPin(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins);

	/**
	* Advanced method to gather override params when you don't have access to FPCGContext (and therefore don't have access to automatic
	* param override).
	* Limitation: Only support metadata types for T.
	*/
	template <typename T>
	bool GetOverrideValue(const FPCGDataCollection& InInputData, const UPCGSettings* InSettings, const FName InPropertyName, const T& InDefaultValue, T& OutValue)
	{
		check(InSettings);

		// Limitation: Only support metadata types
		static_assert(PCG::Private::IsPCGType<T>());

		// Try to find the override param associated with the property.
		const FPCGSettingsOverridableParam* Param = InSettings->OverridableParams().FindByPredicate([InPropertyName](const FPCGSettingsOverridableParam& InParam) { return !InParam.PropertiesNames.IsEmpty() && (InParam.PropertiesNames.Last() == InPropertyName);});

		OutValue = InDefaultValue;

		if (!Param)
		{
			return false;
		}

		PCGAttributeAccessorHelpers::AccessorParamResult AccessorResult{};
		TUniquePtr<const IPCGAttributeAccessor> AttributeAccessor = PCGAttributeAccessorHelpers::CreateConstAccessorForOverrideParamWithResult(InInputData, *Param, &AccessorResult);

		const FName AttributeName = AccessorResult.AttributeName;

		if (!AttributeAccessor)
		{
			return false;
		}

		return PCGMetadataAttribute::CallbackWithRightType(PCG::Private::MetadataTypes<T>::Id, [&AttributeAccessor, &Param, &AttributeName, &InDefaultValue, &OutValue](auto Dummy) -> bool
		{
			using PropertyType = decltype(Dummy);

			// Override were using the first entry (0) by default.
			FPCGAttributeAccessorKeysEntries FirstEntry(PCGMetadataEntryKey(0));

			if (!AttributeAccessor->Get<T>(OutValue, FirstEntry, EPCGAttributeAccessorFlags::AllowBroadcast | EPCGAttributeAccessorFlags::AllowConstructible))
			{
				UE_LOG(LogPCG, Warning, TEXT("[PCGSettingsHelpers::GetOverrideValue] '%s' parameter cannot be converted from '%s' attribute, incompatible types."), *Param->Label.ToString(), *AttributeName.ToString());
				return false;
			}

			return true;
		});
	}

	struct FPCGGetAllOverridableParamsConfig
	{
		// If we don't use the seed, don't add it as override.
		bool bUseSeed = false;

		// Don't look for properties from parents
		bool bExcludeSuperProperties = false;

#if WITH_EDITOR
		// List of metadata values to find in property metadata. Only works in editor builds as metadata on property is not available elsewise.
		TArray<FName> IncludeMetadataValues;

		// If the metadata values is a conjunction (all values need to be present to keep), or disjunction (any values needs to be present to keep)
		bool bIncludeMetadataIsConjunction = false;

		// List of metadata values to find in property metadata. Only works in editor builds as metadata on property is not available elsewise.
		TArray<FName> ExcludeMetadataValues;

		// If the exclude values is a conjunction (all values need to be present to discard), or disjunction (any values needs to be present to discard)
		bool bExcludeMetadataIsConjunction = false;
#endif // WITH_EDITOR

		// Flags to exclude in property flags
		uint64 ExcludePropertyFlags = 0;

		// If the exclude flags is a conjunction (all flags need to be present to discard), or disjunction (any flag needs to be present to discard)
		bool bExcludePropertyFlagsIsConjunction = false;

		// Flags to include in property flags. Note that if exclusion says discard, it will be discarded.
		uint64 IncludePropertyFlags = 0;

		// If the include flags is a conjunction (all flags need to be present to keep), or disjunction (any flag needs to be present to keep)
		bool bIncludePropertyFlagsIsConjunction = false;

		// Max depth for structs of structs. -1 = no limit
		int32 MaxStructDepth = -1;

		// If you want to go through objects too
		bool bExtractObjects = false;

		// If you want to also extract arrays
		bool bExtractArrays = false;
	};

	PCG_API TArray<FPCGSettingsOverridableParam> GetAllOverridableParams(const UStruct* InClass, const FPCGGetAllOverridableParamsConfig& InConfig);
}

// Deprecated macro, not necessary anymore. Cf. GetValue
#define PCG_GET_OVERRIDEN_VALUE(Settings, Variable, Params) PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(std::remove_pointer_t<std::remove_const_t<decltype(Settings)>>, Variable), (Settings)->Variable, Params)
