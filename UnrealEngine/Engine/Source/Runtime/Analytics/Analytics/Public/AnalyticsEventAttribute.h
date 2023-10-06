// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnalyticsConversion.h"

struct FJsonNull
{
};

FORCEINLINE const TCHAR* LexToString(FJsonNull)
{
	return TEXT("null");
}

struct FJsonFragment
{
	explicit FJsonFragment(FString&& StringRef) : FragmentString(MoveTemp(StringRef)) {}
	FString FragmentString;
};

FORCEINLINE const FString& LexToString(const FJsonFragment& Fragment)
{
	return Fragment.FragmentString;
}

FORCEINLINE FString LexToString(FJsonFragment&& Fragment)
{
	return MoveTemp(Fragment.FragmentString);
}



/**
 * Struct to hold key/value pairs that will be sent as attributes along with analytics events.
 * All values are actually strings, but we provide a convenient constructor that relies on ToStringForAnalytics() to 
 * convert common types. 
 */
struct FAnalyticsEventAttribute
{
	UE_DEPRECATED(4.26, "This property has been deprecated, use GetName() instead")
	const FString AttrName;

	UE_DEPRECATED(4.26, "This property has been deprecated, use GetValue() instead")
	const FString AttrValueString;
	UE_DEPRECATED(4.26, "This property has been deprecated, use GetValue() instead. You cannot recover the original non-string value anymore")
	const double AttrValueNumber;
	UE_DEPRECATED(4.26, "This property has been deprecated, use GetValue() instead. You cannot recover the original non-string value anymore")
	const bool AttrValueBool;

	enum class AttrTypeEnum
	{
		String,
		Number,
		Boolean,
		Null,
		JsonFragment
	};
	UE_DEPRECATED(4.26, "This property has been deprecated, use IsJsonFragment or GetValue instead")
	const AttrTypeEnum AttrType;

	template <typename ValueType>
	FAnalyticsEventAttribute(FString InName, ValueType&& InValue);

	const FString& GetName() const;
	const FString& GetValue() const;
	bool IsJsonFragment() const;

	/** Allow setting value for any type that supports LexToString */
	template<typename ValueType>
	void SetValue(ValueType&& InValue);

	/** Default ctor since we declare a custom ctor. */
	FAnalyticsEventAttribute();
	~FAnalyticsEventAttribute();

	/** Reinstate the default copy ctor because that one still works fine. */
	FAnalyticsEventAttribute(const FAnalyticsEventAttribute& RHS);

	/** Hack to allow copy ctor using an rvalue-ref. This class only "sort of" acts like an immutable class because the const members prevents assignment, which was not intended when this code was changed. */
	FAnalyticsEventAttribute(FAnalyticsEventAttribute&& RHS);

	/** Hack to allow assignment. This class only "sort of" acts like an immutable class because the const members prevents assignment, which was not intended when this code was changed. */
	FAnalyticsEventAttribute& operator=(const FAnalyticsEventAttribute& RHS);

	/** Hack to allow assignment. This class only "sort of" acts like an immutable class because the const members prevents assignment, which was not intended when this code was changed. */
	FAnalyticsEventAttribute& operator=(FAnalyticsEventAttribute&& RHS);

	/** ALlow aggregation of attributes */
	FAnalyticsEventAttribute& operator+=(const FAnalyticsEventAttribute& RHS);

	/** ALlow aggregation of attributes */
	FAnalyticsEventAttribute& operator+(const FAnalyticsEventAttribute& RHS);

	/** If you need the old AttrValue behavior (i.e. stringify everything), call this function instead. */
	UE_DEPRECATED(4.26, "This property has been deprecated, use GetValue() instead")
	FString ToString() const;

	/** Legacy support for old RecordEventJson API. Don't call this directly. */
	UE_DEPRECATED(4.26, "This property is used to support the deprecated APIs, construct Json values using FJsonFragment instead")
	void SwitchToJsonFragment();

	static bool IsValidAttributeName(const FString& InName)
	{
		return !InName.IsEmpty() && InName != TEXT("EventName") && InName != TEXT("DateOffset");
	}

private:
	FString& CheckName(FString& InName)
	{
		// These are reserved names in our environment. Enforce things don't use it.
		check(IsValidAttributeName(InName));
		return InName;
	}
};


// The implementation of this class references deprecated members. Don't fire warnings for these.
// For this reason we actually implement the entire class out-of-line, but still in the header files, so we can wrap
// all the implementations in DISABLE macro easily.
PRAGMA_DISABLE_DEPRECATION_WARNINGS

inline FAnalyticsEventAttribute::FAnalyticsEventAttribute() 
: AttrName()
, AttrValueString()
, AttrValueNumber(0)
, AttrValueBool(false)
, AttrType(AttrTypeEnum::String)
{

}

inline FAnalyticsEventAttribute::~FAnalyticsEventAttribute() = default;

inline FAnalyticsEventAttribute::FAnalyticsEventAttribute(const FAnalyticsEventAttribute& RHS) = default;

inline FAnalyticsEventAttribute::FAnalyticsEventAttribute(FAnalyticsEventAttribute&& RHS) : AttrName(MoveTemp(const_cast<FString&>(RHS.AttrName)))
, AttrValueString(MoveTemp(const_cast<FString&>(RHS.AttrValueString)))
// no need to use MoveTemp on intrinsic types.
, AttrValueNumber(RHS.AttrValueNumber)
, AttrValueBool(RHS.AttrValueBool)
, AttrType(RHS.AttrType)
{

}

template <typename ValueType>
inline FAnalyticsEventAttribute::FAnalyticsEventAttribute(FString InName, ValueType&& InValue)
: AttrName(MoveTemp(CheckName(InName)))
, AttrValueString(AnalyticsConversionToString(Forward<ValueType>(InValue)))
, AttrValueNumber(0)
, AttrValueBool(false)
, AttrType(TIsArithmetic<typename TDecay<ValueType>::Type>::Value || std::is_same_v<typename TDecay<ValueType>::Type, FJsonNull> || std::is_same_v<typename TDecay<ValueType>::Type, FJsonFragment> ? AttrTypeEnum::JsonFragment : AttrTypeEnum::String)
{

}


inline FAnalyticsEventAttribute& FAnalyticsEventAttribute::operator=(const FAnalyticsEventAttribute& RHS)
{
	if (&RHS == this)
	{
		return *this;
	}

	const_cast<FString&>(AttrName) = RHS.AttrName;
	const_cast<FString&>(AttrValueString) = RHS.AttrValueString;
	const_cast<double&>(AttrValueNumber) = RHS.AttrValueNumber;
	const_cast<bool&>(AttrValueBool) = RHS.AttrValueBool;
	const_cast<AttrTypeEnum&>(AttrType) = RHS.AttrType;
	return *this;
}

inline FAnalyticsEventAttribute& FAnalyticsEventAttribute::operator+=(const FAnalyticsEventAttribute& RHS)
{
	return *this+RHS;
}

inline FAnalyticsEventAttribute& FAnalyticsEventAttribute::operator+(const FAnalyticsEventAttribute& RHS)
{
	if (&RHS == this)
	{
		return *this;
	}

	const_cast<double&>(AttrValueNumber) += RHS.AttrValueNumber;
	return *this;
}

inline FAnalyticsEventAttribute& FAnalyticsEventAttribute::operator=(FAnalyticsEventAttribute&& RHS)
{
	if (&RHS == this)
	{
		return *this;
	}

	const_cast<FString&>(AttrName) = MoveTemp(const_cast<FString&>(RHS.AttrName));
	const_cast<FString&>(AttrValueString) = MoveTemp(const_cast<FString&>(RHS.AttrValueString));
	// no need to use MoveTemp on intrinsic types.
	const_cast<double&>(AttrValueNumber) = RHS.AttrValueNumber;
	const_cast<bool&>(AttrValueBool) = RHS.AttrValueBool;
	const_cast<AttrTypeEnum&>(AttrType) = RHS.AttrType;
	return *this;
}

inline FString FAnalyticsEventAttribute::ToString() const
{
	return GetValue();
}

inline const FString& FAnalyticsEventAttribute::GetName() const
{
	return AttrName;
}

inline const FString& FAnalyticsEventAttribute::GetValue() const
{
	return AttrValueString;
}

inline bool FAnalyticsEventAttribute::IsJsonFragment() const
{
	return AttrType == AttrTypeEnum::JsonFragment;
}

template<typename ValueType>
inline void FAnalyticsEventAttribute::SetValue(ValueType&& InValue)
{
	const_cast<FString&>(AttrValueString) = AnalyticsConversionToString(Forward<ValueType>(InValue));
	const_cast<AttrTypeEnum&>(AttrType) = TIsArithmetic<typename TDecay<ValueType>::Type>::Value || std::is_same_v<typename TDecay<ValueType>::Type, FJsonNull> || std::is_same_v<typename TDecay<ValueType>::Type, FJsonFragment> ? AttrTypeEnum::JsonFragment : AttrTypeEnum::String;
}

inline void FAnalyticsEventAttribute::SwitchToJsonFragment()
{
	const_cast<AttrTypeEnum&>(AttrType) = AttrTypeEnum::JsonFragment;
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS


/** Helper functions for MakeAnalyticsEventAttributeArray. */
namespace ImplMakeAnalyticsEventAttributeArray
{
	/** Recursion terminator. Empty list. */
	template <typename Allocator>
	FORCEINLINE void MakeArray(TArray<FAnalyticsEventAttribute, Allocator>& Attrs)
	{
	}

	/** Recursion terminator. Convert the key/value pair to analytics strings. */
	template <typename Allocator, typename KeyType, typename ValueType>
	FORCEINLINE void MakeArray(TArray<FAnalyticsEventAttribute, Allocator>& Attrs, KeyType&& Key, ValueType&& Value)
	{
		Attrs.Emplace(Forward<KeyType>(Key), Forward<ValueType>(Value));
	}

	/** recursively add the arguments to the array. */
	template <typename Allocator, typename KeyType, typename ValueType, typename...ArgTypes>
	FORCEINLINE void MakeArray(TArray<FAnalyticsEventAttribute, Allocator>& Attrs, KeyType&& Key, ValueType&& Value, ArgTypes&&...Args)
	{
		// pop off the top two args and recursively apply the rest.
		Attrs.Emplace(Forward<KeyType>(Key), Forward<ValueType>(Value));
		MakeArray(Attrs, Forward<ArgTypes>(Args)...);
	}
}

/** Helper to create an array of attributes using a single expression. Reserves the necessary space in advance. There must be an even number of arguments, one for each key/value pair. */
template <typename Allocator = FDefaultAllocator, typename...ArgTypes>
FORCEINLINE TArray<FAnalyticsEventAttribute, Allocator> MakeAnalyticsEventAttributeArray(ArgTypes&&...Args)
{
	static_assert(sizeof...(Args) % 2 == 0, "Must pass an even number of arguments.");
	TArray<FAnalyticsEventAttribute, Allocator> Attrs;
	Attrs.Empty(sizeof...(Args) / 2);
	ImplMakeAnalyticsEventAttributeArray::MakeArray(Attrs, Forward<ArgTypes>(Args)...);
	return Attrs;
}

/** Helper to append to an array of attributes using a single expression. Reserves the necessary space in advance. There must be an even number of arguments, one for each key/value pair. */
template <typename Allocator = FDefaultAllocator, typename...ArgTypes>
FORCEINLINE TArray<FAnalyticsEventAttribute, Allocator>& AppendAnalyticsEventAttributeArray(TArray<FAnalyticsEventAttribute, Allocator>& Attrs, ArgTypes&&...Args)
{
	static_assert(sizeof...(Args) % 2 == 0, "Must pass an even number of arguments.");
	Attrs.Reserve(Attrs.Num() + (sizeof...(Args) / 2));
	ImplMakeAnalyticsEventAttributeArray::MakeArray(Attrs, Forward<ArgTypes>(Args)...);
	return Attrs;
}
