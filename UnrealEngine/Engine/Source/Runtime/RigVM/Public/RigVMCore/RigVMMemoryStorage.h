// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "EdGraph/EdGraphNode.h"
#include "Misc/AssertionMacros.h"
#include "RigVMMemoryCommon.h"
#include "RigVMPropertyPath.h"
#include "RigVMStatistics.h"
#include "RigVMTraits.h"
#include "Templates/Casts.h"
#include "Templates/EnableIf.h"
#include "Templates/IsEnum.h"
#include "Templates/Models.h"
#include "UObject/Class.h"
#include "UObject/EnumProperty.h"
#include "UObject/Field.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealNames.h"
#include "UObject/UnrealType.h"

#include "RigVMMemoryStorage.generated.h"

class FArchive;

/**
 * The FRigVMMemoryHandle is used to access the memory used within a URigMemoryStorage.
 * The Memory Handle caches the pointer of the head property, and can rely on a
 * RigVMPropertyPath to traverse towards a tail property.
 * For example it can cache the pointer of a TArray<FTransform> property's memory
 * and use a property path of '[2].Translation.X' to retrieve the element 2's translation
 * X float / double.
 * Additionally the FRigVMMemoryHandle can be used to access 'sliced' memory. Sliced
 * memory in this case simply means another level of a nested TArray. So a 'sliced'
 * FTransform is stored as a TArray<FTransform>, which the handle can traverse into and
 * return the memory of a specific FTransform within that array.
 */
struct FRigVMMemoryHandle
{
public:

	// Default constructor
	FORCEINLINE_DEBUGGABLE FRigVMMemoryHandle()
		: Ptr(nullptr)
		, Property(nullptr) 
		, PropertyPath(nullptr)
	{}

	// Constructor from complete data
	FORCEINLINE_DEBUGGABLE FRigVMMemoryHandle(uint8* InPtr, const FProperty* InProperty,  const FRigVMPropertyPath* InPropertyPath)
		: Ptr(InPtr)
		, Property(InProperty)
		, PropertyPath(InPropertyPath)
	{
	}

	/**
	 * Returns the cached pointer stored within the handle.
	 * @param bFollowPropertyPath If set to true the memory handle will traverse the memory using the property path
	 * @param InSliceIndex If this is != INDEX_NONE the memory handle will return the slice of the memory requested
	 * @return The traversed memory cached by this handle.
	 */
	FORCEINLINE uint8* GetData(bool bFollowPropertyPath = false, int32 InSliceIndex = INDEX_NONE)
	{
		return GetData_Internal(bFollowPropertyPath, InSliceIndex);
	}

	/**
	 * Returns the cached pointer stored within the handle.
	 * @param bFollowPropertyPath If set to true the memory handle will traverse the memory using the property path
	 * @param InSliceIndex If this is != INDEX_NONE the memory handle will return the slice of the memory requested
	 * @return The traversed memory cached by this handle (const)
	 */
	FORCEINLINE const uint8* GetData(bool bFollowPropertyPath = false, int32 InSliceIndex = INDEX_NONE) const
	{
		return GetData_Internal(bFollowPropertyPath, InSliceIndex);
	}

	// Returns the head property of this handle
	FORCEINLINE const FProperty* GetProperty() const
	{
		return Property;
	}

	// Returns the [optional] property path used within this handle
	FORCEINLINE const FRigVMPropertyPath* GetPropertyPath() const
	{
		return PropertyPath;
	}

	// Returns the [optional] property path used within this handle (ref)
	FORCEINLINE const FRigVMPropertyPath& GetPropertyPathRef() const
	{
		if(PropertyPath)
		{
			return *PropertyPath;
		}
		return FRigVMPropertyPath::Empty;
	}

	// Returns the resolved property the data is pointing to
	FORCEINLINE const FProperty* GetResolvedProperty(bool bIsHiddenArgument = false) const
	{
		if(bIsHiddenArgument)
		{
			check(PropertyPath == nullptr);
			return GetArrayElementPropertyChecked(Property);
		}
		else if(PropertyPath)
		{
			return PropertyPath->GetTailProperty();
		}
		return Property;
	}

	// Returns true if this memory handle maps to a given type of property
	template<typename PropertyType>
	FORCEINLINE bool IsPropertyType(bool bIsHiddenArgument = false) const
	{
		const FProperty* ResolvedProperty = GetResolvedProperty(bIsHiddenArgument);
		return ResolvedProperty->IsA<PropertyType>();
	}

	// Returns true if this memory handle maps to a given array type of property
	template<typename PropertyType>
	FORCEINLINE bool IsPropertyArrayType(bool bIsHiddenArgument = false) const
	{
		const FProperty* ResolvedProperty = GetResolvedProperty(bIsHiddenArgument);
		if(const FProperty* ElementProperty = GetArrayElementProperty(ResolvedProperty))
		{
			return ElementProperty->IsA<PropertyType>();
		}
		return false;
	}

	// returns true if the handle is of the given type
	template <
		typename T,
		typename TEnableIf<TIsTArray<T>::Value>::Type* = nullptr
	>
	FORCEINLINE bool IsType(bool bIsHiddenArgument = false) const
	{
		typedef typename T::ElementType ElementType;
		return IsTypeArray<ElementType>(bIsHiddenArgument);
	}

	// returns true if the handle is of the given type
	template <
		typename T,
		typename TEnableIf<TRigVMIsBool<T>::Value, T>::Type* = nullptr
	>
	FORCEINLINE bool IsType(bool bIsHiddenArgument = false) const
	{
		return IsBool(bIsHiddenArgument);
	}

	// returns true if the handle is of the given type
	template <
		typename T,
		typename TEnableIf<TRigVMIsBool<T>::Value, T>::Type* = nullptr
	>
	FORCEINLINE bool IsTypeArray(bool bIsHiddenArgument = false) const
	{
		return IsBoolArray(bIsHiddenArgument);
	}

	// returns true if the handle is of the given type
	template <
		typename T,
		typename TEnableIf<TRigVMIsFloat<T>::Value, T>::Type* = nullptr
	>
	FORCEINLINE bool IsType(bool bIsHiddenArgument = false) const
	{
		return IsFloat(bIsHiddenArgument);
	}

	// returns true if the handle is of the given type
	template <
		typename T,
		typename TEnableIf<TRigVMIsFloat<T>::Value, T>::Type* = nullptr
	>
	FORCEINLINE bool IsTypeArray(bool bIsHiddenArgument = false) const
	{
		return IsFloatArray(bIsHiddenArgument);
	}

	// returns true if the handle is of the given type
	template <
		typename T,
		typename TEnableIf<TRigVMIsDouble<T>::Value, T>::Type* = nullptr
	>
	FORCEINLINE bool IsType(bool bIsHiddenArgument = false) const
	{
		return IsDouble(bIsHiddenArgument);
	}

	// returns true if the handle is of the given type
	template <
		typename T,
		typename TEnableIf<TRigVMIsDouble<T>::Value, T>::Type* = nullptr
	>
	FORCEINLINE bool IsTypeArray(bool bIsHiddenArgument = false) const
	{
		return IsDoubleArray(bIsHiddenArgument);
	}

	// returns true if the handle is of the given type
	template <
		typename T,
		typename TEnableIf<TRigVMIsInt32<T>::Value, T>::Type* = nullptr
	>
	FORCEINLINE bool IsType(bool bIsHiddenArgument = false) const
	{
		return IsInt32(bIsHiddenArgument);
	}

	// returns true if the handle is of the given type
	template <
		typename T,
		typename TEnableIf<TRigVMIsInt32<T>::Value, T>::Type* = nullptr
	>
	FORCEINLINE bool IsTypeArray(bool bIsHiddenArgument = false) const
	{
		return IsInt32Array(bIsHiddenArgument);
	}
	// returns true if the handle is of the given type
	template <
		typename T,
		typename TEnableIf<TRigVMIsName<T>::Value, T>::Type* = nullptr
	>
	FORCEINLINE bool IsType(bool bIsHiddenArgument = false) const
	{
		return IsName(bIsHiddenArgument);
	}

	// returns true if the handle is of the given type
	template <
		typename T,
		typename TEnableIf<TRigVMIsName<T>::Value, T>::Type* = nullptr
	>
	FORCEINLINE bool IsTypeArray(bool bIsHiddenArgument = false) const
	{
		return IsNameArray(bIsHiddenArgument);
	}

	// returns true if the handle is of the given type
	template <
		typename T,
		typename TEnableIf<TRigVMIsString<T>::Value, T>::Type* = nullptr
	>
	FORCEINLINE bool IsType(bool bIsHiddenArgument = false) const
	{
		return IsString(bIsHiddenArgument);
	}

	// returns true if the handle is of the given type
	template <
		typename T,
		typename TEnableIf<TRigVMIsString<T>::Value, T>::Type* = nullptr
	>
	FORCEINLINE bool IsTypeArray(bool bIsHiddenArgument = false) const
	{
		return IsStringArray(bIsHiddenArgument);
	}

	// returns true if the handle is of the given type
	template <
		typename T,
		typename TEnableIf<TRigVMIsBaseStructure<T>::Value, T>::Type* = nullptr
	>
	FORCEINLINE bool IsType(bool bIsHiddenArgument = false) const
	{
		return IsStruct<T>(bIsHiddenArgument);
	}

	// returns true if the handle is of the given type
	template <
		typename T,
		typename TEnableIf<TRigVMIsBaseStructure<T>::Value, T>::Type* = nullptr
	>
	FORCEINLINE bool IsTypeArray(bool bIsHiddenArgument = false) const
	{
		return IsStructArray<T>(bIsHiddenArgument);
	}

	// returns true if the handle is of the given type
	template <
		typename T,
		typename TEnableIf<TModels<CRigVMUStruct, T>::Value>::Type * = nullptr
	>
	FORCEINLINE bool IsType(bool bIsHiddenArgument = false) const
	{
		return IsStruct<T>(bIsHiddenArgument);
	}

	// returns true if the handle is of the given type
	template <
		typename T,
		typename TEnableIf<TModels<CRigVMUStruct, T>::Value>::Type * = nullptr
	>
	FORCEINLINE bool IsTypeArray(bool bIsHiddenArgument = false) const
	{
		return IsStructArray<T>(bIsHiddenArgument);
	}

	// returns true if the handle is of the given type
	template <
		typename T,
		typename TEnableIf<TIsEnum<T>::Value>::Type* = nullptr
	>
	FORCEINLINE bool IsType(bool bIsHiddenArgument = false) const
	{
		return IsEnum<T>(bIsHiddenArgument);
	}

	// returns true if the handle is of the given type
	template <
		typename T,
		typename TEnableIf<TIsEnum<T>::Value>::Type* = nullptr
	>
	FORCEINLINE bool IsTypeArray(bool bIsHiddenArgument = false) const
	{
		return IsEnumArray<T>(bIsHiddenArgument);
	}

	// returns true if the handle is of the given type
	template <
		typename T,
		typename TEnableIf<TModels<CRigVMUClass, T>::Value>::Type * = nullptr
	>
	FORCEINLINE bool IsType(bool bIsHiddenArgument = false) const
	{
		return IsObject<T>(bIsHiddenArgument);
	}

	// returns true if the handle is of the given type
	template <
		typename T,
		typename TEnableIf<TModels<CRigVMUClass, T>::Value>::Type * = nullptr
	>
	FORCEINLINE bool IsTypeArray(bool bIsHiddenArgument = false) const
	{
		return IsObjectArray<T>(bIsHiddenArgument);
	}
	
	// Returns true if this memory handle maps to a bool property
	FORCEINLINE bool IsBool(bool bIsHiddenArgument = false) const
	{
		return IsPropertyType<FBoolProperty>(bIsHiddenArgument);
	}

	// Returns true if this memory handle maps to a bool array property
	FORCEINLINE bool IsBoolArray(bool bIsHiddenArgument = false) const
	{
		return IsPropertyArrayType<FBoolProperty>(bIsHiddenArgument);
	}
	
	// Returns true if this memory handle maps to a float property
	FORCEINLINE bool IsFloat(bool bIsHiddenArgument = false) const
	{
		return IsPropertyType<FFloatProperty>(bIsHiddenArgument);
	}

	// Returns true if this memory handle maps to a float array property
	FORCEINLINE bool IsFloatArray(bool bIsHiddenArgument = false) const
	{
		return IsPropertyArrayType<FFloatProperty>(bIsHiddenArgument);
	}

	// Returns true if this memory handle maps to a double property
	FORCEINLINE bool IsDouble(bool bIsHiddenArgument = false) const
	{
		return IsPropertyType<FDoubleProperty>(bIsHiddenArgument);
	}

	// Returns true if this memory handle maps to a double array property
	FORCEINLINE bool IsDoubleArray(bool bIsHiddenArgument = false) const
	{
		return IsPropertyArrayType<FDoubleProperty>(bIsHiddenArgument);
	}

	// Returns true if this memory handle maps to a int32 property
	FORCEINLINE bool IsInt32(bool bIsHiddenArgument = false) const
	{
		return IsPropertyType<FIntProperty>(bIsHiddenArgument);
	}

	// Returns true if this memory handle maps to a int32 array property
	FORCEINLINE bool IsInt32Array(bool bIsHiddenArgument = false) const
	{
		return IsPropertyArrayType<FIntProperty>(bIsHiddenArgument);
	}

	// Returns true if this memory handle maps to an FName property
    FORCEINLINE bool IsName(bool bIsHiddenArgument = false) const
    {
    	return IsPropertyType<FNameProperty>(bIsHiddenArgument);
    }

	// Returns true if this memory handle maps to an FName array property
    FORCEINLINE bool IsNameArray(bool bIsHiddenArgument = false) const
    {
    	return IsPropertyArrayType<FNameProperty>(bIsHiddenArgument);
    }

	// Returns true if this memory handle maps to an FString property
	FORCEINLINE bool IsString(bool bIsHiddenArgument = false) const
	{
		return IsPropertyType<FStrProperty>(bIsHiddenArgument);
	}

	// Returns true if this memory handle maps to an FString array property
	FORCEINLINE bool IsStringArray(bool bIsHiddenArgument = false) const
	{
		return IsPropertyArrayType<FStrProperty>(bIsHiddenArgument);
	}

	// Returns true if this memory handle maps to an enum property
	FORCEINLINE bool IsEnum(const UEnum* InEnum, bool bIsHiddenArgument = false) const
	{
		const FProperty* ResolvedProperty = GetResolvedProperty();
		return IsEnum(ResolvedProperty, InEnum, bIsHiddenArgument);
	}

	// Returns true if this memory handle maps to an enum array property
	FORCEINLINE bool IsEnumArray(const UEnum* InEnum, bool bIsHiddenArgument = false) const
	{
		const FProperty* ResolvedProperty = GetResolvedProperty();
		if(bIsHiddenArgument)
		{
			ResolvedProperty = GetArrayElementPropertyChecked(ResolvedProperty);
		}
		if(const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(ResolvedProperty))
		{
			return IsEnum(ArrayProperty->Inner, InEnum, false);
		}
		return false;
	}

	// Returns true if this memory handle maps to an enum property
	template<typename T>
	FORCEINLINE bool IsEnum(bool bIsHiddenArgument = false) const
	{
		return IsEnum(StaticEnum<T>(), bIsHiddenArgument);
	}

	// Returns true if this memory handle maps to an enum array property
	template<typename T>
	FORCEINLINE bool IsEnumArray(bool bIsHiddenArgument = false) const
	{
		return IsEnumArray(StaticEnum<T>(), bIsHiddenArgument);
	}

	// Returns true if this memory handle maps to a struct property
	FORCEINLINE bool IsStruct(const UScriptStruct* InStruct, bool bIsHiddenArgument = false) const
	{
		const FProperty* ResolvedProperty = GetResolvedProperty();
		return IsStruct(ResolvedProperty, InStruct, bIsHiddenArgument);
	}

	// Returns true if this memory handle maps to a struct array property
	FORCEINLINE bool IsStructArray(const UScriptStruct* InStruct, bool bIsHiddenArgument = false) const
	{
		const FProperty* ResolvedProperty = GetResolvedProperty();
		if(bIsHiddenArgument)
		{
			ResolvedProperty = GetArrayElementPropertyChecked(ResolvedProperty);
		}
		if(const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(ResolvedProperty))
		{
			return IsStruct(ArrayProperty->Inner, InStruct, false);
		}
		return false;
	}

	// Returns true if this memory handle maps to a struct property
	template <
		typename T,
		typename TEnableIf<TRigVMIsBaseStructure<T>::Value, T>::Type* = nullptr
	>
	FORCEINLINE bool IsStruct(bool bIsHiddenArgument = false) const
	{
		return IsStruct(TBaseStructure<T>::Get(), bIsHiddenArgument);
	}

	// Returns true if this memory handle maps to a struct property
	template <
		typename T,
		typename TEnableIf<TModels<CRigVMUStruct, T>::Value>::Type * = nullptr
	>
	FORCEINLINE bool IsStruct(bool bIsHiddenArgument = false) const
	{
		return IsStruct(T::StaticStruct(), bIsHiddenArgument);
	}

	// Returns true if this memory handle maps to a struct array property
	template <
		typename T,
		typename TEnableIf<TRigVMIsBaseStructure<T>::Value, T>::Type* = nullptr
	>
	FORCEINLINE bool IsStructArray(bool bIsHiddenArgument = false) const
	{
		return IsStructArray(TBaseStructure<T>::Get(), bIsHiddenArgument);
	}

	// Returns true if this memory handle maps to a struct array property
	template <
		typename T,
		typename TEnableIf<TModels<CRigVMUStruct, T>::Value>::Type * = nullptr
	>
	FORCEINLINE bool IsStructArray(bool bIsHiddenArgument = false) const
	{
		return IsStructArray(T::StaticStruct(), bIsHiddenArgument);
	}

	// Returns true if this memory handle maps to an object property
	FORCEINLINE bool IsObject(const UClass* InClass, bool bIsHiddenArgument = false) const
	{
		const FProperty* ResolvedProperty = GetResolvedProperty();
		return IsObject(ResolvedProperty, InClass, bIsHiddenArgument);
	}

	// Returns true if this memory handle maps to an objct array property
	FORCEINLINE bool IsObjectArray(const UClass* InClass, bool bIsHiddenArgument = false) const
	{
		const FProperty* ResolvedProperty = GetResolvedProperty();
		if(bIsHiddenArgument)
		{
			ResolvedProperty = GetArrayElementPropertyChecked(ResolvedProperty);
		}
		if(const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(ResolvedProperty))
		{
			return IsObject(ArrayProperty->Inner, InClass, false);
		}
		return false;
	}

	// Returns true if this memory handle maps to an object property
	template <
		typename T,
		typename TEnableIf<TModels<CRigVMUClass, T>::Value>::Type * = nullptr
	>
	FORCEINLINE bool IsObject(bool bIsHiddenArgument = false) const
	{
		return IsObject(T::StaticClass(), bIsHiddenArgument);
	}

	// Returns true if this memory handle maps to an objct array property
	template <
		typename T,
		typename TEnableIf<TModels<CRigVMUClass, T>::Value>::Type * = nullptr
	>
	FORCEINLINE bool IsObjectArray(bool bIsHiddenArgument = false) const
	{
		return IsObjectArray(T::StaticClass(), bIsHiddenArgument);
	}

private:

	FORCEINLINE_DEBUGGABLE uint8* GetData_Internal(bool bFollowPropertyPath, int32 InSliceIndex) const
	{
		if(InSliceIndex != INDEX_NONE)
		{
			// sliced memory cannot be accessed
			// using a property path.
			// it refers to opaque memory only
			check(PropertyPath == nullptr);
			check(!bFollowPropertyPath);

			const FArrayProperty* ArrayProperty = CastFieldChecked<FArrayProperty>(Property);
			FScriptArrayHelper ArrayHelper(ArrayProperty, Ptr);
			if(ArrayHelper.Num() <= InSliceIndex)
			{
				const int32 NumValuesToAdd = 1 + InSliceIndex - ArrayHelper.Num();
				ArrayHelper.AddValues(NumValuesToAdd);
			}

			return ArrayHelper.GetRawPtr(InSliceIndex);
		}

		// traverse the property path to the tail property
		// and return its memory instead.
		if(bFollowPropertyPath && PropertyPath != nullptr)
		{
			return PropertyPath->GetData<uint8>(Ptr, Property);
		}
		return Ptr;
	}

	FORCEINLINE static const FProperty* GetArrayElementProperty(const FProperty* InProperty)
	{
		if(const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InProperty))
		{
			return ArrayProperty->Inner;
		}
		return nullptr;
	}

	FORCEINLINE static const FProperty* GetArrayElementPropertyChecked(const FProperty* InProperty)
	{
		const FArrayProperty* ArrayProperty = CastFieldChecked<FArrayProperty>(InProperty);
		return ArrayProperty->Inner;
	}

	FORCEINLINE static bool IsEnum(const FProperty* InProperty, const UEnum* InEnum, bool bUseArrayElement)
	{
		const FProperty* Property = InProperty;
		if(bUseArrayElement)
		{
			Property = GetArrayElementPropertyChecked(Property);
		}
		if(const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			if(EnumProperty->GetEnum() == InEnum)
			{
				return true;
			}
		}
		if(const FByteProperty* EnumProperty = CastField<FByteProperty>(Property))
		{
			if(EnumProperty->Enum == InEnum)
			{
				return true;
			}
		}
		return false;
	}

	FORCEINLINE static bool IsStruct(const FProperty* InProperty, const UScriptStruct* InScriptStruct, bool bUseArrayElement)
	{
		const FProperty* Property = InProperty;
		if(bUseArrayElement)
		{
			Property = GetArrayElementPropertyChecked(Property);
		}
		if(const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if(StructProperty->Struct == InScriptStruct)
			{
				return true;
			}
		}
		return false;
	}

	FORCEINLINE static bool IsObject(const FProperty* InProperty, const UClass* InClass, bool bUseArrayElement)
	{
		const FProperty* Property = InProperty;
		if(bUseArrayElement)
		{
			Property = GetArrayElementPropertyChecked(Property);
		}
		if(const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
		{
			if(ObjectProperty->PropertyClass == InClass)
			{
				return true;
			}
		}
		return false;
	}

	// The pointer of the memory of the head property
	uint8* Ptr;

	// The head property used by this handle
	const FProperty* Property;

	// The [optional] property path used by this handle
	const FRigVMPropertyPath* PropertyPath;

	friend class URigVM;
};

//////////////////////////////////////////////////////////////////////////////
/// Property Management
//////////////////////////////////////////////////////////////////////////////

/**
 * The property description is used to provide all required information
 * to create a property for the memory storage class.
 */
struct RIGVM_API FRigVMPropertyDescription
{
public:
	
	// The name of the property to create
	FName Name;

	// The property to base a new property off of
	const FProperty* Property;

	// The complete CPP type to base a new property off of (for ex: 'TArray<TArray<FVector>>')
	FString CPPType;

	// The tail CPP Type object, for example the UScriptStruct for a struct 
	UObject* CPPTypeObject;

	// A list of containers to use for this property, for example [Array, Array]
	TArray<EPinContainerType> Containers;

	// The default value to use for this property (for example: '(((X=1.000000, Y=2.000000, Z=3.000000)))')
	FString DefaultValue;

	// Default constructor
	FRigVMPropertyDescription()
		: Name(NAME_None)
		, Property(nullptr)
		, CPPType()
		, CPPTypeObject(nullptr)
		, Containers()
		, DefaultValue()
	{}

	// Constructor from an existing property
	FRigVMPropertyDescription(const FProperty* InProperty, const FString& InDefaultValue, const FName& InName = NAME_None);

	// Constructor from complete data
	FRigVMPropertyDescription(const FName& InName, const FString& InCPPType, UObject* InCPPTypeObject, const FString& InDefaultValue);

	// Returns a sanitized, valid name to use for a new property 
	static FName SanitizeName(const FName& InName);

	// Sanitize the name of this description in line
	void SanitizeName();

	// Returns true if this property description is valid
	bool IsValid() const { return !Name.IsNone(); }

	// Returns the CPP type of the tail property, for ex: '[2].Translation' it is 'FVector'
	FString GetTailCPPType() const;

	static bool RequiresCPPTypeObject(const FString& InCPPType);

private:
	
	static const FString ArrayPrefix;
	static const FString MapPrefix;
	static const FString ContainerSuffix;
};

/**
 * The URigVMMemoryStorageGeneratorClass is used to create / represent heterogeneous
 * memory storages. The generator can produce a UClass which contains a series of
 * properties. This UClass is then used to instantiate URigVMMemoryStorage objects
 * to be consumed by the RigVM. The memory storage objects can contain the literals
 * / constant values used by the virtual machine or work state.
 */
UCLASS()
class RIGVM_API URigVMMemoryStorageGeneratorClass :
	public UClass
{
	GENERATED_BODY()

	// DECLARE_WITHIN(UObject) is only kept for back-compat, please don't parent the class
	// to the asset object.
	// This class should be parented to the package, instead of the asset object
	// because the engine no longer supports asset object as UClass outer
	// Given in the past we have parented the class to the asset object,
	// this flag has to be kept such that we can load the old asset in the first place and
	// re-parent it back to the package in post load
	DECLARE_WITHIN(UObject)

public:

	// Default constructor
	URigVMMemoryStorageGeneratorClass()
		: Super()
		, MemoryType(ERigVMMemoryType::Literal)
		, CachedMemoryHash(0)
	{}
	
	// UClass overrides
	void PurgeClass(bool bRecompilingOnLoad) override;
	void Link(FArchive& Ar, bool bRelinkExistingProperties) override;
	void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;

	// Returns the name of a storage class for a given memory type. The name is unique within the package.
	static const FString& GetClassName(ERigVMMemoryType InMemoryType);

	// Returns an existing class for a memory type within the package (or nullptr) 
	static URigVMMemoryStorageGeneratorClass* GetStorageClass(UObject* InOuter, ERigVMMemoryType InMemoryType);

	/**
	 * Creates a new class provided a list of properties and property paths
	 * @param InOuter The package or the outer object of this class
	 * @param InMemoryType The memory type to use for this class (affects class name)
	 * @param InProperties The descriptions for the properties to create
	 * @param InPropertyPaths The descriptions for the property paths to create
	 */
	static URigVMMemoryStorageGeneratorClass* CreateStorageClass(
		UObject* InOuter,
		ERigVMMemoryType InMemoryType,
		const TArray<FRigVMPropertyDescription>& InProperties,
		const TArray<FRigVMPropertyPathDescription>& InPropertyPaths = TArray<FRigVMPropertyPathDescription>());

	/**
	 * Removes an existing storage class
	 * @param InOuter The package or the outer object of this class
	 * @param InMemoryType The memory type to use for this class (affects class name)
	 */
	static bool RemoveStorageClass(
		UObject* InOuter,
		ERigVMMemoryType InMemoryType);

	// Returns the type of memory of this class (literal, work, etc)
	ERigVMMemoryType GetMemoryType() const { return MemoryType; }

	// Returns a hash of unique to the configuration of the memory
	uint32 GetMemoryHash() const;

	// The properties stored within this class
	const TArray<const FProperty*>& GetProperties() const { return LinkedProperties; }

	// The property paths stored within this class
	const TArray<FRigVMPropertyPath>& GetPropertyPaths() const { return PropertyPaths; }

	// returns the statistics information
	FRigVMMemoryStatistics GetStatistics() const;

protected:

	// Adds a single property to a class.
	static FProperty* AddProperty(URigVMMemoryStorageGeneratorClass* InClass, const FRigVMPropertyDescription& InProperty, FField** LinkToProperty = nullptr);

public:

	void RefreshLinkedProperties();
	void RefreshPropertyPaths();

private:

	// The type of memory of this class
	ERigVMMemoryType MemoryType;

	// A cached list of all linked properties (created by RefreshLinkedProperties)
	TArray<const FProperty*> LinkedProperties;

	// A cached list of all property paths (created by RefreshPropertyPaths)
	TArray<FRigVMPropertyPath> PropertyPaths;

	// A list of decriptions for the property paths - used for serialization
	TArray<FRigVMPropertyPathDescription> PropertyPathDescriptions;

	mutable uint32 CachedMemoryHash;

	friend class URigVMMemoryStorage;
	friend class URigVMCompiler;
	friend class URigVM;
	friend struct FRigVMCodeGenerator;
};

/**
 * The URigVMMemoryStorage represents an instance of heterogeneous memory.
 * The memory layout is defined by the URigVMMemoryStorageGeneratorClass.
 */
UCLASS()
class RIGVM_API URigVMMemoryStorage : public UObject
{
	GENERATED_BODY()

public:

	//////////////////////////////////////////////////////////////////////////////
	/// Memory Access
	//////////////////////////////////////////////////////////////////////////////

	// Returns the memory type of this memory
	FORCEINLINE ERigVMMemoryType GetMemoryType() const
	{
		if(URigVMMemoryStorageGeneratorClass* Class = Cast<URigVMMemoryStorageGeneratorClass>(GetClass()))
		{
			return Class->GetMemoryType();
		}
		// empty debug containers don't have a generator class
		return ERigVMMemoryType::Debug;
	}

	// Returns a hash of unique to the configuration of the memory
	FORCEINLINE uint32 GetMemoryHash() const
	{
		if(URigVMMemoryStorageGeneratorClass* Class = Cast<URigVMMemoryStorageGeneratorClass>(GetClass()))
		{
			return Class->GetMemoryHash();
		}
		// empty debug containers don't have a generator class
		return 0;
	}

	// Returns the number of properties stored in this instance
	FORCEINLINE int32 Num() const
	{
		return GetProperties().Num();
	}

	// Returns true if a provided property index is valid
	FORCEINLINE bool IsValidIndex(int32 InIndex) const
	{
		return GetProperties().IsValidIndex(InIndex);
	}

	// Returns the properties provided by this instance
	const TArray<const FProperty*>& GetProperties() const;

	// Returns the property paths provided by this instance
	const TArray<FRigVMPropertyPath>& GetPropertyPaths() const;

	// Returns the index of a property given the property itself
	int32 GetPropertyIndex(const FProperty* InProperty) const;

	// Returns the index of a property given its name
	int32 GetPropertyIndexByName(const FName& InName) const;

	// Returns a property given its index
	FORCEINLINE const FProperty* GetProperty(int32 InPropertyIndex) const
	{
		return GetProperties()[InPropertyIndex];
	}

	// Returns a property given its name (or nullptr if the name wasn't found)
	FProperty* FindPropertyByName(const FName& InName) const;

	// Creates and returns a new operand for a property (and optionally a property path)
	FRigVMOperand GetOperand(int32 InPropertyIndex, int32 InPropertyPathIndex = INDEX_NONE) const;

	// Creates and returns a new operand for a property (and optionally a property path)
	FRigVMOperand GetOperandByName(const FName& InName, int32 InPropertyPathIndex = INDEX_NONE) const;

	// returns the statistics information
	FRigVMMemoryStatistics GetStatistics() const
	{
		if(URigVMMemoryStorageGeneratorClass* Class = Cast<URigVMMemoryStorageGeneratorClass>(GetClass()))
		{
			return Class->GetStatistics();
		}

		FRigVMMemoryStatistics Statistics;
		Statistics.RegisterCount = 0;
		Statistics.DataBytes = URigVMMemoryStorage::StaticClass()->GetStructureSize();
		Statistics.TotalBytes = Statistics.DataBytes;
		return Statistics;
	}

	// Returns true if the property at a given index is a TArray
	FORCEINLINE bool IsArray(int32 InPropertyIndex) const
	{
		return GetProperty(InPropertyIndex)->IsA<FArrayProperty>();
	}

	// Returns true if the property at a given index is a TMap
	FORCEINLINE bool IsMap(int32 InPropertyIndex) const
	{
		return GetProperty(InPropertyIndex)->IsA<FMapProperty>();
	}

	// Returns the memory for a property given its index
	template<typename T>
	FORCEINLINE T* GetData(int32 InPropertyIndex)
	{
		const TArray<const FProperty*>& Properties = GetProperties();
		check(Properties.IsValidIndex(InPropertyIndex));
		return Properties[InPropertyIndex]->ContainerPtrToValuePtr<T>(this);
	}

	// Returns the memory for a property given its name (or nullptr)
	template<typename T>
	FORCEINLINE T* GetDataByName(const FName& InName)
	{
		const int32 PropertyIndex = GetPropertyIndexByName(InName);
		if(PropertyIndex == INDEX_NONE)
		{
			return nullptr;
		}
		return GetData<T>(PropertyIndex);
	}

	// Returns the memory for a property given its index and a matching property path
	template<typename T>
	FORCEINLINE T* GetData(int32 InPropertyIndex, const FRigVMPropertyPath& InPropertyPath)
	{
		const FProperty* Property = GetProperty(InPropertyIndex);
		return InPropertyPath.GetData<T>(GetData<uint8>(InPropertyIndex), Property);
	}

	// Returns the memory for a property given its name and a matching property path (or nullptr)
	template<typename T>
	FORCEINLINE T* GetDataByName(const FName& InName, const FRigVMPropertyPath& InPropertyPath)
	{
		const int32 PropertyIndex = GetPropertyIndexByName(InName);
		if(PropertyIndex == INDEX_NONE)
		{
			return nullptr;
		}
		return GetData<T>(PropertyIndex, InPropertyPath);
	}

	// Returns the memory for a property (and optionally a property path) given an operand
	template<typename T>
	FORCEINLINE T* GetData(const FRigVMOperand& InOperand)
	{
		const int32 PropertyIndex = InOperand.GetRegisterIndex();
		const int32 PropertyPathIndex = InOperand.GetRegisterOffset();

		check(GetProperties().IsValidIndex(PropertyIndex));
		
		if(PropertyPathIndex == INDEX_NONE)
		{
			return GetData<T>(PropertyIndex);
		}

		check(GetPropertyPaths().IsValidIndex(PropertyPathIndex));
		return GetData<T>(PropertyIndex, GetPropertyPaths()[PropertyPathIndex]); 
	}

	// Returns the ref of an element stored at a given property index
	template<typename T>
	FORCEINLINE T& GetRef(int32 InPropertyIndex)
	{
		return *GetData<T>(InPropertyIndex);
	}

	// Returns the ref of an element stored at a given property name (throws if name is invalid)
	template<typename T>
	FORCEINLINE T& GetRefByName(const FName& InName)
	{
		return *GetDataByName<T>(InName);
	}

	// Returns the ref of an element stored at a given property index and a property path
	template<typename T>
	FORCEINLINE T& GetRef(int32 InPropertyIndex, const FRigVMPropertyPath& InPropertyPath)
	{
		return *GetData<T>(InPropertyIndex, InPropertyPath);
	}

	// Returns the ref of an element stored at a given property name and a property path (throws if name is invalid)
	template<typename T>
	FORCEINLINE T& GetRefByName(const FName& InName, const FRigVMPropertyPath& InPropertyPath)
	{
		return *GetDataByName<T>(InName, InPropertyPath);
	}

	// Returns the ref of an element stored for a given operand
	template<typename T>
	FORCEINLINE T& GetRef(const FRigVMOperand& InOperand)
	{
		return *GetData<T>(InOperand);
	}

	// Returns the exported text for a given property index
	FString GetDataAsString(int32 InPropertyIndex, int32 PortFlags = PPF_None);

	// Returns the exported text for given property name 
	FORCEINLINE FString GetDataAsStringByName(const FName& InName, int32 PortFlags = PPF_None)
	{
		const int32 PropertyIndex = GetPropertyIndexByName(InName);
		return GetDataAsString(PropertyIndex, PortFlags);
	}

	// Returns the exported text for a given operand
	FString GetDataAsString(const FRigVMOperand& InOperand, int32 PortFlags = PPF_None);

	// Returns the exported text for a given property index
	FString GetDataAsStringSafe(int32 InPropertyIndex, int32 PortFlags = PPF_None);

	// Returns the exported text for given property name 
	FORCEINLINE FString GetDataAsStringByNameSafe(const FName& InName, int32 PortFlags = PPF_None)
	{
		const int32 PropertyIndex = GetPropertyIndexByName(InName);
		return GetDataAsStringSafe(PropertyIndex, PortFlags);
	}

	// Returns the exported text for a given operand
	FString GetDataAsStringSafe(const FRigVMOperand& InOperand, int32 PortFlags = PPF_None);

	// Sets the content of a property by index given an exported string. Returns true if succeeded
	bool SetDataFromString(int32 InPropertyIndex, const FString& InValue);

	// Sets the content of a property by name given an exported string. Returns true if succeeded
	FORCEINLINE bool SetDataFromStringByName(const FName& InName, const FString& InValue)
	{
		const int32 PropertyIndex = GetPropertyIndexByName(InName);
		return SetDataFromString(PropertyIndex, InValue);
	}

	// Returns the handle for a given property by index (and optionally property path)
	FRigVMMemoryHandle GetHandle(int32 InPropertyIndex, const FRigVMPropertyPath* InPropertyPath = nullptr);

	// Returns the handle for a given property by name (and optionally property path)
	FORCEINLINE FRigVMMemoryHandle GetHandleByName(const FName& InName, const FRigVMPropertyPath* InPropertyPath = nullptr)
	{
		const int32 PropertyIndex = GetPropertyIndexByName(InName);
		return GetHandle(PropertyIndex, InPropertyPath);
	}

	/**
	 * Copies the content of a source property and memory into the memory of a target property
	 * @param InTargetProperty The target property to copy into
	 * @param InTargetPtr The memory of the target value to copy into
	 * @param InSourceProperty The source property to copy from
	 * @param InSourcePtr The memory of the source value to copy from
	 * @return true if the copy operations was successful
	 */ 
	static bool CopyProperty(
		const FProperty* InTargetProperty,
		uint8* InTargetPtr,
		const FProperty* InSourceProperty,
		const uint8* InSourcePtr);

	/**
	 * Copies the content of a source property and memory into the memory of a target property.
	 * This variant supports property paths for both target and source.
	 * @param InTargetProperty The target property to copy into
	 * @param InTargetPtr The memory of the target value to copy into
	 * @param InTargetPropertyPath The property path to use when traversing the target memory.
	 * @param InSourceProperty The source property to copy from
	 * @param InSourcePtr The memory of the source value to copy from
	 * @param InSourcePropertyPath The property path to use when traversing the source memory.
	 * @return true if the copy operations was successful
	 */ 
	static bool CopyProperty(
		const FProperty* InTargetProperty,
		uint8* InTargetPtr,
		const FRigVMPropertyPath& InTargetPropertyPath,
		const FProperty* InSourceProperty,
		const uint8* InSourcePtr,
		const FRigVMPropertyPath& InSourcePropertyPath);

	/**
	 * Copies the content of a source property and memory into the memory of a target property.
	 * This variant retrieves the memory pointers given target and source storage objects.
	 * @param InTargetStorage The target property to copy into
	 * @param InTargetPropertyIndex The index of the value to use on the InTargetStorage
	 * @param InTargetPropertyPath The property path to use when traversing the target memory.
	 * @param InSourceStorage The source property to copy from
	 * @param InSourcePropertyIndex The index of the value to use on the InSourceStorage
	 * @param InSourcePropertyPath The property path to use when traversing the source memory.
	 * @return true if the copy operations was successful
	 */ 
	static bool CopyProperty(
		URigVMMemoryStorage* InTargetStorage,
		int32 InTargetPropertyIndex,
		const FRigVMPropertyPath& InTargetPropertyPath,
		URigVMMemoryStorage* InSourceStorage,
		int32 InSourcePropertyIndex,
		const FRigVMPropertyPath& InSourcePropertyPath);

	/**
	 * Copies the content of a source property and memory into the memory of a target property.
	 * This variant retrieves the memory pointers given target and source storage objects.
	 * @param InTargetHandle The handle of the target memory to copy into
	 * @param InSourceHandle The handle of the source memory to copy from
	 * @return true if the copy operations was successful
	 */ 
	static bool CopyProperty(
		FRigVMMemoryHandle& InTargetHandle,
		FRigVMMemoryHandle& InSourceHandle);

	void CopyFrom(URigVMMemoryStorage* InSourceMemory);

private:
	
	static const TArray<const FProperty*> EmptyProperties;
	static const TArray<FRigVMPropertyPath> EmptyPropertyPaths;
};
