// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EngineDefines.h"
#include "Engine/EngineTypes.h"
#include "StructView.h"
#include "Misc/Guid.h"
#include "PropertyBindingPath.generated.h"

struct FPropertyBindingPath;



/**
 * Descriptor for a struct or class that can be a binding source or target.
 * Each struct has unique identifier, which is used to distinguish them, and name that is mostly for debugging and UI.
 */
USTRUCT()
struct PROPERTYBINDINGUTILS_API FBindableStructDesc
{
	GENERATED_BODY()

	FBindableStructDesc() = default;

#if WITH_EDITORONLY_DATA
	FBindableStructDesc(const FName InName, const UStruct* InStruct, const FGuid InGuid)
		: Struct(InStruct)
		, Name(InName)
		, ID(InGuid)
	{
	}

	bool operator==(const FBindableStructDesc& RHS) const
	{
		return ID == RHS.ID && Struct == RHS.Struct; // Not checking name, it's cosmetic.
	}
#endif

	bool IsValid() const { return Struct != nullptr; }

	FString ToString() const;
	
	/** The type of the struct or class. */
	UPROPERTY()
	TObjectPtr<const UStruct> Struct = nullptr;

	/** Name of the struct (used for debugging, logging, cosmetic). */
	UPROPERTY()
	FName Name;

#if WITH_EDITORONLY_DATA
	/** Unique identifier of the struct. */
	UPROPERTY()
	FGuid ID;
#endif
};


UENUM()
enum class EPropertyBindingAccessType : uint8
{
	Offset,			// Access node is a simple basePtr + offset
	Object,			// Access node needs to dereference an object at its current address
	WeakObject,		// Access is a weak object
	SoftObject,		// Access is a soft object
	ObjectInstance,	// Access node needs to dereference an object of specific type at its current address
	StructInstance,	// Access node needs to dereference an instanced struct of specific type at its current address
	IndexArray,		// Access node indexes a dynamic array
};

/**
 * Short lived pointer to an object or struct.
 * The data view expects a type (UStruct) when you pass in a valid memory. In case of null, the type can be empty too.
 */
struct PROPERTYBINDINGUTILS_API FPropertyBindingDataView
{
	FPropertyBindingDataView() = default;

	// Generic struct constructor.
	FPropertyBindingDataView(const UStruct* InStruct, void* InMemory) : Struct(InStruct), Memory(InMemory)
	{
		// Must have type with valid pointer.
		check(!Memory || (Memory && Struct));
	}

	// UObject constructor.
	FPropertyBindingDataView(UObject* Object) : Struct(Object ? Object->GetClass() : nullptr), Memory(Object)
	{
		// Must have type with valid pointer.
		check(!Memory || (Memory && Struct));
	}

	// Struct from a StructView.
	FPropertyBindingDataView(FStructView StructView) : Struct(StructView.GetScriptStruct()), Memory(StructView.GetMemory())
	{
		// Must have type with valid pointer.
		check(!Memory || (Memory && Struct));
	}

	/**
	 * Check is the view is valid (both pointer and type are set). On valid views it is safe to call the Get<>() methods returning a reference.
	 * @return True if the view is valid.
	*/
	bool IsValid() const
	{
		return Memory != nullptr && Struct != nullptr;
	}

	/*
	 * UObject getters (reference & pointer, const & mutable)
	 */
	template <typename T>
    typename TEnableIf<TIsDerivedFrom<T, UObject>::IsDerived, const T&>::Type Get() const
	{
		check(Memory != nullptr);
		check(Struct != nullptr);
		check(Struct->IsChildOf(T::StaticClass()));
		return *((T*)Memory);
	}

	template <typename T>
	typename TEnableIf<TIsDerivedFrom<T, UObject>::IsDerived, T&>::Type GetMutable() const
	{
		check(Memory != nullptr);
		check(Struct != nullptr);
		check(Struct->IsChildOf(T::StaticClass()));
		return *((T*)Memory);
	}

	template <typename T>
	typename TEnableIf<TIsDerivedFrom<T, UObject>::IsDerived, const T*>::Type GetPtr() const
	{
		// If Memory is set, expect Struct too. Otherwise, let nulls pass through.
		check(!Memory || (Memory && Struct));
		check(!Struct || Struct->IsChildOf(T::StaticClass()));
		return ((T*)Memory);
	}

	template <typename T>
    typename TEnableIf<TIsDerivedFrom<T, UObject>::IsDerived, T*>::Type GetMutablePtr() const
	{
		// If Memory is set, expect Struct too. Otherwise, let nulls pass through.
		check(!Memory || (Memory && Struct));
		check(!Struct || Struct->IsChildOf(T::StaticClass()));
		return ((T*)Memory);
	}

	/*
	 * Struct getters (reference & pointer, const & mutable)
	 */
	template <typename T>
	typename TEnableIf<!TIsDerivedFrom<T, UObject>::IsDerived && !TIsIInterface<T>::Value, const T&>::Type Get() const
	{
		check(Memory != nullptr);
		check(Struct != nullptr);
		check(Struct->IsChildOf(T::StaticStruct()));
		return *((T*)Memory);
	}

	template <typename T>
    typename TEnableIf<!TIsDerivedFrom<T, UObject>::IsDerived && !TIsIInterface<T>::Value, T&>::Type GetMutable() const
	{
		check(Memory != nullptr);
		check(Struct != nullptr);
		check(Struct->IsChildOf(T::StaticStruct()));
		return *((T*)Memory);
	}

	template <typename T>
    typename TEnableIf<!TIsDerivedFrom<T, UObject>::IsDerived && !TIsIInterface<T>::Value, const T*>::Type GetPtr() const
	{
		// If Memory is set, expect Struct too. Otherwise, let nulls pass through.
		check(!Memory || (Memory && Struct));
		check(!Struct || Struct->IsChildOf(T::StaticStruct()));
		return ((T*)Memory);
	}

	template <typename T>
    typename TEnableIf<!TIsDerivedFrom<T, UObject>::IsDerived && !TIsIInterface<T>::Value, T*>::Type GetMutablePtr() const
	{
		// If Memory is set, expect Struct too. Otherwise, let nulls pass through.
		check(!Memory || (Memory && Struct));
		check(!Struct || Struct->IsChildOf(T::StaticStruct()));
		return ((T*)Memory);
	}

	/*
	 * IInterface getters (reference & pointer, const & mutable)
	 */
	template <typename T>
	typename TEnableIf<TIsIInterface<T>::Value, const T&>::Type Get() const
	{
		check(!Memory || (Memory && Struct));
		check(Struct->IsChildOf(UObject::StaticClass()) && ((UClass*)Struct)->ImplementsInterface(T::UClassType::StaticClass()));
		return *(T*)((UObject*)Memory)->GetInterfaceAddress(T::UClassType::StaticClass());
	}

	template <typename T>
    typename TEnableIf<TIsIInterface<T>::Value, T&>::Type GetMutable() const
	{
		check(!Memory || (Memory && Struct));
		check(Struct->IsChildOf(UObject::StaticClass()) && ((UClass*)Struct)->ImplementsInterface(T::UClassType::StaticClass()));
		return *(T*)((UObject*)Memory)->GetInterfaceAddress(T::UClassType::StaticClass());
	}

	template <typename T>
    typename TEnableIf<TIsIInterface<T>::Value, const T*>::Type GetPtr() const
	{
		check(!Memory || (Memory && Struct));
		check(Struct->IsChildOf(UObject::StaticClass()) && ((UClass*)Struct)->ImplementsInterface(T::UClassType::StaticClass()));
		return (T*)((UObject*)Memory)->GetInterfaceAddress(T::UClassType::StaticClass());
	}

	template <typename T>
    typename TEnableIf<TIsIInterface<T>::Value, T*>::Type GetMutablePtr() const
	{
		check(!Memory || (Memory && Struct));
		check(Struct->IsChildOf(UObject::StaticClass()) && ((UClass*)Struct)->ImplementsInterface(T::UClassType::StaticClass()));
		return (T*)((UObject*)Memory)->GetInterfaceAddress(T::UClassType::StaticClass());
	}

	/** @return Struct describing the data type. */
	const UStruct* GetStruct() const
	{
		return Struct;
	}

	/** @return Raw const pointer to the data. */
	const void* GetMemory() const
	{
		return Memory;
	}

	/** @return Raw mutable pointer to the data. */
	void* GetMutableMemory() const
	{
		return Memory;
	}
	
protected:
	/** UClass or UScriptStruct of the data. */
	const UStruct* Struct = nullptr;

	/** Memory pointing at the class or struct */
	void* Memory = nullptr;
};



/**
 * Struct describing an indirection at specific segment at path.
 * Returned by FPropertyBindingPath::ResolveIndirections() and FPropertyBindingPath::ResolveIndirectionsWithValue().
 * Generally there's one indirection per FProperty. Containers have one path segment but two indirection (container + inner type).
 */
struct PROPERTYBINDINGUTILS_API FPropertyBindingPathIndirection
{
	FPropertyBindingPathIndirection() = default;
	explicit FPropertyBindingPathIndirection(const UStruct* InContainerStruct)
		: ContainerStruct(InContainerStruct)
	{
	}

	const FProperty* GetProperty() const
	{
		return Property;
	}

	const void* GetContainerAddress() const
	{
		return ContainerAddress;
	}

	const UStruct* GetInstanceStruct() const
	{
		return InstanceStruct;
	}

	const UStruct* GetContainerStruct() const
	{
		return ContainerStruct;
	}

	int32 GetArrayIndex() const
	{
		return ArrayIndex;
	}

	int32 GetPropertyOffset() const
	{
		return PropertyOffset;
	}

	int32 GetPathSegmentIndex() const
	{
		return PathSegmentIndex;
	}

	EPropertyBindingAccessType GetAccessType() const
	{
		return AccessType;
	}
	
	const void* GetPropertyAddress() const
	{
		return (uint8*)ContainerAddress + PropertyOffset;
	}

	void* GetMutablePropertyAddress() const
	{
		return (uint8*)ContainerAddress + PropertyOffset;
	}

#if WITH_EDITORONLY_DATA
	FName GetRedirectedName() const
	{
		return RedirectedName;
	}
	
	FGuid GetPropertyGuid() const
	{
		return PropertyGuid;
	}
#endif
	
private:
	/** Property at the indirection. */
	const FProperty* Property = nullptr;
	
	/** Address of the container class/struct where the property belongs to. Only valid if created with ResolveIndirectionsWithValue() */
	const void* ContainerAddress = nullptr;
	
	/** Type of the container class/struct. */
	const UStruct* ContainerStruct = nullptr;
	
	/** Type of the instance class/struct of when AccessType is ObjectInstance or StructInstance. */
	const UStruct* InstanceStruct = nullptr;

	/** Array index for static and dynamic arrays. Note: static array indexing is baked in the PropertyOffset. */
	int32 ArrayIndex = 0;
	
	/** Offset of the property relative to ContainerAddress. Includes static array indexing. */
	int32 PropertyOffset = 0;
	
	/** Index of the path segment where indirection originated from. */
	int32 PathSegmentIndex = 0;
	
	/** How to access the data through the indirection. */
	EPropertyBindingAccessType AccessType = EPropertyBindingAccessType::Offset;

#if WITH_EDITORONLY_DATA
	/** Redirected name, if the give property name was not found but was reconciled using core redirect or property Guid. Requires ResolveIndirections/WithValue() to be called with bHandleRedirects = true. */
	FName RedirectedName;

	/** Guid of the property for Blueprint classes or User Defined Structs. Requires ResolveIndirections/WithValue() to be called with bHandleRedirects = true. */
	FGuid PropertyGuid;
#endif

	friend FPropertyBindingPath;
};

/** Struct describing a path segment in FPropertyBindingPath. */
USTRUCT()
struct PROPERTYBINDINGUTILS_API FPropertyBindingPathSegment
{
	GENERATED_BODY()

	FPropertyBindingPathSegment() = default;

	explicit FPropertyBindingPathSegment(const FName InName, const int32 InArrayIndex = INDEX_NONE, const UStruct* InInstanceStruct = nullptr)
		: Name(InName)
		, ArrayIndex(InArrayIndex)
		, InstanceStruct(InInstanceStruct)
	{
	}

	bool operator==(const FPropertyBindingPathSegment& RHS) const
	{
		return Name == RHS.Name && InstanceStruct == RHS.InstanceStruct && ArrayIndex == RHS.ArrayIndex;
	}

	bool operator!=(const FPropertyBindingPathSegment& RHS) const
	{
		return !(*this == RHS);
	}

	void SetName(const FName InName)
	{
		Name = InName;
	}

	FName GetName() const
	{
		return Name;
	}

	void SetArrayIndex(const int32 InArrayIndex)
	{
		ArrayIndex = InArrayIndex;
	}

	int32 GetArrayIndex() const
	{
		return ArrayIndex;
	}

	void SetInstanceStruct(const UStruct* InInstanceStruct)
	{
		InstanceStruct = InInstanceStruct;
	}

	const UStruct* GetInstanceStruct() const
	{
		return InstanceStruct;
	}

#if WITH_EDITORONLY_DATA
	FGuid GetPropertyGuid() const
	{
		return PropertyGuid;
	}

	void SetPropertyGuid(const FGuid NewGuid)
	{
		PropertyGuid = NewGuid;
	}
#endif
	
private:
	/** Name of the property */
	UPROPERTY()
	FName Name;

	/** Array index if the property is dynamic or static array. */
	UPROPERTY()
	int32 ArrayIndex = INDEX_NONE;

	/** Type of the instanced struct or object reference by the property at the segment. This allows the path to be resolved when it points to a specific instance. */
	UPROPERTY()
	TObjectPtr<const UStruct> InstanceStruct = nullptr;

#if WITH_EDITORONLY_DATA
	/** Guid of the property for Blueprint classes, User Defined Structs, or Property Bags. */
	FGuid PropertyGuid;
#endif
};


/**
 * Representation of a property path that can be used for property access and binding.
 *
 * The engine supports many types of property paths, this implementation has these specific properties:
 *		- Allow to resolve all the indirections from a base value (object or struct) up to the leaf property
 *		- handle redirects from Core Redirect, BP classes, User Defines Structs and Property Bags
 *
 * You may also take a look at: FCachedPropertyPath, TFieldPath<>, FPropertyPath.
 */
USTRUCT()
struct PROPERTYBINDINGUTILS_API FPropertyBindingPath
{
	GENERATED_BODY()

	FPropertyBindingPath() = default;

#if WITH_EDITORONLY_DATA
	explicit FPropertyBindingPath(const FGuid InStructID)
		: StructID(InStructID)
	{
	}

	explicit FPropertyBindingPath(const FGuid InStructID, const FName PropertyName)
		: StructID(InStructID)
	{
		Segments.Emplace(PropertyName);
	}
	
	explicit FPropertyBindingPath(const FGuid InStructID, TConstArrayView<FPropertyBindingPathSegment> InSegments)
		: StructID(InStructID)
		, Segments(InSegments)
	{
	}
#endif // WITH_EDITORONLY_DATA
	
	/**
	 * Parses path from string. The path should be in format: Foo.Bar[1].Baz
	 * @param InPath Path string to parse
	 * @return true if path was parsed successfully.
	 */
	bool FromString(const FString& InPath);

	/**
	 * Returns the property path as a one string. Highlight allows to decorate a specific segment.
	 * @param HighlightedSegment Index of the highlighted path segment
	 * @param HighlightPrefix String to append before highlighted segment
	 * @param HighlightPostfix String to append after highlighted segment
	 * @param bOutputInstances if true, the instance struct types will be output. 
	 */
	FString ToString(const int32 HighlightedSegment = INDEX_NONE, const TCHAR* HighlightPrefix = nullptr, const TCHAR* HighlightPostfix = nullptr, const bool bOutputInstances = false) const;

	/**
	 * Resolves the property path against base struct type. The path is assumed to be relative to the BaseStruct.
	 * @param BaseStruct Base struct/class type the path is relative to.
	 * @param OutIndirections Indirections describing how the properties were accessed. 
	 * @param OutError Optional, pointer to string where error will be logged if update fails.
	 * @param bHandleRedirects If true, the method will try to resolve missing properties using core redirects, and properties on Blueprint and User Defined Structs by ID. Available only in editor builds!
	 * @return true of path could be resolved against the base value, and instance types were updated.
	 */
	bool ResolveIndirections(const UStruct* BaseStruct, TArray<FPropertyBindingPathIndirection>& OutIndirections, FString* OutError = nullptr, bool bHandleRedirects = false) const;
	
	/**
	 * Resolves the property path against base value. The path is assumed to be relative to the BaseValueView.
	 * @param BaseValueView Base value the path is relative to.
	 * @param OutIndirections Indirections describing how the properties were accessed. 
	 * @param OutError Optional, pointer to string where error will be logged if update fails.
	 * @param bHandleRedirects If true, the method will try to resolve missing properties using core redirects, and properties on Blueprint and User Defined Structs by ID. Available only in editor builds!
	 * @return true of path could be resolved against the base value, and instance types were updated.
	 */
	bool ResolveIndirectionsWithValue(const FPropertyBindingDataView BaseValueView, TArray<FPropertyBindingPathIndirection>& OutIndirections, FString* OutError = nullptr, bool bHandleRedirects = false) const;

	/**
	 * Updates property segments from base struct type. The path is expected to be relative to the BaseStruct.
	 * The method handles renamed properties (core redirect, Blueprint, User Defined Structs and Property Bags by ID).
	 * @param BaseStruct Base value the path is relative to.
	 * @param OutError Optional, pointer to string where error will be logged if update fails.
	 * @return true of path could be resolved against the base value, and instance types were updated.
	 */
	bool UpdateSegments(const UStruct* BaseStruct, FString* OutError = nullptr);

	/**
	 * Updates property segments from base value. The path is expected to be relative to the base value.
	 * The method updates instance types, and handles renamed properties (core redirect, Blueprint, User Defined Structs and Property Bags by ID).
	 * By storing the instance types on the path, we can resolve the path without the base value later.
	 * @param BaseValueView Base value the path is relative to.
	 * @param OutError Optional, pointer to string where error will be logged if update fails.
	 * @return true of path could be resolved against the base value, and instance types were updated.
	 */
	bool UpdateSegmentsFromValue(const FPropertyBindingDataView BaseValueView, FString* OutError = nullptr);

	/** @return true if the path is empty. In that case the path points to the struct. */
	bool IsPathEmpty() const
	{
		return Segments.IsEmpty();
	}

	/** @return true if any of the path segments is and indirection via instanced struct or object. */
	bool HasAnyInstancedIndirection() const
	{
		return Segments.ContainsByPredicate([](const FPropertyBindingPathSegment& Segment)
		{
			return Segment.GetInstanceStruct() != nullptr;
		});
	}

	/** Reset the path to empty. */
	void Reset()
	{
#if WITH_EDITORONLY_DATA
		StructID = FGuid();
#endif
		Segments.Reset();
	}

#if WITH_EDITORONLY_DATA
	const FGuid& GetStructID() const
	{
		return StructID;
	}
	
	void SetStructID(const FGuid NewStructID)
	{
		StructID = NewStructID;
	}
#endif // WITH_EDITORONLY_DATA

	TConstArrayView<FPropertyBindingPathSegment> GetSegments() const
	{
		return Segments;
	}
	
	TArrayView<FPropertyBindingPathSegment> GetMutableSegments()
	{
		return Segments;
	}
	
	int32 NumSegments() const
	{
		return Segments.Num();
	}
	
	const FPropertyBindingPathSegment& GetSegment(const int32 Index) const
	{
		return Segments[Index];
	}

	/** Adds a path segment to the path. */
	void AddPathSegment(const FName InName, const int32 InArrayIndex = INDEX_NONE, const UStruct* InInstanceType = nullptr)
	{
		Segments.Emplace(InName, InArrayIndex, InInstanceType);
	}

	/** Adds a path segment to the path. */
	void AddPathSegment(const FPropertyBindingPathSegment& PathSegment)
	{
		Segments.Add(PathSegment);
	}

	/** Test if paths are equal. */
	bool operator==(const FPropertyBindingPath& RHS) const;

private:
#if WITH_EDITORONLY_DATA
	/** ID of the struct this property path is relative to. */
	UPROPERTY()
	FGuid StructID;
#endif // WITH_EDITORONLY_DATA

	/** Path segments pointing to a specific property on the path. */
	UPROPERTY()
	TArray<FPropertyBindingPathSegment> Segments;
};