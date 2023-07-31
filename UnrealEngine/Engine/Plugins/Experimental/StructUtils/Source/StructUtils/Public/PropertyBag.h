// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InstancedStruct.h"
#include "StructView.h"
#include "Templates/ValueOrError.h"
#include "UObject/UnrealType.h"
#include <atomic>
#include "PropertyBag.generated.h"

/** Property bag property type, loosely based on BluePrint pin types. */
UENUM()
enum class EPropertyBagPropertyType : uint8
{
	None UMETA(Hidden),
	Bool,
	Byte,
	Int32,
	Int64,
	Float,
	Double,
	Name,
	String,
	Text,
	Enum UMETA(Hidden),
	Struct UMETA(Hidden),
	Object UMETA(Hidden),
	SoftObject UMETA(Hidden),
	Class UMETA(Hidden),
	SoftClass UMETA(Hidden),
};

/** Property bag property container type. */
UENUM()
enum class EPropertyBagContainerType : uint8
{
	None UMETA(Hidden),
	Array,
};

/** Getter and setter result code. */
UENUM()
enum class EPropertyBagResult : uint8
{
	Success,			// Operation succeeded.
	TypeMismatch,		// Tried to access mismatching type (e.g. setting a struct to bool)
	OutOfBounds,		// Tried to access an array property out of bounds.
	PropertyNotFound,	// Could not find property of specified name.
};

USTRUCT()
struct STRUCTUTILS_API FPropertyBagPropertyDescMetaData
{
	GENERATED_BODY()
	
	FPropertyBagPropertyDescMetaData() = default;
	FPropertyBagPropertyDescMetaData(const FName InKey, const FString& InValue)
		: Key(InKey)
		, Value(InValue)
	{
	}
	
	UPROPERTY()
	FName Key;

	UPROPERTY()
	FString Value;

	void Serialize(FArchive& Ar);

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FPropertyBagPropertyDescMetaData& PropertyDescMetaData)
	{
		PropertyDescMetaData.Serialize(Ar);
		return Ar;
	}

	friend FORCEINLINE uint32 GetTypeHash(const FPropertyBagPropertyDescMetaData& PropertyDescMetaData)
	{
		return HashCombine(GetTypeHash(PropertyDescMetaData.Key), GetTypeHash(PropertyDescMetaData.Value));
	}

	friend FORCEINLINE uint32 GetTypeHash(const TArrayView<const FPropertyBagPropertyDescMetaData>& MetaData)
	{
		uint32 Hash = GetTypeHash(MetaData.Num());
		for (const FPropertyBagPropertyDescMetaData& PropertyDescMetaData : MetaData)
		{
			Hash = HashCombine(Hash, GetTypeHash(PropertyDescMetaData));
		}
		return Hash;
	}

	friend FORCEINLINE uint32 GetTypeHash(const TArray<FPropertyBagPropertyDescMetaData>& MetaData)
	{
		return GetTypeHash(TArrayView<const FPropertyBagPropertyDescMetaData>(MetaData.GetData(), MetaData.Num()));
	}
};

/** Describes a property in the property bag. */
USTRUCT()
struct STRUCTUTILS_API FPropertyBagPropertyDesc
{
	GENERATED_BODY()

	FPropertyBagPropertyDesc() = default;
	FPropertyBagPropertyDesc(const FName InName, const FProperty* InSourceProperty);
	FPropertyBagPropertyDesc(const FName InName, const EPropertyBagPropertyType InValueType, UObject* InValueTypeObject = nullptr)
		: ValueTypeObject(InValueTypeObject)
		, Name(InName)
		, ValueType(InValueType)
	{
	}
	FPropertyBagPropertyDesc(const FName InName, const EPropertyBagContainerType InContainerType, const EPropertyBagPropertyType InValueType, UObject* InValueTypeObject = nullptr)
		: ValueTypeObject(InValueTypeObject)
		, Name(InName)
		, ValueType(InValueType)
		, ContainerType(InContainerType)
	{
	}

	/** @return true if the two descriptors have the same type. Object types are compatible if Other can be cast to this type. */
	bool CompatibleType(const FPropertyBagPropertyDesc& Other) const;

	/** @return true if the property type is numeric (bool, int32, int64, float, double, enum) */
	bool IsNumericType() const;
	
	/** @return true if the property type is floating point numeric (float, double) */
	bool IsNumericFloatType() const;

	/** @return true if the property type is object or soft object */
	bool IsObjectType() const;

	/** @return true if the property type is class or soft class */
	bool IsClassType() const;

	/** Pointer to object that defines the Enum, Struct, or Class. */
	UPROPERTY(EditAnywhere, Category="Default")
	TObjectPtr<const UObject> ValueTypeObject = nullptr;

	/** Unique ID for this property. Used as main identifier when copying values over. */
	UPROPERTY(EditAnywhere, Category="Default")
	FGuid ID;

	/** Name for the property. */
	UPROPERTY(EditAnywhere, Category="Default")
	FName Name;

	/** Type of the value described by this property. */
	UPROPERTY(EditAnywhere, Category="Default")
	EPropertyBagPropertyType ValueType = EPropertyBagPropertyType::None;

	/** Type of the container described by this property. */
	UPROPERTY(EditAnywhere, Category="Default")
	EPropertyBagContainerType ContainerType = EPropertyBagContainerType::None;

#if WITH_EDITORONLY_DATA
	/** Editor-only meta data for CachedProperty */
	UPROPERTY(EditAnywhere, Category="Default")
	TArray<FPropertyBagPropertyDescMetaData> MetaData;
#endif

	/** Cached property pointer, set in UPropertyBag::GetOrCreateFromDescs. */
	const FProperty* CachedProperty = nullptr;
};

/**
 * Instanced property bag allows to create and store a bag of properties.
 *
 * When used as editable property, the UI allows properties to be added and removed, and values to be set.
 * The value is stored as a struct, the type of the value is never serialized, instead the composition of the properties
 * is saved with the instance, and the type is recreated on load. The types with same composition of properties share same type (based on hashing).
 *
 * UPROPERTY() meta tags:
 *		- FixedLayout: Property types cannot be altered, but values can be. This is useful if e.g. if the bag layout is set byt code.
 *
 * NOTE: Adding or removing properties to the instance is quite expensive as it will create new UPropertyBag, reallocate memory, and copy all values over. 
 *
 * Example usage, this allows the bag to be configured in the UI:
 *
 *		UPROPERTY(EditDefaultsOnly, Category = Common)
 *		FInstancedPropertyBag Bag;
 *
 * Changing the layout from code:
 *
 *		static const FName TemperatureName(TEXT("Temperature"));
 *		static const FName IsHotName(TEXT("bIsHot"));
 *
 *		FInstancedPropertyBag Bag;
 *
 *		// Add properties to the bag, and set their values.
 *		// Adding or removing properties is not cheap, so better do it in batches.
 *		Bag.AddProperties({
 *			{ TemperatureName, EPropertyBagPropertyType::Float },
 *			{ CountName, EPropertyBagPropertyType::Int32 }
 *		});
 *
 *		// Amend the bag with a new property.
 *		Bag.AddProperty(IsHotName, EPropertyBagPropertyType::Bool);
 *		Bag.SetValueBool(IsHotName, true);
 *
 *		// Get value and use the result
 *		if (auto Temperature = Bag.GetValueFloat(TemperatureName); Temperature.IsValid())
 *		{
 *			float Val = Temperature.GetValue();
 *		}
 */

class UPropertyBag;
class FPropertyBagArrayRef;

USTRUCT()
struct STRUCTUTILS_API FInstancedPropertyBag
{
	GENERATED_BODY()

	FInstancedPropertyBag() = default;
	FInstancedPropertyBag(const FInstancedPropertyBag& Other) = default;
	FInstancedPropertyBag(FInstancedPropertyBag&& Other) = default;

	FInstancedPropertyBag& operator=(const FInstancedPropertyBag& InOther) = default;
	FInstancedPropertyBag& operator=(FInstancedPropertyBag&& InOther) = default;

	/** @return true if the instance contains data. */
	bool IsValid() const
	{
		return Value.IsValid();
	}
	
	/** Resets the instance to empty. */
	void Reset()
	{
		Value.Reset();
	}

	/** Initializes the instance from an bag struct. */
	void InitializeFromBagStruct(const UPropertyBag* NewBagStruct);
	
	/**
	 * Copies matching property values from another bag of potentially mismatching layout.
	 * The properties are matched between the bags based on the property ID.
	 * @param Other Reference to the bag to copy the values from
	 */
	void CopyMatchingValuesByID(const FInstancedPropertyBag& NewDescs) const;

	/** Returns number of the Properties in this Property Bag */
	int32 GetNumPropertiesInBag() const;

	/**
	 * Adds properties to the bag. If property of same name already exists, it will be replaced with the new type.
	 * Numeric property values will be converted if possible, when a property's type changes.
	 * @param Descs Descriptors of new properties to add.
	 */
	void AddProperties(const TConstArrayView<FPropertyBagPropertyDesc> Descs);
	
	/**
	 * Adds a new property to the bag. If property of same name already exists, it will be replaced with the new type.
	 * Numeric property values will be converted if possible, when a property's type changes.
	 * @param InName Name of the new property
	 * @param InValueType Type of the new property
	 * @param InValueTypeObject Type object (for struct, class, enum) of the new property
	 */
	void AddProperty(const FName InName, const EPropertyBagPropertyType InValueType, UObject* InValueTypeObject = nullptr);

	/**
	 * Adds a new container property to the bag. If property of same name already exists, it will be replaced with the new type.
	 * @param InName Name of the new property
	 * @param InContainerType Type of the new container
	 * @param InValueType Type of the new property
	 * @param InValueTypeObject Type object (for struct, class, enum) of the new property
	 */
	void AddContainerProperty(const FName InName, const EPropertyBagContainerType InContainerType, const EPropertyBagPropertyType InValueType, UObject* InValueTypeObject = nullptr);

	/**
	 * Adds a new property to the bag. Property type duplicated from source property to. If property of same name already exists, it will be replaced with the new type.
	 * @param Descs Descriptors of new properties to add.
	 */
	void AddProperty(const FName InName, const FProperty* InSourceProperty);

	/**
	 * Removes properties from the bag by name if they exists.
	 */
	void RemovePropertiesByName(const TConstArrayView<FName> PropertiesToRemove);
	
	/**
	 * Removes a property from the bag by name if it exists.
	 */
	void RemovePropertyByName(const FName PropertyToRemove);
	
	/**
	 * Changes the type of this bag and migrates existing values.
	 * The properties are matched between the bags based on the property ID.
	 * @param NewBagStruct Pointer to the new type.
	 */
	void MigrateToNewBagStruct(const UPropertyBag* NewBagStruct);

	/**
	 * Changes the type of this bag to the specified other bag, copies base values from the other bag, and migrates existing values over.
	 * The properties are matched between the bags based on the property ID.
	 * @param NewBagInstance Reference to the new type.
	 */
	void MigrateToNewBagInstance(const FInstancedPropertyBag& NewBagInstance);

	/** @return pointer to the property bag struct. */ 
	const UPropertyBag* GetPropertyBagStruct() const;
	
	/** Returns property descriptor by specified name. */
	const FPropertyBagPropertyDesc* FindPropertyDescByID(const FGuid ID) const;
	
	/** Returns property descriptor by specified ID. */
	const FPropertyBagPropertyDesc* FindPropertyDescByName(const FName Name) const;

	/** @return const view to the struct that holds the values. NOTE: The returned value/view cannot be serialized, use this to access the struct only temporarily. */
	FConstStructView GetValue() const { return Value; };

	/** @return const view to the struct that holds the values. NOTE: The returned value/view cannot be serialized, use this to access the struct only temporarily. */
	FStructView GetMutableValue() const { return Value; };
	
	/**
	 * Getters
	 * Numeric types (bool, int32, int64, float, double) support type conversion.
	 */

	TValueOrError<bool, EPropertyBagResult> GetValueBool(const FName Name) const;
	TValueOrError<uint8, EPropertyBagResult> GetValueByte(const FName Name) const;
	TValueOrError<int32, EPropertyBagResult> GetValueInt32(const FName Name) const;
	TValueOrError<int64, EPropertyBagResult> GetValueInt64(const FName Name) const;
	TValueOrError<float, EPropertyBagResult> GetValueFloat(const FName Name) const;
	TValueOrError<double, EPropertyBagResult> GetValueDouble(const FName Name) const;
	TValueOrError<FName, EPropertyBagResult> GetValueName(const FName Name) const;
	TValueOrError<FString, EPropertyBagResult> GetValueString(const FName Name) const;
	TValueOrError<FText, EPropertyBagResult> GetValueText(const FName Name) const;
	TValueOrError<uint8, EPropertyBagResult> GetValueEnum(const FName Name, const UEnum* RequestedEnum) const;
	TValueOrError<FStructView, EPropertyBagResult> GetValueStruct(const FName Name, const UScriptStruct* RequestedStruct = nullptr) const;
	TValueOrError<UObject*, EPropertyBagResult> GetValueObject(const FName Name, const UClass* RequestedClass = nullptr) const;
	TValueOrError<UClass*, EPropertyBagResult> GetValueClass(const FName Name) const;

	/** @return enum value of specified type. */
	template <typename T>
	TValueOrError<T, EPropertyBagResult> GetValueEnum(const FName Name) const
	{
		static_assert(TIsEnum<T>::Value, "Should only call this with enum types");
		
		TValueOrError<uint8, EPropertyBagResult> Result = GetValueEnum(Name, StaticEnum<T>());
		if (!Result.IsValid())
		{
			return MakeError(Result.GetError());
		}
		return MakeValue((T)Result.GetValue());
	}
	
	/** @return struct reference of specified type. */
	template <typename T>
	TValueOrError<T*, EPropertyBagResult> GetValueStruct(const FName Name) const
	{
		TValueOrError<FStructView, EPropertyBagResult> Result = GetValueStruct(Name, TBaseStructure<T>::Get());
		if (!Result.IsValid())
		{
			return MakeError(Result.GetError());
		}
		if (T* ValuePtr = Result.GetValue().GetMutablePtr<T>())
		{
			return MakeValue(ValuePtr);
		}
		return MakeError(EPropertyBagResult::TypeMismatch);
	}
	
	/** @return object pointer value of specified type. */
	template <typename T>
	TValueOrError<T*, EPropertyBagResult> GetValueObject(const FName Name) const
	{
		static_assert(TIsDerivedFrom<T, UObject>::Value, "Should only call this with object types");
		
		TValueOrError<UObject*, EPropertyBagResult> Result = GetValueObject(Name, T::StaticClass());
		if (!Result.IsValid())
		{
			return MakeError(Result.GetError());
		}
		if (Result.GetValue() == nullptr)
		{
			return MakeValue(nullptr);
		}
		if (T* Object = Cast<T>(Result.GetValue()))
		{
			return MakeValue(Object);
		}
		return MakeError(EPropertyBagResult::TypeMismatch);
	}

	/**
	 * Value Setters. A property must exists in that bag before it can be set.  
	 * Numeric types (bool, int32, int64, float, double) support type conversion.
	 */
	EPropertyBagResult SetValueBool(const FName Name, const bool bInValue) const;
	EPropertyBagResult SetValueByte(const FName Name, const uint8 InValue) const;
	EPropertyBagResult SetValueInt32(const FName Name, const int32 InValue) const;
	EPropertyBagResult SetValueInt64(const FName Name, const int64 InValue) const;
	EPropertyBagResult SetValueFloat(const FName Name, const float InValue) const;
	EPropertyBagResult SetValueDouble(const FName Name, const double InValue) const;
	EPropertyBagResult SetValueName(const FName Name, const FName InValue) const;
	EPropertyBagResult SetValueString(const FName Name, const FString& InValue) const;
	EPropertyBagResult SetValueText(const FName Name, const FText& InValue) const;
	EPropertyBagResult SetValueEnum(const FName Name, const uint8 InValue, const UEnum* Enum) const;
	EPropertyBagResult SetValueStruct(const FName Name, FConstStructView InValue) const;
	EPropertyBagResult SetValueObject(const FName Name, UObject* InValue) const;
	EPropertyBagResult SetValueClass(const FName Name, UClass* InValue) const;

	/** Sets enum value specified type. */
	template <typename T>
	EPropertyBagResult SetValueEnum(const FName Name, const T InValue) const
	{
		static_assert(TIsEnum<T>::Value, "Should only call this with enum types");
		return SetValueEnum(Name, (uint8)InValue, StaticEnum<T>());
	}

	/** Sets struct value specified type. */
	template <typename T>
	EPropertyBagResult SetValueStruct(const FName Name, const T& InValue) const
	{
		return SetValueStruct(Name, FConstStructView::Make(InValue));
	}

	/** Sets object pointer value specified type. */
	template <typename T>
	EPropertyBagResult SetValueObject(const FName Name, T* InValue) const
	{
		static_assert(TIsDerivedFrom<T, UObject>::Value, "Should only call this with object types");
		return SetValueObject(Name, (UObject*)InValue);
	}

	/**
	 * Sets property value from given source property and source container address
	 * A property must exists in that bag before it can be set. 
	 */
	EPropertyBagResult SetValue(const FName Name, const FProperty* InSourceProperty, const void* InSourceContainerAddress) const;

	/**
	 * Returns helper class to modify and access an array property.
	 * Note: Note: The array reference is not valid after the layout of the referenced property bag has changed!
	 * @returns helper class to modify and access arrays
	*/
	TValueOrError<FPropertyBagArrayRef, EPropertyBagResult> GetArrayRef(const FName Name) const;

	bool Serialize(FArchive& Ar);

protected:
	void* GetValueAddress(const FPropertyBagPropertyDesc* Desc) const;
	
	UPROPERTY(EditAnywhere, Category="")
	FInstancedStruct Value;
};

template<> struct TStructOpsTypeTraits<FInstancedPropertyBag> : public TStructOpsTypeTraitsBase2<FInstancedPropertyBag>
{
	enum
	{
		WithSerializer = true
	};
};


/**
 * A reference to an array in FInstancedPropertyBag
 * Allows to modify the array via the FScriptArrayHelper API, and contains helper methods to get and set properties.
 *
 *		FInstancedPropertyBag Bag;
 *		Bag.AddProperties({
 *			{ ArrayName, EPropertyBagContainerType::Array, EPropertyBagPropertyType::Float }
 *		});
 *
 *		if (auto FloatArrayRes = Bag.GetArrayRef(ArrayName); FloatArrayRes.IsValid())
 *		{
 *			FPropertyBagArrayRef& FloatArray = FloatArrayRes.GetValue();
 *			const int32 NewIndex = FloatArray.AddValue();
 *			FloatArray.SetValueFloat(NewIndex, 123.0f);
 *		}
 * 
 * Note: The array reference is not valid after the layout of the referenced property bag has changed! 
 */
class STRUCTUTILS_API FPropertyBagArrayRef : public FScriptArrayHelper
{
	FPropertyBagPropertyDesc ValueDesc;

	const void* GetAddress(const int32 Index) const
	{
		if (IsValidIndex(Index) == false)
		{
			return nullptr;
		}
		// Ugly, but FScriptArrayHelper does not give us other option.
		FPropertyBagArrayRef* NonConstThis = const_cast<FPropertyBagArrayRef*>(this);
		return (void*)NonConstThis->GetRawPtr(Index);
	}

	void* GetMutableAddress(const int32 Index) const
	{
		if (IsValidIndex(Index) == false)
		{
			return nullptr;
		}
		// Ugly, but FScriptArrayHelper does not give us other option.
		FPropertyBagArrayRef* NonConstThis = const_cast<FPropertyBagArrayRef*>(this);
		return (void*)NonConstThis->GetRawPtr(Index);
	}

public:	
	FORCEINLINE FPropertyBagArrayRef(const FPropertyBagPropertyDesc& InDesc, const void* InArray)
		: FScriptArrayHelper(CastField<FArrayProperty>(InDesc.CachedProperty), InArray)
	{
		const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InDesc.CachedProperty);
		check(ArrayProperty);
		check(ArrayProperty->Inner);
		// Create dummy desc for the inner property. 
		ValueDesc.ValueType = InDesc.ValueType;
		ValueDesc.ValueTypeObject = InDesc.ValueTypeObject;
		ValueDesc.CachedProperty = ArrayProperty->Inner;
		ValueDesc.ContainerType = EPropertyBagContainerType::None;
	}

	/**
	 * Getters
	 * Numeric types (bool, int32, int64, float, double) support type conversion.
	 */
	
	TValueOrError<bool, EPropertyBagResult> GetValueBool(const int32 Index) const;
	TValueOrError<uint8, EPropertyBagResult> GetValueByte(const int32 Index) const;
	TValueOrError<int32, EPropertyBagResult> GetValueInt32(const int32 Index) const;
	TValueOrError<int64, EPropertyBagResult> GetValueInt64(const int32 Index) const;
	TValueOrError<float, EPropertyBagResult> GetValueFloat(const int32 Index) const;
	TValueOrError<double, EPropertyBagResult> GetValueDouble(const int32 Index) const;
	TValueOrError<FName, EPropertyBagResult> GetValueName(const int32 Index) const;
	TValueOrError<FString, EPropertyBagResult> GetValueString(const int32 Index) const;
	TValueOrError<FText, EPropertyBagResult> GetValueText(const int32 Index) const;
	TValueOrError<uint8, EPropertyBagResult> GetValueEnum(const int32 Index, const UEnum* RequestedEnum) const;
	TValueOrError<FStructView, EPropertyBagResult> GetValueStruct(const int32 Index, const UScriptStruct* RequestedStruct = nullptr) const;
	TValueOrError<UObject*, EPropertyBagResult> GetValueObject(const int32 Index, const UClass* RequestedClass = nullptr) const;
	TValueOrError<UClass*, EPropertyBagResult> GetValueClass(const int32 Index) const;

	/** @return enum value of specified type. */
	template <typename T>
	TValueOrError<T, EPropertyBagResult> GetValueEnum(const int32 Index) const
	{
		static_assert(TIsEnum<T>::Value, "Should only call this with enum types");
		
		TValueOrError<uint8, EPropertyBagResult> Result = GetValueEnum(Index, StaticEnum<T>());
		if (!Result.IsValid())
		{
			return MakeError(Result.GetError());
		}
		return MakeValue((T)Result.GetValue());
	}
	
	/** @return struct reference of specified type. */
	template <typename T>
	TValueOrError<T*, EPropertyBagResult> GetValueStruct(const int32 Index) const
	{
		TValueOrError<FStructView, EPropertyBagResult> Result = GetValueStruct(Index, TBaseStructure<T>::Get());
		if (!Result.IsValid())
		{
			return MakeError(Result.GetError());
		}
		if (T* ValuePtr = Result.GetValue().GetMutablePtr<T>())
		{
			return MakeValue(ValuePtr);
		}
		return MakeError(EPropertyBagResult::TypeMismatch);
	}
	
	/** @return object pointer value of specified type. */
	template <typename T>
	TValueOrError<T*, EPropertyBagResult> GetValueObject(const int32 Index) const
	{
		static_assert(TIsDerivedFrom<T, UObject>::Value, "Should only call this with object types");
		
		TValueOrError<UObject*, EPropertyBagResult> Result = GetValueObject(Index, T::StaticClass());
		if (!Result.IsValid())
		{
			return MakeError(Result.GetError());
		}
		if (Result.GetValue() == nullptr)
		{
			return MakeValue(nullptr);
		}
		if (T* Object = Cast<T>(Result.GetValue()))
		{
			return MakeValue(Object);
		}
		return MakeError(EPropertyBagResult::TypeMismatch);
	}

	/**
	 * Value Setters. A property must exists in that bag before it can be set.  
	 * Numeric types (bool, int32, int64, float, double) support type conversion.
	 */
	EPropertyBagResult SetValueBool(const int32 Index, const bool bInValue) const;
	EPropertyBagResult SetValueByte(const int32 Index, const uint8 InValue) const;
	EPropertyBagResult SetValueInt32(const int32 Index, const int32 InValue) const;
	EPropertyBagResult SetValueInt64(const int32 Index, const int64 InValue) const;
	EPropertyBagResult SetValueFloat(const int32 Index, const float InValue) const;
	EPropertyBagResult SetValueDouble(const int32 Index, const double InValue) const;
	EPropertyBagResult SetValueName(const int32 Index, const FName InValue) const;
	EPropertyBagResult SetValueString(const int32 Index, const FString& InValue) const;
	EPropertyBagResult SetValueText(const int32 Index, const FText& InValue) const;
	EPropertyBagResult SetValueEnum(const int32 Index, const uint8 InValue, const UEnum* Enum) const;
	EPropertyBagResult SetValueStruct(const int32 Index, FConstStructView InValue) const;
	EPropertyBagResult SetValueObject(const int32 Index, UObject* InValue) const;
	EPropertyBagResult SetValueClass(const int32 Index, UClass* InValue) const;

	/** Sets enum value specified type. */
	template <typename T>
	EPropertyBagResult SetValueEnum(const int32 Index, const T InValue) const
	{
		static_assert(TIsEnum<T>::Value, "Should only call this with enum types");
		return SetValueEnum(Index, (uint8)InValue, StaticEnum<T>());
	}

	/** Sets struct value specified type. */
	template <typename T>
	EPropertyBagResult SetValueStruct(const int32 Index, const T& InValue) const
	{
		return SetValueStruct(Index, FConstStructView::Make(InValue));
	}

	/** Sets object pointer value specified type. */
	template <typename T>
	EPropertyBagResult SetValueObject(const int32 Index, T* InValue) const
	{
		static_assert(TIsDerivedFrom<T, UObject>::Value, "Should only call this with object types");
		return SetValueObject(Index, (UObject*)InValue);
	}
};


/**
 * Dummy types used to mark up missing types when creating property bags. These are used in the UI to display error message.
 */
UENUM()
enum class EPropertyBagMissingEnum : uint8
{
	Missing,
};

USTRUCT()
struct STRUCTUTILS_API FPropertyBagMissingStruct
{
	GENERATED_BODY()
};

UCLASS()
class STRUCTUTILS_API UPropertyBagMissingObject : public UObject
{
	GENERATED_BODY()
};


/**
 * A script struct that is used to store the value of the property bag instance.
 * References to UPropertyBag cannot be serialized, instead the array of the properties
 * is serialized and new class is create on load based on the composition of the properties.
 *
 * Note: Should not be used directly.
 */
UCLASS(Transient)
class STRUCTUTILS_API UPropertyBag : public UScriptStruct
{
public:
	GENERATED_BODY()

	/**
	 * Creates new UPropertyBag struct based on the properties passed in.
	 * If there are multiple properties that have the same name, only the first one is added.
	 */
	static const UPropertyBag* GetOrCreateFromDescs(const TConstArrayView<FPropertyBagPropertyDesc> InPropertyDescs);

	/** Returns property descriptions that specify this struct. */
	TConstArrayView<FPropertyBagPropertyDesc> GetPropertyDescs() const { return PropertyDescs; }

	/** @return property description based on ID. */
	const FPropertyBagPropertyDesc* FindPropertyDescByID(const FGuid ID) const;

	/** @return property description based on name. */
	const FPropertyBagPropertyDesc* FindPropertyDescByName(const FName Name) const;

protected:

	virtual void InitializeStruct(void* Dest, int32 ArrayDim = 1) const override;
	virtual void DestroyStruct(void* Dest, int32 ArrayDim = 1) const override;
	virtual void FinishDestroy();
	
	UPROPERTY()
	TArray<FPropertyBagPropertyDesc> PropertyDescs;

	std::atomic<int32> RefCount = 0;
	
	friend struct FInstancedPropertyBag;
};
