// Copyright Epic Games, Inc. All Rights Reserved.

/*-----------------------------------------------------------------------------
	FPropertyTag.
-----------------------------------------------------------------------------*/

#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"
#include "Serialization/StructuredArchive.h"
#include "UObject/NameTypes.h"
#include "UObject/PropertyTypeName.h"

enum class EOverriddenPropertyOperation : uint8;
class FArchive;
class FProperty;

/**
 * Used by the tag to describe how the property was serialized.
 */
enum class EPropertyTagSerializeType : uint8
{
	/** Tag was loaded from an older version or has not yet been saved. */
	Unknown,
	/** Serialized with tagged property serialization. */
	Property,
	/** Serialized with binary or native serialization. */
	BinaryOrNative,
};

/**
 *  A tag describing a class property, to aid in serialization.
 */
struct FPropertyTag
{
	// Transient.
	UE_DEPRECATED(5.4, "Prop has been deprecated. Use GetProperty()/SetProperty().")
	FProperty* Prop = nullptr;

	// Variables.
private:
	UE::FPropertyTypeName TypeName;

public:
	FName	Type;		// Type of property
	FName	Name;		// Name of property.
	UE_DEPRECATED(5.4, "StructName has been deprecated and replaced by GetType(). Use GetType().IsStruct(StructName) to test, otherwise if Type is StructProperty then StructName is GetType().GetParameterName(0).")
	FName	StructName;	// Struct name if FStructProperty.
	UE_DEPRECATED(5.4, "EnumName has been deprecated and replaced by GetType(). Use GetType().IsEnum(EnumName) to test, otherwise if Type is EnumProperty or ByteProperty then EnumName is GetType().GetParameterName(0).")
	FName	EnumName;	// Enum name if FByteProperty or FEnumProperty
	UE_DEPRECATED(5.4, "InnerName has been deprecated and replaced by GetType(). If Type is ArrayProperty/SetProperty/MapProperty/OptionalProperty then InnerType is GetType().GetParameterName(0).")
	FName	InnerType;	// Inner type if FArrayProperty, FSetProperty, FMapProperty, or OptionalProperty
	UE_DEPRECATED(5.4, "ValueName has been deprecated and replaced by GetType(). If Type is MapProperty then ValueType is GetType().GetParameterName(1).")
	FName	ValueType;	// Value type if UMapPropery
	int32	Size = 0;   // Property size.
	int32	ArrayIndex = INDEX_NONE; // Index if an array; else 0.
	int64	SizeOffset = INDEX_NONE; // location in stream of tag size member
	UE_DEPRECATED(5.4, "StructGuid has been deprecated and replaced by GetType(). If Type is StructProperty then StructGuid is GetType().GetParameterName(1).")
	FGuid	StructGuid;
	FGuid	PropertyGuid;
	uint8	HasPropertyGuid = 0;
	uint8	BoolVal = 0;// a boolean property's value (never need to serialize data for bool properties except here)
	EPropertyTagSerializeType SerializeType = EPropertyTagSerializeType::Unknown;
	EOverriddenPropertyOperation OverrideOperation; // Overridable serialization state reconstruction 
	bool	bExperimentalOverridableLogic = false; // Remember if property had CPF_ExperimentalOverridableLogic when saved

	// Constructors.
	FPropertyTag();
	UE_INTERNAL COREUOBJECT_API FPropertyTag(FProperty* Property, int32 InIndex, uint8* Value);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FPropertyTag(FPropertyTag&&) = default;
	FPropertyTag(const FPropertyTag&) = default;
	FPropertyTag& operator=(FPropertyTag&&) = default;
	FPropertyTag& operator=(const FPropertyTag&) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	inline FProperty* GetProperty() const
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return Prop;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	void SetProperty(FProperty* Property);
	void SetPropertyGuid(const FGuid& InPropertyGuid);

	inline UE::FPropertyTypeName GetType() const { return TypeName; }
	void SetType(UE::FPropertyTypeName TypeName);

	// Serializer.
	friend FArchive& operator<<(FArchive& Ar, FPropertyTag& Tag);
	friend void operator<<(FStructuredArchive::FSlot Slot, FPropertyTag& Tag);

	// Property serializer.
	void SerializeTaggedProperty(FArchive& Ar, FProperty* Property, uint8* Value, const uint8* Defaults) const;
	UE_INTERNAL COREUOBJECT_API void SerializeTaggedProperty(FStructuredArchive::FSlot Slot, FProperty* Property, uint8* Value, const uint8* Defaults) const;

private:
	friend void LoadPropertyTagNoFullType(FStructuredArchive::FSlot Slot, FPropertyTag& Tag);
	friend void SerializePropertyTagAsText(FStructuredArchive::FSlot Slot, FPropertyTag& Tag);
};

struct UE_INTERNAL FPropertyTagScope
{
	FPropertyTagScope(const FPropertyTag* InCurrentPropertyTag)
	: PropertyTagToRestore(CurrentPropertyTag)
	{
		CurrentPropertyTag = InCurrentPropertyTag;
	}

	~FPropertyTagScope()
	{
		CurrentPropertyTag = PropertyTagToRestore;
	}

	static FORCEINLINE const FPropertyTag* GetCurrentPropertyTag()
	{
		return CurrentPropertyTag;
	}
private:
	const FPropertyTag* PropertyTagToRestore;

	static thread_local const FPropertyTag* CurrentPropertyTag;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
