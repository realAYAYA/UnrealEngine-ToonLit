// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeTypes.h"
#include "StateTreePropertyRefHelpers.h"
#include "StateTreePropertyBindings.generated.h"

class FProperty;
struct FStateTreePropertyPath;
struct FStateTreePropertyBindingCompiler;
struct FStateTreePropertyRef;
class UStateTree;

UENUM()
enum class EStateTreeBindableStructSource : uint8
{
	/** Source is StateTree context object */
	Context,
	/** Source is StateTree parameter */
	Parameter,
	/** Source is StateTree evaluator */
	Evaluator,
	/** Source is StateTree global task */
	GlobalTask,
	/** Source is State parameter */
	State,
	/** Source is State task */
	Task,
	/** Source is State condition */
	Condition,
};


/**
 * The type of access a property path describes.
 */
UENUM()
enum class EStateTreePropertyAccessType : uint8
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
 * Describes how the copy should be performed.
 */
UENUM()
enum class EStateTreePropertyCopyType : uint8
{
	None,						// No copying
	
	CopyPlain,					// For plain old data types, we do a simple memcpy.
	CopyComplex,				// For more complex data types, we need to call the properties copy function
	CopyBool,					// Read and write properties using bool property helpers, as source/dest could be bitfield or boolean
	CopyStruct,					// Use struct copy operation, as this needs to correctly handle CPP struct ops
	CopyObject,					// Read and write properties using object property helpers, as source/dest could be regular/weak/soft etc.
	CopyName,					// FName needs special case because its size changes between editor/compiler and runtime.
	CopyFixedArray,				// Array needs special handling for fixed size TArrays

	StructReference,			// Copies pointer to a source struct into a FStateTreeStructRef.

	/* Promote the type during the copy */

	/* Bool promotions */
	PromoteBoolToByte,
	PromoteBoolToInt32,
	PromoteBoolToUInt32,
	PromoteBoolToInt64,
	PromoteBoolToFloat,
	PromoteBoolToDouble,

	/* Byte promotions */
	PromoteByteToInt32,
	PromoteByteToUInt32,
	PromoteByteToInt64,
	PromoteByteToFloat,
	PromoteByteToDouble,

	/* Int32 promotions */
	PromoteInt32ToInt64,
	PromoteInt32ToFloat,	// This is strictly sketchy because of potential data loss, but it is usually OK in the general case
	PromoteInt32ToDouble,

	/* UInt32 promotions */
	PromoteUInt32ToInt64,
	PromoteUInt32ToFloat,	// This is strictly sketchy because of potential data loss, but it is usually OK in the general case
	PromoteUInt32ToDouble,

	/* Float promotions */
	PromoteFloatToInt32,
	PromoteFloatToInt64,
	PromoteFloatToDouble,

	/* Double promotions */
	DemoteDoubleToInt32,
	DemoteDoubleToInt64,
	DemoteDoubleToFloat,
};

/** Enum describing property compatibility */
enum class EStateTreePropertyAccessCompatibility
{
	/** Properties are incompatible */
	Incompatible,
	/** Properties are directly compatible */
	Compatible,	
	/** Properties can be copied with a simple type promotion */
	Promotable,
};

/**
 * Descriptor for a struct or class that can be a binding source or target.
 * Each struct has unique identifier, which is used to distinguish them, and name that is mostly for debugging and UI.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreeBindableStructDesc
{
	GENERATED_BODY()

	FStateTreeBindableStructDesc() = default;

#if WITH_EDITORONLY_DATA
	FStateTreeBindableStructDesc(const FName InName, const UStruct* InStruct, const FStateTreeDataHandle InDataHandle, const EStateTreeBindableStructSource InDataSource, const FGuid InGuid)
		: Struct(InStruct)
		, Name(InName)
		, DataHandle(InDataHandle)
		, DataSource(InDataSource)
		, ID(InGuid)
	{
	}

	UE_DEPRECATED(5.4, "Use constructor with DataHandle instead.")
	FStateTreeBindableStructDesc(const FName InName, const UStruct* InStruct, const EStateTreeBindableStructSource InDataSource, const FGuid InGuid)
		: Struct(InStruct)
		, Name(InName)
		, DataSource(InDataSource)
		, ID(InGuid)
	{
	}

	bool operator==(const FStateTreeBindableStructDesc& RHS) const
	{
		return ID == RHS.ID && Struct == RHS.Struct; // Not checking name, it's cosmetic.
	}
#endif

	bool IsValid() const { return Struct != nullptr; }

	FString ToString() const;
	
	/** The type of the struct or class. */
	UPROPERTY()
	TObjectPtr<const UStruct> Struct = nullptr;

	/** Name of the container (cosmetic). */
	UPROPERTY()
	FName Name;

	/** Runtime data the struct represents. */
	UPROPERTY()
	FStateTreeDataHandle DataHandle = FStateTreeDataHandle::Invalid;

	/** Type of the source. */
	UPROPERTY()
	EStateTreeBindableStructSource DataSource = EStateTreeBindableStructSource::Context;

#if WITH_EDITORONLY_DATA
	/** Unique identifier of the struct. */
	UPROPERTY()
	FGuid ID;
#endif
};

/** Struct describing a path segment in FStateTreePropertyPath. */
USTRUCT()
struct STATETREEMODULE_API FStateTreePropertyPathSegment
{
	GENERATED_BODY()

	FStateTreePropertyPathSegment() = default;

	explicit FStateTreePropertyPathSegment(const FName InName, const int32 InArrayIndex = INDEX_NONE, const UStruct* InInstanceStruct = nullptr)
		: Name(InName)
		, ArrayIndex(InArrayIndex)
		, InstanceStruct(InInstanceStruct)
	{
	}

	bool operator==(const FStateTreePropertyPathSegment& RHS) const
	{
		return Name == RHS.Name && InstanceStruct == RHS.InstanceStruct && ArrayIndex == RHS.ArrayIndex;
	}

	bool operator!=(const FStateTreePropertyPathSegment& RHS) const
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

#if WITH_EDITORONLY_DATA
	/** Guid of the property for Blueprint classes or User Defined Structs. */
	FGuid PropertyGuid;
#endif

	/** Array index if the property is dynamic or static array. */
	UPROPERTY()
	int32 ArrayIndex = INDEX_NONE;

	/** Type of the instanced struct or object reference by the property at the segment. This allows the path to be resolved when it points to a specific instance. */
	UPROPERTY()
	TObjectPtr<const UStruct> InstanceStruct = nullptr;
};

/**
 * Struct describing an indirection at specific segment at path.
 * Returned by FStateTreePropertyPath::ResolveIndirections() and FStateTreePropertyPath::ResolveIndirectionsWithValue().
 * Generally there's one indirection per FProperty. Containers have one path segment but two indirection (container + inner type).
 */
struct FStateTreePropertyPathIndirection
{
	FStateTreePropertyPathIndirection() = default;
	explicit FStateTreePropertyPathIndirection(const UStruct* InContainerStruct)
		: ContainerStruct(InContainerStruct)
	{
	}

	const FProperty* GetProperty() const { return Property; }
	const uint8* GetContainerAddress() const { return ContainerAddress; }
	const UStruct* GetInstanceStruct() const { return InstanceStruct; }
	const UStruct* GetContainerStruct() const { return ContainerStruct; }
	int32 GetArrayIndex() const { return ArrayIndex; }
	int32 GetPropertyOffset() const { return PropertyOffset; }
	int32 GetPathSegmentIndex() const { return PathSegmentIndex; }
	EStateTreePropertyAccessType GetAccessType() const { return AccessType; }
	const uint8* GetPropertyAddress() const { return ContainerAddress + PropertyOffset; }

#if WITH_EDITORONLY_DATA
	FName GetRedirectedName() const { return RedirectedName; }
	FGuid GetPropertyGuid() const { return PropertyGuid; }
#endif
	
private:
	/** Property at the indirection. */
	const FProperty* Property = nullptr;
	
	/** Address of the container class/struct where the property belongs to. Only valid if created with ResolveIndirectionsWithValue() */
	const uint8* ContainerAddress = nullptr;
	
	/** Type of the container class/struct. */
	const UStruct* ContainerStruct = nullptr;
	
	/** Type of the instance class/struct of when AccessType is ObjectInstance or StructInstance. */
	const UStruct* InstanceStruct = nullptr;

#if WITH_EDITORONLY_DATA
	/** Redirected name, if the give property name was not found but was reconciled using core redirect or property Guid. Requires ResolveIndirections/WithValue() to be called with bHandleRedirects = true. */
	FName RedirectedName;

	/** Guid of the property for Blueprint classes or User Defined Structs. Requires ResolveIndirections/WithValue() to be called with bHandleRedirects = true. */
	FGuid PropertyGuid;
#endif
	
	/** Array index for static and dynamic arrays. Note: static array indexing is baked in the PropertyOffset. */
	int32 ArrayIndex = 0;
	
	/** Offset of the property relative to ContainerAddress. Includes static array indexing. */
	int32 PropertyOffset = 0;
	
	/** Index of the path segment where indirection originated from. */
	int32 PathSegmentIndex = 0;
	
	/** How to access the data through the indirection. */
	EStateTreePropertyAccessType AccessType = EStateTreePropertyAccessType::Offset;

	friend FStateTreePropertyPath;
};
	

/**
 * Representation of a property path used for property binding in StateTree.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreePropertyPath
{
	GENERATED_BODY()

	FStateTreePropertyPath() = default;

#if WITH_EDITORONLY_DATA
	explicit FStateTreePropertyPath(const FGuid InStructID)
		: StructID(InStructID)
	{
	}

	explicit FStateTreePropertyPath(const FGuid InStructID, const FName PropertyName)
		: StructID(InStructID)
	{
		Segments.Emplace(PropertyName);
	}
	
	explicit FStateTreePropertyPath(const FGuid InStructID, TConstArrayView<FStateTreePropertyPathSegment> InSegments)
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
	bool ResolveIndirections(const UStruct* BaseStruct, TArray<FStateTreePropertyPathIndirection>& OutIndirections, FString* OutError = nullptr, bool bHandleRedirects = false) const;
	
	/**
	 * Resolves the property path against base value. The path is assumed to be relative to the BaseValueView.
	 * @param BaseValueView Base value the path is relative to.
	 * @param OutIndirections Indirections describing how the properties were accessed. 
	 * @param OutError Optional, pointer to string where error will be logged if update fails.
	 * @param bHandleRedirects If true, the method will try to resolve missing properties using core redirects, and properties on Blueprint and User Defined Structs by ID. Available only in editor builds!
	 * @return true of path could be resolved against the base value, and instance types were updated.
	 */
	bool ResolveIndirectionsWithValue(const FStateTreeDataView BaseValueView, TArray<FStateTreePropertyPathIndirection>& OutIndirections, FString* OutError = nullptr, bool bHandleRedirects = false) const;

	/**
	 * Updates property segments from base struct type. The path is expected to be relative to the BaseStruct.
	 * The method handles renamed properties (core redirect, Blueprint and User Defined Structs by ID).
	 * @param BaseValueView Base value the path is relative to.
	 * @param OutError Optional, pointer to string where error will be logged if update fails.
	 * @return true of path could be resolved against the base value, and instance types were updated.
	 */
	bool UpdateSegments(const UStruct* BaseStruct, FString* OutError = nullptr);

	/**
	 * Updates property segments from base value. The path is expected to be relative to the base value.
	 * The method updates instance types, and handles renamed properties (core redirect, Blueprint and User Defined Structs by ID).
	 * By storing the instance types on the path, we can resolve the path without the base value later.
	 * @param BaseValueView Base value the path is relative to.
	 * @param OutError Optional, pointer to string where error will be logged if update fails.
	 * @return true of path could be resolved against the base value, and instance types were updated.
	 */
	bool UpdateSegmentsFromValue(const FStateTreeDataView BaseValueView, FString* OutError = nullptr);

	UE_DEPRECATED(5.3, "Use UpdateSegmentsFromValue instead")
	bool UpdateInstanceStructsFromValue(const FStateTreeDataView BaseValueView, FString* OutError = nullptr)
	{
		return UpdateSegmentsFromValue(BaseValueView, OutError);
	}
	
	/** @return true if the path is empty. In that case the path points to the struct. */
	bool IsPathEmpty() const { return Segments.IsEmpty(); }

	/** @return true if any of the path segments is and indirection via instanced struct or object. */
	bool HasAnyInstancedIndirection() const
	{
		return Segments.ContainsByPredicate([](const FStateTreePropertyPathSegment& Segment)
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
	const FGuid& GetStructID() const { return StructID; }
	void SetStructID(const FGuid NewStructID) { StructID = NewStructID; }
#endif // WITH_EDITORONLY_DATA

	TConstArrayView<FStateTreePropertyPathSegment> GetSegments() const { return Segments; }
	TArrayView<FStateTreePropertyPathSegment> GetMutableSegments() { return Segments; }
	int32 NumSegments() const { return Segments.Num(); }
	const FStateTreePropertyPathSegment& GetSegment(const int32 Index) const { return Segments[Index]; }

	/** Adds a path segment to the path. */
	void AddPathSegment(const FName InName, const int32 InArrayIndex = INDEX_NONE, const UStruct* InInstanceType = nullptr)
	{
		Segments.Emplace(InName, InArrayIndex, InInstanceType);
	}

	/** Adds a path segment to the path. */
	void AddPathSegment(const FStateTreePropertyPathSegment& PathSegment)
	{
		Segments.Add(PathSegment);
	}

	/** Test if paths are equal. */
	bool operator==(const FStateTreePropertyPath& RHS) const;

private:
#if WITH_EDITORONLY_DATA
	/** ID of the struct this property path is relative to. */
	UPROPERTY()
	FGuid StructID;
#endif // WITH_EDITORONLY_DATA

	/** Path segments pointing to a specific property on the path. */
	UPROPERTY()
	TArray<FStateTreePropertyPathSegment> Segments;
};


USTRUCT()
struct UE_DEPRECATED(5.3, "Use FStateTreePropertyPath instead.") STATETREEMODULE_API FStateTreeEditorPropertyPath
{
	GENERATED_BODY()

	FStateTreeEditorPropertyPath() = default;
	FStateTreeEditorPropertyPath(const FGuid& InStructID, const TCHAR* PropertyName) : StructID(InStructID) { Path.Add(PropertyName); }
	
	/**
	 * Returns the property path as a one string. Highlight allows to decorate a specific segment.
	 * @param HighlightedSegment Index of the highlighted path segment
	 * @param HighlightPrefix String to append before highlighted segment
	 * @param HighlightPostfix String to append after highlighted segment
	 */
	FString ToString(const int32 HighlightedSegment = INDEX_NONE, const TCHAR* HighlightPrefix = nullptr, const TCHAR* HighlightPostfix = nullptr) const;

	/** Handle of the struct this property path is relative to. */
	UPROPERTY()
	FGuid StructID;

	/** Property path segments */
	UPROPERTY()
	TArray<FString> Path;

	bool IsValid() const { return StructID.IsValid(); }

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	bool operator==(const FStateTreeEditorPropertyPath& RHS) const;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
};


/**
 * Representation of a property binding in StateTree
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreePropertyPathBinding
{
	GENERATED_BODY()

PRAGMA_DISABLE_DEPRECATION_WARNINGS	
	FStateTreePropertyPathBinding() = default;
	
	FStateTreePropertyPathBinding(const FStateTreePropertyPath& InSourcePath, const FStateTreePropertyPath& InTargetPath)
		: SourcePropertyPath(InSourcePath)
		, TargetPropertyPath(InTargetPath)
	{
	}

	FStateTreePropertyPathBinding(const FStateTreeDataHandle InSourceDataHandle, const FStateTreePropertyPath& InSourcePath, const FStateTreePropertyPath& InTargetPath)
		: SourcePropertyPath(InSourcePath)
		, TargetPropertyPath(InTargetPath)
		, SourceDataHandle(InSourceDataHandle)
	{
	}

	UE_DEPRECATED(5.4, "Use constructor with DataHandle instead.")
	FStateTreePropertyPathBinding(const FStateTreeIndex16 InCompiledSourceStructIndex, const FStateTreePropertyPath& InSourcePath, const FStateTreePropertyPath& InTargetPath)
		: SourcePropertyPath(InSourcePath)
		, TargetPropertyPath(InTargetPath)
	{
	}

	UE_DEPRECATED(5.3, "Use constructor with FStateTreePropertyPath instead.")
	FStateTreePropertyPathBinding(const FStateTreeEditorPropertyPath& InSourcePath, const FStateTreeEditorPropertyPath& InTargetPath)
	{
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	void PostSerialize(const FArchive& Ar);

	const FStateTreePropertyPath& GetSourcePath() const { return SourcePropertyPath; }
	const FStateTreePropertyPath& GetTargetPath() const { return TargetPropertyPath; }

	FStateTreePropertyPath& GetMutableSourcePath() { return SourcePropertyPath; }
	FStateTreePropertyPath& GetMutableTargetPath() { return TargetPropertyPath; }

	UE_DEPRECATED(5.4, "Use GetSourceDataHandle instead.")
	FStateTreeIndex16 GetCompiledSourceStructIndex() const { return {}; }

	void SetSourceDataHandle(const FStateTreeDataHandle NewSourceDataHandle) { SourceDataHandle = NewSourceDataHandle; }
	FStateTreeDataHandle GetSourceDataHandle() const { return SourceDataHandle; }

private:
	/** Source property path of the binding */
	UPROPERTY()
	FStateTreePropertyPath SourcePropertyPath;

	/** Target property path of the binding */
	UPROPERTY()
	FStateTreePropertyPath TargetPropertyPath;

	/** Describes how to get the source data pointer for the binding. */
	UPROPERTY()
	FStateTreeDataHandle SourceDataHandle = FStateTreeDataHandle::Invalid;

public:
#if WITH_EDITORONLY_DATA
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UPROPERTY()
	FStateTreeEditorPropertyPath SourcePath_DEPRECATED;

	UPROPERTY()
	FStateTreeEditorPropertyPath TargetPath_DEPRECATED;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITORONLY_DATA
};

template<>
struct TStructOpsTypeTraits<FStateTreePropertyPathBinding> : public TStructOpsTypeTraitsBase2<FStateTreePropertyPathBinding>
{
	enum 
	{
		WithPostSerialize = true,
	};
};

using FStateTreeEditorPropertyBinding UE_DEPRECATED(5.3, "Deprecated struct. Please use FStateTreePropertyPathBinding instead.") = FStateTreePropertyPathBinding;

/**
 * Representation of a property reference binding in StateTree.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreePropertyRefPath
{
	GENERATED_BODY()

	FStateTreePropertyRefPath() = default;

	FStateTreePropertyRefPath(FStateTreeDataHandle InSourceDataHandle, const FStateTreePropertyPath& InSourcePath)
		: SourcePropertyPath(InSourcePath)
		, SourceDataHandle(InSourceDataHandle)
	{
	}

	const FStateTreePropertyPath& GetSourcePath() const { return SourcePropertyPath; }

	FStateTreePropertyPath& GetMutableSourcePath() { return SourcePropertyPath; }

	void SetSourceDataHandle(const FStateTreeDataHandle NewSourceDataHandle) { SourceDataHandle = NewSourceDataHandle; }
	FStateTreeDataHandle GetSourceDataHandle() const { return SourceDataHandle; }

private:
	/** Source property path of the reference */
	UPROPERTY()
	FStateTreePropertyPath SourcePropertyPath;

	/** Describes how to get the source data pointer */
	UPROPERTY()
	FStateTreeDataHandle SourceDataHandle = FStateTreeDataHandle::Invalid;
};

/**
 * Deprecated. Describes a segment of a property path. Used for storage only.
 */
USTRUCT()
struct UE_DEPRECATED(5.3, "Use FStateTreePropertyPath instead.") STATETREEMODULE_API FStateTreePropertySegment
{
	GENERATED_BODY()

	/** @return true if the segment is empty. */
	bool IsEmpty() const { return Name.IsNone(); }
	
	/** Property name. */
	UPROPERTY()
	FName Name;

	/** Index in the array the property points at. */
	UPROPERTY()
	FStateTreeIndex16 ArrayIndex = FStateTreeIndex16::Invalid;

	/** Index to next segment. */
	UPROPERTY()
	FStateTreeIndex16 NextIndex = FStateTreeIndex16::Invalid;

	/** Type of access/indirection. */
	UPROPERTY()
	EStateTreePropertyAccessType Type = EStateTreePropertyAccessType::Offset;
};


/**
 * Deprecated. Describes property binding, the property from source path is copied into the property at the target path.
 */
USTRUCT()
struct UE_DEPRECATED(5.3, "Use FStateTreePropertyPath instead.") STATETREEMODULE_API FStateTreePropertyBinding
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS

	GENERATED_BODY()

	/** Source property path. */
	UPROPERTY()
	FStateTreePropertySegment SourcePath;

	/** Target property path. */
	UPROPERTY()
	FStateTreePropertySegment TargetPath;
	
	/** Index to the source struct the source path refers to, sources are stored in FStateTreePropertyBindings. */
	UPROPERTY()
	FStateTreeIndex16 SourceStructIndex = FStateTreeIndex16::Invalid;
	
	/** The type of copy to use between the properties. */
	UPROPERTY()
	EStateTreePropertyCopyType CopyType = EStateTreePropertyCopyType::None;

PRAGMA_ENABLE_DEPRECATION_WARNINGS
};


/**
 * Used internally.
 * Property indirection is a resolved property path segment, used for accessing properties in structs.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreePropertyIndirection
{
	GENERATED_BODY()

	/** Index in the array the property points at. */
	UPROPERTY()
	FStateTreeIndex16 ArrayIndex = FStateTreeIndex16::Invalid;

	/** Cached offset of the property */
	UPROPERTY()
	uint16 Offset = 0;

	/** Cached offset of the property */
	UPROPERTY()
	FStateTreeIndex16 NextIndex = FStateTreeIndex16::Invalid;

	/** Type of access/indirection. */
	UPROPERTY()
	EStateTreePropertyAccessType Type = EStateTreePropertyAccessType::Offset;

	/** Type of the struct or object instance in case the segment is pointing into an instanced data. */
	UPROPERTY()
	TObjectPtr<const UStruct> InstanceStruct = nullptr;

	/** Cached array property. */
	const FArrayProperty* ArrayProperty = nullptr;
};


/**
 * Used internally.
 * Describes property copy, the property from source is copied into the property at the target.
 * Copy target struct is described in the property copy batch.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreePropertyCopy
{
	GENERATED_BODY()

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FStateTreePropertyCopy() = default;
	FStateTreePropertyCopy(const FStateTreePropertyCopy&) = default;
	FStateTreePropertyCopy(FStateTreePropertyCopy&&) = default;
	FStateTreePropertyCopy& operator=(const FStateTreePropertyCopy&) = default;
	FStateTreePropertyCopy& operator=(FStateTreePropertyCopy&&) = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** Source property access. */
	UPROPERTY()
	FStateTreePropertyIndirection SourceIndirection;

	/** Target property access. */
	UPROPERTY()
	FStateTreePropertyIndirection TargetIndirection;

	/** Cached pointer to the leaf property of the access. */
	const FProperty* SourceLeafProperty = nullptr;

	/** Cached pointer to the leaf property of the access. */
	const FProperty* TargetLeafProperty = nullptr;

	/** Type of the source data, used for validation. */
	UPROPERTY(Transient)
	TObjectPtr<const UStruct> SourceStructType = nullptr;

	/** Cached property element size * dim. */
	UPROPERTY()
	int32 CopySize = 0;

	/** Describes how to get the source data pointer for the copy. */
	UPROPERTY()
	FStateTreeDataHandle SourceDataHandle = FStateTreeDataHandle::Invalid;

	/** Type of the copy */
	UPROPERTY()
	EStateTreePropertyCopyType Type = EStateTreePropertyCopyType::None;

	UE_DEPRECATED(5.4, "Use SourceDataHandle instead")
	UPROPERTY()
	FStateTreeIndex16 SourceStructIndex_DEPRECATED = FStateTreeIndex16::Invalid;
};

using FStateTreePropCopy UE_DEPRECATED(5.3, "Deprecated struct. Please use FStateTreePropertyCopy instead.") = FStateTreePropertyCopy;


/**
 * Describes a batch of property copies from many sources to one target struct.
 * Note: The batch is used to reference both bindings and copies (a binding turns into copy when resolved).
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreePropertyCopyBatch
{
	GENERATED_BODY()

	/** Expected target struct */
	UPROPERTY()
	FStateTreeBindableStructDesc TargetStruct;

	/** Index to first binding/copy. */
	UPROPERTY()
	uint16 BindingsBegin = 0;

	/** Index to one past the last binding/copy. */
	UPROPERTY()
	uint16 BindingsEnd = 0;
};

using FStateTreePropCopyBatch UE_DEPRECATED(5.3, "Deprecated struct. Please use FStateTreePropertyCopy instead.") = FStateTreePropertyCopyBatch;

/**
 * Describes access to referenced property.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreePropertyAccess
{
	GENERATED_BODY()

	/** Source property access. */
	UPROPERTY()
	FStateTreePropertyIndirection SourceIndirection;

	/** Cached pointer to the leaf property of the access. */
	const FProperty* SourceLeafProperty = nullptr;

	/** Type of the source data, used for validation. */
	UPROPERTY(Transient)
	TObjectPtr<const UStruct> SourceStructType = nullptr;

	/** Describes how to get the source data pointer. */
	UPROPERTY()
	FStateTreeDataHandle SourceDataHandle = FStateTreeDataHandle::Invalid;
};

/**
 * Runtime storage and execution of property bindings.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreePropertyBindings
{
	GENERATED_BODY()

	/**
	 * Clears all bindings.
	 */
	void Reset();

	/**
	 * Resolves paths to indirections.
	 * @return True if resolve succeeded.
	 */
	[[nodiscard]] bool ResolvePaths();

	/**
	 * @return True if bindings have been resolved.
	 */
	bool IsValid() const { return bBindingsResolved; }

	/**
	 * @return Number of source structs the copy expects.
	 */
	int32 GetSourceStructNum() const { return SourceStructs.Num(); }

	/**
	 * Copies a property from Source to Target based on the provided Copy.
	 * @param Copy Describes which parameter and how is copied.
	 * @param SourceStructView Pointer and type for the source containing the property to be copied.
	 * @param TargetStructView Pointer and type for the target containing the property to be copied.
	 * @return true if the property was copied successfully.
	 */
	bool CopyProperty(const FStateTreePropertyCopy& Copy, FStateTreeDataView SourceStructView, FStateTreeDataView TargetStructView) const;

	/** @return copy batch at specified index. */
	const FStateTreePropertyCopyBatch& GetBatch(const FStateTreeIndex16 TargetBatchIndex) const
	{
		check(TargetBatchIndex.IsValid());
		return CopyBatches[TargetBatchIndex.Get()];
	}

	/** @return All the property copies for a specific batch. */
	TConstArrayView<FStateTreePropertyCopy> GetBatchCopies(const FStateTreeIndex16 TargetBatchIndex) const
	{
		return GetBatchCopies(GetBatch(TargetBatchIndex));
	}

	/** @return All the property copies for a specific batch. */
	TConstArrayView<FStateTreePropertyCopy> GetBatchCopies(const FStateTreePropertyCopyBatch& Batch) const
	{
		const int32 Count = (int32)Batch.BindingsEnd - (int32)Batch.BindingsBegin;
		if (Count == 0)
		{
			return {};
		}
		return MakeArrayView(&PropertyCopies[Batch.BindingsBegin], Count);
	}

	/**
	 * @return Referenced property access for provided PropertyRef.
	 */
	const FStateTreePropertyAccess* GetPropertyAccess(const FStateTreePropertyRef& Reference) const;

	/**
	 * Pointer to referenced property 
	 * @param SourceView Data view to referenced property's owner.
	 * @param PropertyAccess Access to the property for which we want to obtain a pointer.
	 * @return Pointer to referenced property if it's type match, nullptr otherwise.
	 */
	template< class T >
	T* GetMutablePropertyPtr(FStateTreeDataView SourceView, const FStateTreePropertyAccess& PropertyAccess) const
	{
		check(SourceView.GetStruct() == PropertyAccess.SourceStructType);

		if (!UE::StateTree::PropertyRefHelpers::Validator<T>::IsValid(*PropertyAccess.SourceLeafProperty))
		{
			return nullptr;
		}

		return reinterpret_cast<T*>(GetAddress(SourceView, PropertyAccess.SourceIndirection, PropertyAccess.SourceLeafProperty));
	}

	/**
	 * Resets copied properties in TargetStructView. Can be used e.g. to erase UObject references.
	 * @param TargetBatchIndex Batch index to copy (see FStateTreePropertyBindingCompiler).
	 * @param TargetStructView View to struct where properties are copied to.
	 * @return true if all resets succeeded (a reset can fail e.g. if source or destination struct view is invalid).
	 */
	bool ResetObjects(const FStateTreeIndex16 TargetBatchIndex, FStateTreeDataView TargetStructView) const;

	/**
	 * @return true if any of the elements in the property bindings contains any of the structs in the set.
	 */
	bool ContainsAnyStruct(const TSet<const UStruct*>& Structs);
	
	void DebugPrintInternalLayout(FString& OutString) const;

	/** @return how properties are compatible for copying. */
	static EStateTreePropertyAccessCompatibility GetPropertyCompatibility(const FProperty* FromProperty, const FProperty* ToProperty);

	/**
	 * Resolves what kind of copy type to use between specified property indirections.
	 * @param SourceIndirection Property path indirections of the copy source,
	 * @param TargetIndirection Property path indirections of the copy target.
	 * @param OutCopy Resulting copy type.
	 * @return true if copy was resolved, or false if no copy could be resolved between paths.
	 */
	[[nodiscard]] static bool ResolveCopyType(const FStateTreePropertyPathIndirection& SourceIndirection, const FStateTreePropertyPathIndirection& TargetIndirection, FStateTreePropertyCopy& OutCopy);

	
	UE_DEPRECATED(5.3, "Should not be used, will be removed in a future version.")
	TArrayView<FStateTreeBindableStructDesc> GetSourceStructs() { return  SourceStructs; };

	UE_DEPRECATED(5.3, "Use GetBatch() instead.")
	TArrayView<FStateTreePropertyCopyBatch> GetCopyBatches() { return CopyBatches; };

	UE_DEPRECATED(5.4, "Use GetBatchCopies() and Copy() instead.")
	bool CopyTo(TConstArrayView<FStateTreeDataView> SourceStructViews, const FStateTreeIndex16 TargetBatchIndex, FStateTreeDataView TargetStructView) const
	{
		return false;
	}

private:
	[[nodiscard]] static bool ResolvePath(const UStruct* Struct, const FStateTreePropertyPath& Path, TArray<FStateTreePropertyIndirection>& OutIndirections, FStateTreePropertyIndirection& OutFirstIndirection, FStateTreePropertyPathIndirection& OutLeafIndirection);
	static uint8* GetAddress(FStateTreeDataView InStructView, TConstArrayView<FStateTreePropertyIndirection> Indirections, const FStateTreePropertyIndirection& FirstIndirection, const FProperty* LeafProperty);
	
	uint8* GetAddress(FStateTreeDataView InStructView, const FStateTreePropertyIndirection& FirstIndirection, const FProperty* LeafProperty) const
	{
		return GetAddress(InStructView, PropertyIndirections, FirstIndirection, LeafProperty);
	}

	[[nodiscard]] bool ResolvePath(const UStruct* Struct, const FStateTreePropertyPath& Path, FStateTreePropertyIndirection& OutFirstIndirection, FStateTreePropertyPathIndirection& OutLeafIndirection)
	{
		return ResolvePath(Struct, Path, PropertyIndirections, OutFirstIndirection, OutLeafIndirection);
	}

	const FStateTreeBindableStructDesc* GetSourceDescByHandle(const FStateTreeDataHandle SourceDataHandle);

	void PerformCopy(const FStateTreePropertyCopy& Copy, uint8* SourceAddress, uint8* TargetAddress) const;
	void PerformResetObjects(const FStateTreePropertyCopy& Copy, uint8* TargetAddress) const;

	/** Array of expected source structs. */
	UPROPERTY()
	TArray<FStateTreeBindableStructDesc> SourceStructs;

	/** Array of copy batches. */
	UPROPERTY()
	TArray<FStateTreePropertyCopyBatch> CopyBatches;

	/** Array of property bindings, resolved into arrays of copies before use. */
	UPROPERTY()
	TArray<FStateTreePropertyPathBinding> PropertyPathBindings;

	/** Array of property copies */
	UPROPERTY(Transient)
	TArray<FStateTreePropertyCopy> PropertyCopies;

	/** Array of referenced property paths */
	UPROPERTY()
	TArray<FStateTreePropertyRefPath> PropertyReferencePaths;

	/** Array of individually accessed properties */
	UPROPERTY()
	TArray<FStateTreePropertyAccess> PropertyAccesses;

	/** Array of property indirections, indexed by accesses*/
	UPROPERTY(Transient)
	TArray<FStateTreePropertyIndirection> PropertyIndirections;

	/** Flag indicating if the properties has been resolved successfully . */
	bool bBindingsResolved = false;

#if WITH_EDITORONLY_DATA
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	
	UE_DEPRECATED_FORGAME(5.3, "Use PropertyPathBindings instead.")
	UPROPERTY()
	TArray<FStateTreePropertyBinding> PropertyBindings_DEPRECATED;

	UE_DEPRECATED_FORGAME(5.3, "Use PropertyPathBindings instead.")
	UPROPERTY()
	TArray<FStateTreePropertySegment> PropertySegments_DEPRECATED;
	
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITORONLY_DATA

	friend FStateTreePropertyBindingCompiler;
	friend UStateTree;
};

/**
 * Helper interface to reason about bound properties. The implementation is in the editor plugin.
 */
struct STATETREEMODULE_API IStateTreeBindingLookup
{
	/** @return Source path for given target path, or null if binding does not exists. */
	virtual const FStateTreePropertyPath* GetPropertyBindingSource(const FStateTreePropertyPath& InTargetPath) const PURE_VIRTUAL(IStateTreeBindingLookup::GetPropertyBindingSource, return nullptr; );

	/** @return Display name given property path. */
	virtual FText GetPropertyPathDisplayName(const FStateTreePropertyPath& InPath) const PURE_VIRTUAL(IStateTreeBindingLookup::GetPropertyPathDisplayName, return FText::GetEmpty(); );

	/** @return Leaf property based on property path. */
	virtual const FProperty* GetPropertyPathLeafProperty(const FStateTreePropertyPath& InPath) const PURE_VIRTUAL(IStateTreeBindingLookup::GetPropertyPathLeafProperty, return nullptr; );

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.3, "Use version with FStateTreePropertyPath instead.")
	virtual const FStateTreeEditorPropertyPath* GetPropertyBindingSource(const FStateTreeEditorPropertyPath& InTargetPath) const final { return nullptr; }
	
	UE_DEPRECATED(5.3, "Use version with FStateTreePropertyPath instead.")
	virtual FText GetPropertyPathDisplayName(const FStateTreeEditorPropertyPath& InPath) const final { return FText::GetEmpty(); }
	
	UE_DEPRECATED(5.3, "Use version with FStateTreePropertyPath instead.")
	virtual const FProperty* GetPropertyPathLeafProperty(const FStateTreeEditorPropertyPath& InPath) const final { return nullptr; }
PRAGMA_ENABLE_DEPRECATION_WARNINGS
};


namespace UE::StateTree
{
	/** @return desc and path as a display string. */
	extern STATETREEMODULE_API FString GetDescAndPathAsString(const FStateTreeBindableStructDesc& Desc, const FStateTreePropertyPath& Path);

#if WITH_EDITOR
	/**
	 * Returns property usage based on the Category metadata of given property.
	 * @param Property Handle to property where value is got from.
	 * @return found usage type, or EStateTreePropertyUsage::Invalid if not found.
	 */
	STATETREEMODULE_API EStateTreePropertyUsage GetUsageFromMetaData(const FProperty* Property);
#endif
} // UE::StateTree

namespace UE::StateTree::Private
{
#if WITH_EDITORONLY_DATA
// Helper functions to convert between property path types.
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.3, "Will be removed when FStateTreeEditorPropertyPath is removed.")
	extern STATETREEMODULE_API FStateTreePropertyPath ConvertEditorPath(const FStateTreeEditorPropertyPath& InEditorPath);
	UE_DEPRECATED(5.3, "Will be removed when FStateTreeEditorPropertyPath is removed.")
	extern STATETREEMODULE_API FStateTreeEditorPropertyPath ConvertEditorPath(const FStateTreePropertyPath& InPath);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITORONLY_DATA
} // UE::StateTree::Private 

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
