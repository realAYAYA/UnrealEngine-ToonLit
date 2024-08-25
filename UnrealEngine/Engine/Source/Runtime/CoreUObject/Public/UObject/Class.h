// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Class.h: UClass definition.
=============================================================================*/

#pragma once

#include "Concepts/GetTypeHashable.h"
#include "Concepts/StaticStructProvider.h"
#include "Containers/Array.h"
#include "Containers/EnumAsByte.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/StringFwd.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Async/Mutex.h"
#include "Async/UniqueLock.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformCrt.h"
#include "HAL/UnrealMemory.h"
#include "Internationalization/Text.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Logging/LogVerbosity.h"
#include "Math/Box2D.h"
#include "Math/InterpCurvePoint.h"
#include "Math/MathFwd.h"
#include "Math/Matrix.h"
#include "Math/Plane.h"
#include "Math/Quat.h"
#include "Math/RandomStream.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"
#include "Math/Vector2D.h"
#include "Math/Vector4.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CString.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/FallbackStruct.h"
#include "Misc/Guid.h"
#include "Misc/Optional.h"
#include "Misc/ScopeRWLock.h"
#include "Serialization/StructuredArchive.h"
#include "Serialization/StructuredArchiveAdapters.h"
#include "Templates/AlignmentTemplates.h"
#include "Templates/EnableIf.h"
#include "Templates/IsAbstract.h"
#include "Templates/IsEnum.h"
#include "Templates/IsPODType.h"
#include "Templates/IsTriviallyDestructible.h"
#include "Templates/IsUECoreType.h"
#include "Templates/Models.h"
#include "Templates/Tuple.h"
#include "Templates/TypeCompatibleBytes.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/UnrealTypeTraits.h"
#include "Trace/Detail/Channel.h"
#include "UObject/CoreNative.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
#include "UObject/GarbageCollection.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/PropertyTag.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "UObject/Script.h"
#include "UObject/TopLevelAssetPath.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealNames.h"
#include "UObject/ObjectPtr.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Misc/PackageAccessTracking.h"
#include "Misc/PackageAccessTrackingOps.h"
#include "UObject/Package.h"
#endif

class FArchive;
class FEditPropertyChain;
class FField;
class FOutputDevice;
class FProperty;
class FStructProperty;
class UPropertyWrapper;
struct CGetTypeHashable;
struct FAssetData;
struct FBlake3Hash;
struct FColor;
struct FCustomPropertyListNode;
struct FDoubleInterval;
struct FDoubleRange;
struct FDoubleRangeBound;
struct FFallbackStruct;
struct FFloatInterval;
struct FFloatRange;
struct FFloatRangeBound;
struct FFrame;
struct FInt32Interval;
struct FInt32Range;
struct FInt32RangeBound;
struct FLinearColor;
struct FNetDeltaSerializeInfo;
struct FObjectInstancingGraph;
struct FPropertyTag;
struct FRandomStream;
struct FUObjectSerializeContext;
template <typename FuncType> class TFunctionRef;
enum class EPropertyObjectReferenceType : uint32;

COREUOBJECT_API DECLARE_LOG_CATEGORY_EXTERN(LogClass, Log, All);
COREUOBJECT_API DECLARE_LOG_CATEGORY_EXTERN(LogScriptSerialization, Log, All);

#ifndef USE_UE_LOCK_FOR_UCLASS_FUNCTION_HANDLING
	#define USE_UE_LOCK_FOR_UCLASS_FUNCTION_HANDLING 0
#endif

#if USE_UE_LOCK_FOR_UCLASS_FUNCTION_HANDLING
	typedef UE::TUniqueLock<UE::FMutex> FUClassFuncScopeReadLock;
	typedef UE::TUniqueLock<UE::FMutex> FUClassFuncScopeWriteLock;
	typedef UE::FMutex FUClassFuncLock;
#else
	typedef FReadScopeLock FUClassFuncScopeReadLock;
	typedef FWriteScopeLock FUClassFuncScopeWriteLock;
	typedef FRWLock FUClassFuncLock;
#endif


/*-----------------------------------------------------------------------------
	FRepRecord.
-----------------------------------------------------------------------------*/

//
// Information about a property to replicate.
//
struct FRepRecord
{
	FProperty* Property;
	int32 Index;
	FRepRecord(FProperty* InProperty,int32 InIndex)
	: Property(InProperty), Index(InIndex)
	{}
};

/*-----------------------------------------------------------------------------
	UField.
-----------------------------------------------------------------------------*/

//
// Base class of reflection data objects.
//
class UField : public UObject
{
	DECLARE_CASTED_CLASS_INTRINSIC_WITH_API(UField, UObject, CLASS_Abstract, TEXT("/Script/CoreUObject"), CASTCLASS_UField, COREUOBJECT_API)

	typedef UField BaseFieldClass;
	typedef UClass FieldTypeClass;

	/** Next Field in the linked list */
	UField*			Next;

	// Constructors.
	UField(EStaticConstructor, EObjectFlags InFlags);

	// UObject interface.
	COREUOBJECT_API virtual void Serialize( FArchive& Ar ) override;
	COREUOBJECT_API virtual void PostLoad() override;
	COREUOBJECT_API virtual bool NeedsLoadForClient() const override;
	COREUOBJECT_API virtual bool NeedsLoadForServer() const override;

	// UField interface.
	COREUOBJECT_API virtual void AddCppProperty(FProperty* Property);
	COREUOBJECT_API virtual void Bind();

	/** Goes up the outer chain to look for a UClass */
	COREUOBJECT_API UClass* GetOwnerClass() const;

	/** Goes up the outer chain to look for a UStruct */
	COREUOBJECT_API UStruct* GetOwnerStruct() const;

	/** 
	 * Returns a human readable string that was assigned to this field at creation. 
	 * By default this is the same as GetName() but it can be overridden if that is an internal-only name.
	 * This name is consistent in editor/cooked builds, is not localized, and is useful for data import/export.
	 */
	COREUOBJECT_API FString GetAuthoredName() const;

#if WITH_EDITORONLY_DATA
	/**
	 * Finds the localized display name or native display name as a fallback.
	 *
	 * @return The display name for this object.
	 */
	COREUOBJECT_API FText GetDisplayNameText() const;

	/**
	 * Finds the localized tooltip or native tooltip as a fallback.
	 *
	 * @param bShortTooltip Look for a shorter version of the tooltip (falls back to the long tooltip if none was specified)
	 *
	 * @return The tooltip for this object.
	 */
	COREUOBJECT_API FText GetToolTipText(bool bShortTooltip = false) const;

	/** 
	 * Formats a source comment into the form we want to show in the editor, is used by GetToolTipText and anything else that will get a native tooltip 
	 * 
	 * @param ToolTipString			String parsed out of C++ headers that is modified in place
	 * @param bRemoveExtraSections	If true, cut off the comment on first line separator or 2 empty lines in a row
	 */
	static COREUOBJECT_API void FormatNativeToolTip(FString& ToolTipString, bool bRemoveExtraSections = true);

	/**
	 * Determines if the property has any metadata associated with the key
	 * 
	 * @param Key The key to lookup in the metadata
	 * @return true if there is a (possibly blank) value associated with this key
	 */
	bool HasMetaData(const TCHAR* Key) const { return FindMetaData(Key) != nullptr; }
	bool HasMetaData(const FName& Key) const { return FindMetaData(Key) != nullptr; }

	/**
	 * Find the metadata value associated with the key
	 *
	 * @param Key The key to lookup in the metadata
	 * @return The value associated with the key if exists, null otherwise
	 */
	COREUOBJECT_API const FString* FindMetaData(const TCHAR* Key) const;
	COREUOBJECT_API const FString* FindMetaData(const FName& Key) const;

	/**
	 * Find the metadata value associated with the key
	 * 
	 * @param Key The key to lookup in the metadata
	 * @return The value associated with the key
	 */
	COREUOBJECT_API const FString& GetMetaData(const TCHAR* Key) const;
	COREUOBJECT_API const FString& GetMetaData(const FName& Key) const;

	/**
	 * Find the metadata value associated with the key and localization namespace and key
	 *
	 * @param Key						The key to lookup in the metadata
	 * @param LocalizationNamespace		Namespace to lookup in the localization manager
	 * @param LocalizationKey			Key to lookup in the localization manager
	 * @return							Localized metadata if available, defaults to whatever is provided via GetMetaData
	 */
	COREUOBJECT_API FText GetMetaDataText(const TCHAR* MetaDataKey, const FString LocalizationNamespace = FString(), const FString LocalizationKey = FString()) const;
	COREUOBJECT_API FText GetMetaDataText(const FName& MetaDataKey, const FString LocalizationNamespace = FString(), const FString LocalizationKey = FString()) const;

	/**
	 * Sets the metadata value associated with the key
	 * 
	 * @param Key The key to lookup in the metadata
	 * @return The value associated with the key
	 */
	COREUOBJECT_API void SetMetaData(const TCHAR* Key, const TCHAR* InValue);
	COREUOBJECT_API void SetMetaData(const FName& Key, const TCHAR* InValue);

	/**
	 * Find the metadata value associated with the key
	 * and return bool 
	 * @param Key The key to lookup in the metadata
	 * @return return true if the value was true (case insensitive)
	 */
	bool GetBoolMetaData(const TCHAR* Key) const
	{		
		const FString& BoolString = GetMetaData(Key);
		// FString == operator does case insensitive comparison
		return (BoolString == "true");
	}
	bool GetBoolMetaData(const FName& Key) const
	{		
		const FString& BoolString = GetMetaData(Key);
		// FString == operator does case insensitive comparison
		return (BoolString == "true");
	}

	/**
	 * Find the metadata value associated with the key
	 * and return int32 
	 * @param Key The key to lookup in the metadata
	 * @return the int value stored in the metadata. 0 if not a valid integer.
	 */
	int32 GetIntMetaData(const TCHAR* Key) const
	{
		const FString& IntString = GetMetaData(Key);
		int32 Value = FCString::Atoi(*IntString);
		return Value;
	}
	int32 GetIntMetaData(const FName& Key) const
	{
		const FString& IntString = GetMetaData(Key);
		int32 Value = FCString::Atoi(*IntString);
		return Value;
	}

	/**
	 * Find the metadata value associated with the key
	 * and return float
	 * @param Key The key to lookup in the metadata
	 * @return the float value stored in the metadata. 0 if not a valid float.
	 */
	float GetFloatMetaData(const TCHAR* Key) const
	{
		const FString& FloatString = GetMetaData(Key);
		float Value = FCString::Atof(*FloatString);
		return Value;
	}
	float GetFloatMetaData(const FName& Key) const
	{
		const FString& FloatString = GetMetaData(Key);
		float Value = FCString::Atof(*FloatString);
		return Value;
	}
	
	/**
	 * Find the metadata value associated with the key
	 * and return Class
	 * @param Key The key to lookup in the metadata
	 * @return the class value stored in the metadata.
	 */
	COREUOBJECT_API UClass* GetClassMetaData(const TCHAR* Key) const;
	COREUOBJECT_API UClass* GetClassMetaData(const FName& Key) const;

	/** Clear any metadata associated with the key */
	COREUOBJECT_API void RemoveMetaData(const TCHAR* Key);
	COREUOBJECT_API void RemoveMetaData(const FName& Key);
#endif // WITH_EDITORONLY_DATA

	COREUOBJECT_API bool HasAnyCastFlags(const uint64 InCastFlags) const;
	COREUOBJECT_API bool HasAllCastFlags(const uint64 InCastFlags) const;

#if WITH_EDITORONLY_DATA
	/**
	 * Gets the FField object associated with this Field
	 */
	COREUOBJECT_API virtual FField* GetAssociatedFField();
	/**
	 * Sets the FField object associated with this Field
	 */
	COREUOBJECT_API virtual void SetAssociatedFField(FField* InField);
#endif // WITH_EDITORONLY_DATA
};

#if USTRUCT_FAST_ISCHILDOF_IMPL == USTRUCT_ISCHILDOF_STRUCTARRAY
class FStructBaseChain
{
protected:
	COREUOBJECT_API FStructBaseChain();
	COREUOBJECT_API ~FStructBaseChain();

	// Non-copyable
	FStructBaseChain(const FStructBaseChain&) = delete;
	FStructBaseChain& operator=(const FStructBaseChain&) = delete;

	COREUOBJECT_API void ReinitializeBaseChainArray();

	FORCEINLINE bool IsChildOfUsingStructArray(const FStructBaseChain& Parent) const
	{
		int32 NumParentStructBasesInChainMinusOne = Parent.NumStructBasesInChainMinusOne;
		return NumParentStructBasesInChainMinusOne <= NumStructBasesInChainMinusOne && StructBaseChainArray[NumParentStructBasesInChainMinusOne] == &Parent;
	}

private:
	FStructBaseChain** StructBaseChainArray;
	int32 NumStructBasesInChainMinusOne;

	friend class UStruct;
};
#endif

/*-----------------------------------------------------------------------------
	UStruct.
-----------------------------------------------------------------------------*/

/**
 * Base class for all UObject types that contain fields.
 */
class UStruct : public UField
#if USTRUCT_FAST_ISCHILDOF_IMPL == USTRUCT_ISCHILDOF_STRUCTARRAY
	, private FStructBaseChain
#endif
{
	DECLARE_CASTED_CLASS_INTRINSIC_WITH_API(UStruct, UField, CLASS_MatchedSerializers, TEXT("/Script/CoreUObject"), CASTCLASS_UStruct, COREUOBJECT_API)

	// Variables.
protected:
	friend struct Z_Construct_UClass_UStruct_Statics;
private:
	/** Struct this inherits from, may be null */
	ObjectPtr_Private::TNonAccessTrackedObjectPtr<UStruct> SuperStruct;
public:
	/** Pointer to start of linked list of child fields */
	TObjectPtr<UField> Children;
	
	/** Pointer to start of linked list of child fields */
	FField* ChildProperties;

	/** Total size of all UProperties, the allocated structure may be larger due to alignment */
	int32 PropertiesSize;
	/** Alignment of structure in memory, structure will be at least this large */
	int32 MinAlignment;
	
	/** Script bytecode associated with this object */
	TArray<uint8> Script;

	/** In memory only: Linked list of properties from most-derived to base */
	FProperty* PropertyLink;
	/** In memory only: Linked list of object reference properties from most-derived to base */
	FProperty* RefLink;
	/** In memory only: Linked list of properties requiring destruction. Note this does not include things that will be destroyed by the native destructor */
	FProperty* DestructorLink;
	/** In memory only: Linked list of properties requiring post constructor initialization */
	FProperty* PostConstructLink;

	/** Array of object references embedded in script code and referenced by FProperties. Mirrored for easy access by realtime garbage collection code */
	TArray<TObjectPtr<UObject>> ScriptAndPropertyObjectReferences;

	typedef TArray<TPair<TFieldPath<FField>, int32>> FUnresolvedScriptPropertiesArray;
	/** Contains a list of script properties that couldn't be resolved at load time */
	FUnresolvedScriptPropertiesArray* UnresolvedScriptProperties;

#if WITH_EDITORONLY_DATA
	/** List of wrapper UObjects for FProperties */
	TArray<TObjectPtr<UPropertyWrapper>> PropertyWrappers;
	/** Unique id incremented each time this class properties get destroyed */
	int32 FieldPathSerialNumber;
#endif

	/** Cached schema for optimized unversioned and filtereditoronly property serialization, owned by this. */
	mutable const struct FUnversionedStructSchema* UnversionedGameSchema = nullptr;
#if WITH_EDITORONLY_DATA
	/** Cached schema for optimized unversioned property serialization, with editor data, owned by this. */
	mutable const struct FUnversionedStructSchema* UnversionedEditorSchema = nullptr;

	/** Get the Schema Hash for this struct - the hash of its property names and types. */
	COREUOBJECT_API const FBlake3Hash& GetSchemaHash(bool bSkipEditorOnly) const;

protected:
	/** True if this struct has Asset Registry searchable properties */
	bool bHasAssetRegistrySearchableProperties;
#endif

public:
	// Constructors.
	COREUOBJECT_API UStruct( EStaticConstructor, int32 InSize, int32 InAlignment, EObjectFlags InFlags );
	COREUOBJECT_API explicit UStruct(UStruct* InSuperStruct, SIZE_T ParamsSize = 0, SIZE_T Alignment = 0);
	COREUOBJECT_API explicit UStruct(const FObjectInitializer& ObjectInitializer, UStruct* InSuperStruct, SIZE_T ParamsSize = 0, SIZE_T Alignment = 0 );
	COREUOBJECT_API virtual ~UStruct();

	// UObject interface.
	COREUOBJECT_API virtual void Serialize(FArchive& Ar) override;
	COREUOBJECT_API virtual void Serialize(FStructuredArchive::FRecord Record) override;
	COREUOBJECT_API virtual void PostLoad() override;
	COREUOBJECT_API virtual void FinishDestroy() override;
	COREUOBJECT_API virtual void RegisterDependencies() override;
	static COREUOBJECT_API void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	COREUOBJECT_API virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
	COREUOBJECT_API virtual void TagSubobjects(EObjectFlags NewFlags) override;

	// UField interface.
	COREUOBJECT_API virtual void AddCppProperty(FProperty* Property) override;

	/**
	 * Returns struct path name as a package + struct FName pair
	 */
	COREUOBJECT_API FTopLevelAssetPath GetStructPathName() const;

	/** Searches property link chain for a property with the specified name */
	COREUOBJECT_API FProperty* FindPropertyByName(FName InName) const;

	/**
	 * Creates new copies of components
	 * 
	 * @param	Data						pointer to the address of the subobject referenced by this FProperty
	 * @param	DefaultData					pointer to the address of the default value of the subbject referenced by this FProperty
	 * @param	DefaultStruct				the struct corresponding to the buffer pointed to by DefaultData
	 * @param	Owner						the object that contains the component currently located at Data
	 * @param	InstanceGraph				contains the mappings of instanced objects and components to their templates
	 */
	COREUOBJECT_API void InstanceSubobjectTemplates( void* Data, void const* DefaultData, UStruct* DefaultStruct, UObject* Owner, FObjectInstancingGraph* InstanceGraph );

	/** Returns the structure used for inheritance, may be changed by child types */
	virtual UStruct* GetInheritanceSuper() const {return GetSuperStruct();}

	/** Static wrapper for Link, using a dummy archive */
	COREUOBJECT_API void StaticLink(bool bRelinkExistingProperties = false);

	/** Creates the field/property links and gets structure ready for use at runtime */
	COREUOBJECT_API virtual void Link(FArchive& Ar, bool bRelinkExistingProperties);

	/**
	 * Serializes struct properties, does not handle defaults.  See SerializeBinEx for handling defaults.
	 *
	 * @param	Ar				the archive to use for serialization
	 * @param	Data			pointer to the location of the beginning of the property data
	 *
	 * @note Binary serialization will read and write unstructured data from the archive.  As deprecated
	 *       properties are read from archives but not written, it is dangerous to call this function on
	 *       types with deprecated properties, unless the ArWantBinarySerialization flag is set on the
	 *       archive to force serialization to occur always.
	 */
	virtual void SerializeBin(FArchive& Ar, void* Data) const final 
	{
		SerializeBin(FStructuredArchiveFromArchive(Ar).GetSlot(), Data);
	}

	/**
	 * Serializes struct properties, does not handle defaults.  See SerializeBinEx for handling defaults.
	 *
	 * @param	Slot			The structured archive slot we are serializing to
	 * @param	Data			pointer to the location of the beginning of the property data
	 *
	 * @note Binary serialization will read and write unstructured data from the archive.  As deprecated
	 *       properties are read from archives but not written, it is dangerous to call this function on
	 *       types with deprecated properties, unless the ArWantBinarySerialization flag is set on the
	 *       archive to force serialization to occur always.
	 */
	COREUOBJECT_API virtual void SerializeBin(FStructuredArchive::FSlot Slot, void* Data) const;

	/**
	 * Serializes the class properties that reside in Data if they differ from the corresponding values in DefaultData
	 *
	 * @param	Slot			The structured archive slot we are serializing to
	 * @param	Data			pointer to the location of the beginning of the property data
	 * @param	DefaultData		pointer to the location of the beginning of the data that should be compared against
	 * @param	DefaultStruct	the struct corresponding to the block of memory located at DefaultData 
	 *
	 * @note Binary serialization will read and write unstructured data from the archive.  As deprecated
	 *       properties are read from archives but not written, it is dangerous to call this function on
	 *       types with deprecated properties, unless the ArWantBinarySerialization flag is set on the
	 *       archive to force serialization to occur always.
	 */
	COREUOBJECT_API void SerializeBinEx( FStructuredArchive::FSlot Slot, void* Data, void const* DefaultData, UStruct* DefaultStruct ) const;

	/** Serializes list of properties, using property tags to handle mismatches */
	virtual void SerializeTaggedProperties(FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, uint8* Defaults, const UObject* BreakRecursionIfFullyLoad = nullptr) const final 
	{
		SerializeTaggedProperties(FStructuredArchiveFromArchive(Ar).GetSlot(), Data, DefaultsStruct, Defaults, BreakRecursionIfFullyLoad);
	}

	/** Serializes list of properties, using property tags to handle mismatches */
	COREUOBJECT_API virtual void SerializeTaggedProperties(FStructuredArchive::FSlot Slot, uint8* Data, UStruct* DefaultsStruct, uint8* Defaults, const UObject* BreakRecursionIfFullyLoad = nullptr) const;

	/**
	 * Preloads all fields that belong to this struct
	 * @param Ar Archive used for loading this struct
	 */
	COREUOBJECT_API virtual void PreloadChildren(FArchive& Ar);

	/**
	 * Initialize a struct over uninitialized memory. This may be done by calling the native constructor or individually initializing properties
	 *
	 * @param	Dest		Pointer to memory to initialize
	 * @param	ArrayDim	Number of elements in the array
	 * @param	Stride		Stride of the array, If this default (0), then we will pull the size from the struct
	 */
	COREUOBJECT_API virtual void InitializeStruct(void* Dest, int32 ArrayDim = 1) const;
	
	/**
	 * Destroy a struct in memory. This may be done by calling the native destructor or individually destroying properties
	 *
	 * @param	Dest		Pointer to memory to destory
	 * @param	ArrayDim	Number of elements in the array
	 * @param	Stride		Stride of the array. If this default (0), then we will pull the size from the struct
	 */
	COREUOBJECT_API virtual void DestroyStruct(void* Dest, int32 ArrayDim = 1) const;

	/** Look up a property by an alternate name if it was not found in the first search, this is overridden for user structs */
	virtual FProperty* CustomFindProperty(const FName InName) const { return nullptr; };

	/** Serialize an expression to an archive. Returns expression token */
	COREUOBJECT_API virtual EExprToken SerializeExpr(int32& iCode, FArchive& Ar);

	/**
	 * Returns the struct/ class prefix used for the C++ declaration of this struct/ class.
	 *
	 * @return Prefix character used for C++ declaration of this struct/ class.
	 */
	virtual const TCHAR* GetPrefixCPP() const { return TEXT("F"); }

	/** Total size of all UProperties, the allocated structure may be larger due to alignment */
	FORCEINLINE int32 GetPropertiesSize() const
	{
		return PropertiesSize;
	}

	/** Alignment of structure in memory, structure will be at least this large */
	FORCEINLINE int32 GetMinAlignment() const
	{
		return MinAlignment;
	}

	/** Returns actual allocated size of structure in memory */
	FORCEINLINE int32 GetStructureSize() const
	{
		return Align(PropertiesSize,MinAlignment);
	}

	/** Modifies the property size after it's been recomputed */
	void SetPropertiesSize( int32 NewSize )
	{
		PropertiesSize = NewSize;
	}

	/** Returns true if this struct either is class T, or is a child of class T. This will not crash on null structs */
	template<class T>
	bool IsChildOf() const
	{
		return IsChildOf(T::StaticClass());
	}

	/** Returns true if this struct either is SomeBase, or is a child of SomeBase. This will not crash on null structs */
#if USTRUCT_FAST_ISCHILDOF_COMPARE_WITH_OUTERWALK || USTRUCT_FAST_ISCHILDOF_IMPL == USTRUCT_ISCHILDOF_OUTERWALK
	COREUOBJECT_API bool IsChildOf( const UStruct* SomeBase ) const;
#else
	bool IsChildOf(const UStruct* SomeBase) const
	{
		return (SomeBase ? IsChildOfUsingStructArray(*SomeBase) : false);
	}
#endif

	/** Struct this inherits from, may be null */
	UStruct* GetSuperStruct() const
	{
		return SuperStruct;
	}

	/**
	 * Sets the super struct pointer and updates hash information as necessary.
	 * Note that this is not sufficient to actually reparent a struct, it simply sets a pointer.
	 */
	COREUOBJECT_API virtual void SetSuperStruct(UStruct* NewSuperStruct);

	/** Returns a human readable string for a given field, overridden for user defined structs */
	COREUOBJECT_API virtual FString GetAuthoredNameForField(const UField* Field) const;

	/** Returns a human readable string for a given field, overridden for user defined structs */
	COREUOBJECT_API virtual FString GetAuthoredNameForField(const FField* Field) const;

	/** If true, this class has been cleaned and sanitized (trashed) and should not be used */
	virtual bool IsStructTrashed() const
	{
		return false;
	}

	/** Destroys all properties owned by this struct */
	COREUOBJECT_API void DestroyChildPropertiesAndResetPropertyLinks();

#if WITH_EDITORONLY_DATA
	/** Try and find boolean metadata with the given key. If not found on this class, work up hierarchy looking for it. */
	COREUOBJECT_API bool GetBoolMetaDataHierarchical(const FName& Key) const;

	/** Try and find string metadata with the given key. If not found on this class, work up hierarchy looking for it. */
	COREUOBJECT_API bool GetStringMetaDataHierarchical(const FName& Key, FString* OutValue = nullptr) const;

	/**
	 * Determines if the struct or any of its super structs has any metadata associated with the provided key
	 *
	 * @param Key The key to lookup in the metadata
	 * @return pointer to the UStruct that has associated metadata, nullptr if Key is not associated with any UStruct in the hierarchy
	 */
	COREUOBJECT_API const UStruct* HasMetaDataHierarchical(const FName& Key) const;

	/* Returns true if this struct has Asset Registry searchable properties */
	FORCEINLINE bool HasAssetRegistrySearchableProperties() const
	{
		return bHasAssetRegistrySearchableProperties;
	}
#endif // WITH_EDITORONLY_DATA

	/** Sets the UnresolvedScriptProperties array */
	void SetUnresolvedScriptProperties(FUnresolvedScriptPropertiesArray& InUnresolvedProperties)
	{
		if (!UnresolvedScriptProperties)
		{
			UnresolvedScriptProperties = new FUnresolvedScriptPropertiesArray();
		}
		*UnresolvedScriptProperties = MoveTemp(InUnresolvedProperties);
	}

	/** Deletes the UnresolvedScriptProperties array */
	FORCEINLINE void DeleteUnresolvedScriptProperties()
	{
		if (UnresolvedScriptProperties)
		{
			delete UnresolvedScriptProperties;
			UnresolvedScriptProperties = nullptr;
		}
	}

	/**
	 * Collects UObjects referenced by bytecode
	 * @param OutReferencedObjects buffer to store the referenced objects in (not cleared by this function)
	 */
	COREUOBJECT_API void CollectBytecodeReferencedObjects(TArray<UObject*>& OutReferencedObjects);
	/**
	 * Collects UObjects referenced by properties
	 * @param OutReferencedObjects buffer to store the referenced objects in (not cleared by this function)
	 */
	COREUOBJECT_API void CollectPropertyReferencedObjects(TArray<UObject*>& OutReferencedObjects);
	/**
	 * Collects UObjects referenced by bytecode and properties for faster GC access
	 */
	COREUOBJECT_API void CollectBytecodeAndPropertyReferencedObjects();
	/**
	 * Collects UObjects referenced by bytecode and properties for this class and its child fields and their children...
	 */
	COREUOBJECT_API void CollectBytecodeAndPropertyReferencedObjectsRecursively();

protected:

	/** Returns the property name from the guid */
	virtual FName FindPropertyNameFromGuid(const FGuid& PropertyGuid) const { return NAME_None; }

	/** Find property guid */
	virtual FGuid FindPropertyGuidFromName(const FName InName) const { return FGuid(); }

	/** Returns if we have access to property guids */
	virtual bool ArePropertyGuidsAvailable() const { return false; }

	/** Serializes properties of this struct */
	COREUOBJECT_API void SerializeProperties(FArchive& Ar);

#if WITH_EDITORONLY_DATA
	COREUOBJECT_API void ConvertUFieldsToFFields();
#endif // WITH_EDITORONLY_DATA

	/** Serializes list of properties to a te, using property tags to handle mismatches */
	COREUOBJECT_API void LoadTaggedPropertiesFromText(FStructuredArchive::FSlot Slot, uint8* Data, UStruct* DefaultsStruct, uint8* Defaults, const UObject* BreakRecursionIfFullyLoad) const;

private:
#if USTRUCT_FAST_ISCHILDOF_IMPL == USTRUCT_ISCHILDOF_STRUCTARRAY
	// For UObjectBaseUtility
	friend class UObjectBaseUtility;
	using FStructBaseChain::IsChildOfUsingStructArray;
	using FStructBaseChain::ReinitializeBaseChainArray;

	friend class FStructBaseChain;
	friend class FBlueprintCompileReinstancer;
#endif

	COREUOBJECT_API void SerializeVersionedTaggedProperties(FStructuredArchive::FSlot Slot, uint8* Data, UStruct* DefaultsStruct, uint8* Defaults, const UObject* BreakRecursionIfFullyLoad) const;
};

/**
 * Flags describing a struct
 * 
 * This MUST be kept in sync with EStructFlags defined in
 * Engine\Source\Programs\Shared\EpicGames.Core\UnrealEngineTypes.cs
 */
enum EStructFlags
{
	// State flags.
	STRUCT_NoFlags				= 0x00000000,	
	STRUCT_Native				= 0x00000001,

	/** If set, this struct will be compared using native code */
	STRUCT_IdenticalNative		= 0x00000002,
	
	STRUCT_HasInstancedReference= 0x00000004,

	STRUCT_NoExport				= 0x00000008,

	/** Indicates that this struct should always be serialized as a single unit */
	STRUCT_Atomic				= 0x00000010,

	/** Indicates that this struct uses binary serialization; it is unsafe to add/remove members from this struct without incrementing the package version */
	STRUCT_Immutable			= 0x00000020,

	/** If set, native code needs to be run to find referenced objects */
	STRUCT_AddStructReferencedObjects = 0x00000040,

	/** Indicates that this struct should be exportable/importable at the DLL layer.  Base structs must also be exportable for this to work. */
	STRUCT_RequiredAPI			= 0x00000200,	

	/** If set, this struct will be serialized using the CPP net serializer */
	STRUCT_NetSerializeNative	= 0x00000400,	

	/** If set, this struct will be serialized using the CPP serializer */
	STRUCT_SerializeNative		= 0x00000800,	

	/** If set, this struct will be copied using the CPP operator= */
	STRUCT_CopyNative			= 0x00001000,	

	/** If set, this struct will be copied using memcpy */
	STRUCT_IsPlainOldData		= 0x00002000,	

	/** If set, this struct has no destructor and non will be called. STRUCT_IsPlainOldData implies STRUCT_NoDestructor */
	STRUCT_NoDestructor			= 0x00004000,	

	/** If set, this struct will not be constructed because it is assumed that memory is zero before construction. */
	STRUCT_ZeroConstructor		= 0x00008000,	

	/** If set, native code will be used to export text */
	STRUCT_ExportTextItemNative	= 0x00010000,	

	/** If set, native code will be used to export text */
	STRUCT_ImportTextItemNative	= 0x00020000,	

	/** If set, this struct will have PostSerialize called on it after CPP serializer or tagged property serialization is complete */
	STRUCT_PostSerializeNative  = 0x00040000,

	/** If set, this struct will have SerializeFromMismatchedTag called on it if a mismatched tag is encountered. */
	STRUCT_SerializeFromMismatchedTag = 0x00080000,

	/** If set, this struct will be serialized using the CPP net delta serializer */
	STRUCT_NetDeltaSerializeNative = 0x00100000,

	/** If set, this struct will be have PostScriptConstruct called on it after a temporary object is constructed in a running blueprint */
	STRUCT_PostScriptConstruct     = 0x00200000,

	/** If set, this struct can share net serialization state across connections */
	STRUCT_NetSharedSerialization = 0x00400000,

	/** If set, this struct has been cleaned and sanitized (trashed) and should not be used */
	STRUCT_Trashed = 0x00800000,

	/** If set, this structure has been replaced via reinstancing */
	STRUCT_NewerVersionExists = 0x01000000,

	/** If set, this struct will have CanEditChange on it in the editor to determine if a child property can be edited */
	STRUCT_CanEditChange = 0x02000000,
	
	/** Struct flags that are automatically inherited */
	STRUCT_Inherit				= STRUCT_HasInstancedReference|STRUCT_Atomic,

	/** Flags that are always computed, never loaded or done with code generation */
	STRUCT_ComputedFlags		= STRUCT_NetDeltaSerializeNative | STRUCT_NetSerializeNative | STRUCT_SerializeNative | STRUCT_PostSerializeNative | STRUCT_CopyNative | STRUCT_IsPlainOldData | STRUCT_NoDestructor | STRUCT_ZeroConstructor | STRUCT_IdenticalNative | STRUCT_AddStructReferencedObjects | STRUCT_ExportTextItemNative | STRUCT_ImportTextItemNative | STRUCT_SerializeFromMismatchedTag | STRUCT_PostScriptConstruct | STRUCT_NetSharedSerialization
};


/** type traits to cover the custom aspects of a script struct **/
template <class CPPSTRUCT>
struct TStructOpsTypeTraitsBase2
{
	enum
	{
		WithZeroConstructor            = false,                         // struct can be constructed as a valid object by filling its memory footprint with zeroes.
		WithNoInitConstructor          = false,                         // struct has a constructor which takes an EForceInit parameter which will force the constructor to perform initialization, where the default constructor performs 'uninitialization'.
		WithNoDestructor               = false,                         // struct will not have its destructor called when it is destroyed.
		WithCopy                       = !TIsPODType<CPPSTRUCT>::Value, // struct can be copied via its copy assignment operator.
		WithIdenticalViaEquality       = false,                         // struct can be compared via its operator==.  This should be mutually exclusive with WithIdentical.
		WithIdentical                  = false,                         // struct can be compared via an Identical(const T* Other, uint32 PortFlags) function.  This should be mutually exclusive with WithIdenticalViaEquality.
		WithExportTextItem             = false,                         // struct has an ExportTextItem function used to serialize its state into a string.
		WithImportTextItem             = false,                         // struct has an ImportTextItem function used to deserialize a string into an object of that class.
		WithAddStructReferencedObjects = false,                         // struct has an AddStructReferencedObjects function which allows it to add references to the garbage collector.
		WithSerializer                 = false,                         // struct has a Serialize function for serializing its state to an FArchive.
		WithStructuredSerializer       = false,                         // struct has a Serialize function for serializing its state to an FStructuredArchive.
		WithPostSerialize              = false,                         // struct has a PostSerialize function which is called after it is serialized
		WithNetSerializer              = false,                         // struct has a NetSerialize function for serializing its state to an FArchive used for network replication.
		WithNetDeltaSerializer         = false,                         // struct has a NetDeltaSerialize function for serializing differences in state from a previous NetSerialize operation.
		WithSerializeFromMismatchedTag = false,                         // struct has a SerializeFromMismatchedTag function for converting from other property tags.
		WithStructuredSerializeFromMismatchedTag = false,               // struct has an FStructuredArchive-based SerializeFromMismatchedTag function for converting from other property tags.
		WithPostScriptConstruct        = false,                         // struct has a PostScriptConstruct function which is called after it is constructed in blueprints
		WithNetSharedSerialization     = false,                         // struct has a NetSerialize function that does not require the package map to serialize its state.
		WithGetPreloadDependencies     = false,                         // struct has a GetPreloadDependencies function to return all objects that will be Preload()ed when the struct is serialized at load time.
		WithPureVirtual                = false,                         // struct has PURE_VIRTUAL functions and cannot be constructed when CHECK_PUREVIRTUALS is true
		WithFindInnerPropertyInstance  = false,							// struct has a FindInnerPropertyInstance function that can provide an FProperty and data pointer when given a property FName
		WithCanEditChange			   = false,							// struct has an editor-only CanEditChange function that can conditionally make child properties read-only in the details panel (same idea as UObject::CanEditChange)
	};

	static constexpr EPropertyObjectReferenceType WithSerializerObjectReferences = EPropertyObjectReferenceType::Conservative; // struct's Serialize method(s) may serialize object references of these types - default Conservative means unknown and object reference collector archives should serialize this struct 
};

template<class CPPSTRUCT>
struct TStructOpsTypeTraits : public TStructOpsTypeTraitsBase2<CPPSTRUCT>
{
};

#if CHECK_PUREVIRTUALS
#define DISABLE_ABSTRACT_CONSTRUCT TStructOpsTypeTraits<CPPSTRUCT>::WithPureVirtual
#else
#define DISABLE_ABSTRACT_CONSTRUCT (false && TStructOpsTypeTraits<CPPSTRUCT>::WithPureVirtual)
#endif


	/**
	 * Selection of AddStructReferencedObjects check.
	 */
	template<class CPPSTRUCT>
	FORCEINLINE void AddStructReferencedObjectsOrNot(void* A, FReferenceCollector& Collector)
	{
		if constexpr (TStructOpsTypeTraits<CPPSTRUCT>::WithAddStructReferencedObjects)
		{
			((CPPSTRUCT*)A)->AddStructReferencedObjects(Collector);
		}
	}

/**
 * Reflection data for a standalone structure declared in a header or as a user defined struct
 */
class UScriptStruct : public UStruct
{
public:
	/** Interface to template to manage dynamic access to C++ struct construction and destruction **/
	struct ICppStructOps
	{
		/** Filled by implementation classes to report their capabilities */
		struct FCapabilities
		{
			EPropertyFlags ComputedPropertyFlags;
			EPropertyObjectReferenceType HasSerializerObjectReferences;
			bool HasNoopConstructor : 1;
			bool HasZeroConstructor : 1;
			bool HasDestructor : 1;
			bool HasSerializer : 1;
			bool HasStructuredSerializer : 1;
			bool HasPostSerialize : 1;
			bool HasNetSerializer : 1;
			bool HasNetSharedSerialization : 1;
			bool HasNetDeltaSerializer : 1;
			bool HasPostScriptConstruct : 1;
			bool IsPlainOldData : 1;
			bool IsUECoreType : 1;
			bool IsUECoreVariant : 1;
			bool HasCopy : 1;
			bool HasIdentical : 1;
			bool HasExportTextItem : 1;
			bool HasImportTextItem : 1;
			bool HasAddStructReferencedObjects : 1;
			bool HasSerializeFromMismatchedTag : 1;
			bool HasStructuredSerializeFromMismatchedTag : 1;
			bool HasGetTypeHash : 1;
			bool IsAbstract : 1;
			bool HasFindInnerPropertyInstance : 1;
#if WITH_EDITOR
			bool HasCanEditChange : 1;
#endif
		};

		/**
		 * Constructor
		 * @param InSize: sizeof() of the structure
		 */
		ICppStructOps(int32 InSize, int32 InAlignment)
			: Size(InSize)
			, Alignment(InAlignment)
		{
		}
		virtual ~ICppStructOps() {}

		/** returns struct capabilities */
		virtual FCapabilities GetCapabilities() const = 0;

		/** return true if this class has a no-op constructor and takes EForceInit to init **/
		bool HasNoopConstructor() const
		{
			return GetCapabilities().HasNoopConstructor;
		}
		/** return true if memset can be used instead of the constructor **/
		bool HasZeroConstructor() const
		{
			return GetCapabilities().HasZeroConstructor;
		}
		/** Call the C++ constructor **/
		virtual void Construct(void *Dest) = 0;
		/** Call the C++ constructor without value-init (new T instead of new T()) **/
		virtual void ConstructForTests(void* Dest) = 0;
		/** return false if this destructor can be skipped **/
		bool HasDestructor() const
		{
			return GetCapabilities().HasDestructor;
		}
		/** Call the C++ destructor **/
		virtual void Destruct(void *Dest) = 0;
		/** return the sizeof() of this structure **/
		FORCEINLINE int32 GetSize() const
		{
			return Size;
		}
		/** return the alignof() of this structure **/
		FORCEINLINE int32 GetAlignment() const
		{
			return Alignment;
		}

		/** return true if this class can serialize **/
		bool HasSerializer() const
		{
			return GetCapabilities().HasSerializer;
		}
		/** return true if this class can serialize to a structured archive**/
		bool HasStructuredSerializer() const
		{
			return GetCapabilities().HasStructuredSerializer;
		}
		/** 
		 * Serialize this structure 
		 * @return true if the package is new enough to support this, if false, it will fall back to ordinary script struct serialization
		 */
		virtual bool Serialize(FArchive& Ar, void *Data) = 0;
		virtual bool Serialize(FStructuredArchive::FSlot Slot, void *Data) = 0;

		/** return true if this class implements a post serialize call **/
		bool HasPostSerialize() const
		{
			return GetCapabilities().HasPostSerialize;
		}
		/** Call PostSerialize on this structure */
		virtual void PostSerialize(const FArchive& Ar, void *Data) = 0;

		/** return true if this struct can net serialize **/
		bool HasNetSerializer() const
		{
			return GetCapabilities().HasNetSerializer;
		}
		
		/** return true if this can share net serialization across connections */
		bool HasNetSharedSerialization() const
		{
			return GetCapabilities().HasNetSharedSerialization;
		}
		/** 
		 * Net serialize this structure 
		 * @return true if the struct was serialized, otherwise it will fall back to ordinary script struct net serialization
		 */
		virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess, void *Data) = 0;

		/** return true if this struct can net delta serialize delta (serialize a network delta from a base state) **/
		bool HasNetDeltaSerializer() const
		{
			return GetCapabilities().HasNetDeltaSerializer;
		}
		/** 
		 * Net serialize delta this structure. Serialize a network delta from a base state
		 * @return true if the struct was serialized, otherwise it will fall back to ordinary script struct net delta serialization
		 */
		virtual bool NetDeltaSerialize(FNetDeltaSerializeInfo & DeltaParms, void *Data) = 0;

		/** return true if this class implements a post script construct call **/
		bool HasPostScriptConstruct() const
		{
			return GetCapabilities().HasPostScriptConstruct;
		}
		/** Call PostScriptConstruct on this structure */
		virtual void PostScriptConstruct(void *Data) = 0;

		/** Call PreloadDependencies on this structure */
		virtual void GetPreloadDependencies(void* Data, TArray<UObject*>& OutDeps) = 0;

		/** return true if this struct should be memcopied **/
		bool IsPlainOldData() const
		{
			return GetCapabilities().IsPlainOldData;
		}

		/** return true if this struct is one of the UE Core types (and is include in CoreMinimal.h) **/
		bool IsUECoreType() const
		{
			return GetCapabilities().IsUECoreType;
		}

		/** return true if this struct is one of the UE Core types (and is include in CoreMinimal.h) **/
		bool IsUECoreVariant() const
		{
			return GetCapabilities().IsUECoreVariant;
		}

		/** return true if this struct can copy **/
		bool HasCopy() const
		{
			return GetCapabilities().HasCopy;
		}
		/** 
		 * Copy this structure 
		 * @return true if the copy was handled, otherwise it will fall back to CopySingleValue
		 */
		virtual bool Copy(void* Dest, void const* Src, int32 ArrayDim) = 0;

		/** return true if this struct can compare **/
		bool HasIdentical() const
		{
			return GetCapabilities().HasIdentical;
		}
		/** 
		 * Compare this structure 
		 * @return true if the copy was handled, otherwise it will fall back to FStructProperty::Identical
		 */
		virtual bool Identical(const void* A, const void* B, uint32 PortFlags, bool& bOutResult) = 0;

		/** return true if this struct can export **/
		bool HasExportTextItem() const
		{
			return GetCapabilities().HasExportTextItem;
		}
		/** 
		 * export this structure 
		 * @return true if the copy was exported, otherwise it will fall back to FStructProperty::ExportTextItem
		 */
		virtual bool ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, class UObject* Parent, int32 PortFlags, class UObject* ExportRootScope) = 0;

		/** return true if this struct can import **/
		bool HasImportTextItem() const
		{
			return GetCapabilities().HasImportTextItem;
		}
		/** 
		 * import this structure 
		 * @return true if the copy was imported, otherwise it will fall back to FStructProperty::ImportText
		 */
		virtual bool ImportTextItem(const TCHAR*& Buffer, void* Data, int32 PortFlags, class UObject* OwnerObject, FOutputDevice* ErrorText) = 0;

		bool HasFindInnerPropertyInstance() const
		{
			return GetCapabilities().HasFindInnerPropertyInstance;
		}
		virtual bool FindInnerPropertyInstance(FName PropertyName, const void* Data, const FProperty*& OutProp, const void*& OutData) const = 0;

		/** return true if this struct has custom GC code **/
		bool HasAddStructReferencedObjects() const
		{
			return GetCapabilities().HasAddStructReferencedObjects;
		}
		/** returns true if the native serialize functions may serialize object references of the given type **/
		bool HasSerializerObjectReferences(EPropertyObjectReferenceType Type) const
		{
			return EnumHasAnyFlags(GetCapabilities().HasSerializerObjectReferences, Type);
		}
		/** 
		 * return a pointer to a function that can add referenced objects
		 * @return true if the copy was imported, otherwise it will fall back to FStructProperty::ImportText
		 */
		typedef void (*TPointerToAddStructReferencedObjects)(void* A, class FReferenceCollector& Collector);
		virtual TPointerToAddStructReferencedObjects AddStructReferencedObjects() = 0;

		/** return true if this class wants to serialize from some other tag (usually for conversion purposes) **/
		bool HasSerializeFromMismatchedTag() const
		{
			return GetCapabilities().HasSerializeFromMismatchedTag;
		}
		bool HasStructuredSerializeFromMismatchedTag() const
		{
			return GetCapabilities().HasStructuredSerializeFromMismatchedTag;
		}

		/** 
		 * Serialize this structure, from some other tag
		 * @return true if this succeeded, false will trigger a warning and not serialize at all
		 */
		virtual bool SerializeFromMismatchedTag(struct FPropertyTag const& Tag, FArchive& Ar, void *Data) = 0;
		virtual bool StructuredSerializeFromMismatchedTag(struct FPropertyTag const& Tag, FStructuredArchive::FSlot Slot, void *Data) = 0;

		/** return true if this struct has a GetTypeHash */
		bool HasGetTypeHash() const
		{
			return GetCapabilities().HasGetTypeHash;
		}

		/** Calls GetTypeHash if enabled */
		virtual uint32 GetStructTypeHash(const void* Src) = 0;

		/** Returns property flag values that can be computed at compile time */
		EPropertyFlags GetComputedPropertyFlags() const
		{
			return GetCapabilities().ComputedPropertyFlags;
		}

		/** return true if this struct is abstract **/
		bool IsAbstract() const
		{
			return GetCapabilities().IsAbstract;
		}

#if WITH_EDITOR
		/** Returns true if this struct wants to indicate whether a property can be edited in the details panel */
		bool HasCanEditChange() const
		{
			return GetCapabilities().HasCanEditChange;
		}
		/** Returns true if this struct would allow the given property to be edited in the details panel. */
		virtual bool CanEditChange(const FEditPropertyChain& PropertyChain, const void* Data) const = 0;
#endif
		
	private:
		/** sizeof() of the structure **/
		const int32 Size;
		/** alignof() of the structure **/
		const int32 Alignment;
	};


	/** Template to manage dynamic access to C++ struct construction and destruction **/
	template<class CPPSTRUCT>
	struct TCppStructOps final : public ICppStructOps
	{
		typedef TStructOpsTypeTraits<CPPSTRUCT> TTraits;
		TCppStructOps()
			: ICppStructOps(sizeof(CPPSTRUCT), alignof(CPPSTRUCT))
		{
		}

		virtual FCapabilities GetCapabilities() const override
		{
			constexpr FCapabilities Capabilities {
				(TIsPODType<CPPSTRUCT>::Value ? CPF_IsPlainOldData : CPF_None)
				| (TIsTriviallyDestructible<CPPSTRUCT>::Value ? CPF_NoDestructor : CPF_None)
				| (TIsZeroConstructType<CPPSTRUCT>::Value ? CPF_ZeroConstructor : CPF_None)
				| (TModels_V<CGetTypeHashable, CPPSTRUCT> ? CPF_HasGetValueTypeHash : CPF_None),
				TTraits::WithSerializerObjectReferences,
				TTraits::WithNoInitConstructor,
				TTraits::WithZeroConstructor,
				!(TTraits::WithNoDestructor || TIsPODType<CPPSTRUCT>::Value),
				TTraits::WithSerializer,
				TTraits::WithStructuredSerializer,
				TTraits::WithPostSerialize,
				TTraits::WithNetSerializer,
				TTraits::WithNetSharedSerialization,
				TTraits::WithNetDeltaSerializer,
				TTraits::WithPostScriptConstruct,
				TIsPODType<CPPSTRUCT>::Value,
				TIsUECoreType<CPPSTRUCT>::Value,
				TIsUECoreVariant<CPPSTRUCT>::Value,
				TTraits::WithCopy,
				TTraits::WithIdentical || TTraits::WithIdenticalViaEquality,
				TTraits::WithExportTextItem,
				TTraits::WithImportTextItem,
				TTraits::WithAddStructReferencedObjects,
				TTraits::WithSerializeFromMismatchedTag,
				TTraits::WithStructuredSerializeFromMismatchedTag,
				TModels_V<CGetTypeHashable, CPPSTRUCT>,
				TIsAbstract<CPPSTRUCT>::Value,
				TTraits::WithFindInnerPropertyInstance,
#if WITH_EDITOR
				TTraits::WithCanEditChange,
#endif
			};
			return Capabilities;
		}
		virtual void Construct(void* Dest) override
		{
			check(!TTraits::WithZeroConstructor); // don't call this if we have indicated it is not necessary
			// that could have been an if statement, but we might as well force optimization above the virtual call
			// could also not attempt to call the constructor for types where this is not possible, but I didn't do that here
#if CHECK_PUREVIRTUALS
			if constexpr (!TStructOpsTypeTraits<CPPSTRUCT>::WithPureVirtual)
#endif
			{
				if constexpr (TStructOpsTypeTraits<CPPSTRUCT>::WithNoInitConstructor)
				{
					new (Dest) CPPSTRUCT(ForceInit);
				}
				else
				{
					new (Dest) CPPSTRUCT();
				}
			}
		}
		virtual void ConstructForTests(void* Dest) override
		{
			check(!TTraits::WithZeroConstructor); // don't call this if we have indicated it is not necessary
			// that could have been an if statement, but we might as well force optimization above the virtual call
			// could also not attempt to call the constructor for types where this is not possible, but I didn't do that here
#if CHECK_PUREVIRTUALS
			if constexpr (!TStructOpsTypeTraits<CPPSTRUCT>::WithPureVirtual)
#endif
			{
				if constexpr (TStructOpsTypeTraits<CPPSTRUCT>::WithNoInitConstructor)
				{
					new (Dest) CPPSTRUCT(ForceInit);
				}
				else
				{
					new (Dest) CPPSTRUCT;
				}
			}
		}
		virtual void Destruct(void *Dest) override
		{
			check(!(TTraits::WithNoDestructor || TIsPODType<CPPSTRUCT>::Value)); // don't call this if we have indicated it is not necessary
			// that could have been an if statement, but we might as well force optimization above the virtual call
			// could also not attempt to call the destructor for types where this is not possible, but I didn't do that here
			((CPPSTRUCT*)Dest)->~CPPSTRUCT();
		}
		virtual bool Serialize(FArchive& Ar, void *Data) override
		{
			check(TTraits::WithSerializer); // don't call this if we have indicated it is not necessary
			if constexpr (TStructOpsTypeTraits<CPPSTRUCT>::WithSerializer)
			{
				return ((CPPSTRUCT*)Data)->Serialize(Ar);
			}
			else
			{
				return false;
			}
		}
		virtual bool Serialize(FStructuredArchive::FSlot Slot, void *Data) override
		{
			check(TTraits::WithStructuredSerializer); // don't call this if we have indicated it is not necessary
			if constexpr (TStructOpsTypeTraits<CPPSTRUCT>::WithStructuredSerializer)
			{
				return ((CPPSTRUCT*)Data)->Serialize(Slot);
			}
			else
			{
				return false;
			}
		}
		virtual void PostSerialize(const FArchive& Ar, void *Data) override
		{
			check(TTraits::WithPostSerialize); // don't call this if we have indicated it is not necessary
			if constexpr (TStructOpsTypeTraits<CPPSTRUCT>::WithPostSerialize)
			{
				((CPPSTRUCT*)Data)->PostSerialize(Ar);
			}
		}
		virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess, void *Data) override
		{
			if constexpr (TStructOpsTypeTraits<CPPSTRUCT>::WithNetSerializer)
			{
				return ((CPPSTRUCT*)Data)->NetSerialize(Ar, Map, bOutSuccess);
			}
			else
			{
				return false;
			}
		}
		virtual bool NetDeltaSerialize(FNetDeltaSerializeInfo & DeltaParms, void *Data) override
		{
			if constexpr (TStructOpsTypeTraits<CPPSTRUCT>::WithNetDeltaSerializer)
			{
				return ((CPPSTRUCT*)Data)->NetDeltaSerialize(DeltaParms);
			}
			else
			{
				return false;
			}
		}
		virtual void PostScriptConstruct(void *Data) override
		{
			check(TTraits::WithPostScriptConstruct); // don't call this if we have indicated it is not necessary
			if constexpr (TStructOpsTypeTraits<CPPSTRUCT>::WithPostScriptConstruct)
			{
				((CPPSTRUCT*)Data)->PostScriptConstruct();
			}
		}
		virtual void GetPreloadDependencies(void* Data, TArray<UObject*>& OutDeps) override
		{
			if constexpr (TStructOpsTypeTraits<CPPSTRUCT>::WithGetPreloadDependencies)
			{
				((CPPSTRUCT*)Data)->GetPreloadDependencies(OutDeps);
			}
		}
		virtual bool Copy(void* Dest, void const* Src, int32 ArrayDim) override
		{
			if constexpr (TStructOpsTypeTraits<CPPSTRUCT>::WithCopy)
			{
				static_assert((!TIsPODType<CPPSTRUCT>::Value), "You probably don't want custom copy for a POD type.");

				CPPSTRUCT* TypedDest = (CPPSTRUCT*)Dest;
				const CPPSTRUCT* TypedSrc  = (const CPPSTRUCT*)Src;

				for (; ArrayDim; --ArrayDim)
				{
					*TypedDest++ = *TypedSrc++;
				}
				return true;
			}
			else
			{
				return false;
			}
		}
		virtual bool Identical(const void* A, const void* B, uint32 PortFlags, bool& bOutResult) override
		{
			check((TTraits::WithIdentical || TTraits::WithIdenticalViaEquality)); // don't call this if we have indicated it is not necessary
			if constexpr (TStructOpsTypeTraits<CPPSTRUCT>::WithIdentical)
			{
				static_assert(!TStructOpsTypeTraits<CPPSTRUCT>::WithIdenticalViaEquality, "Should not have both WithIdenticalViaEquality and WithIdentical.");

				bOutResult = ((const CPPSTRUCT*)A)->Identical((const CPPSTRUCT*)B, PortFlags);
				return true;
			}
			else if constexpr (TStructOpsTypeTraits<CPPSTRUCT>::WithIdenticalViaEquality)
			{
				bOutResult = (*(const CPPSTRUCT*)A == *(const CPPSTRUCT*)B);
				return true;
			}
			else
			{
				bOutResult = false;
				return false;
			}
		}
		virtual bool ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, class UObject* Parent, int32 PortFlags, class UObject* ExportRootScope) override
		{
			check(TTraits::WithExportTextItem); // don't call this if we have indicated it is not necessary
			if constexpr (TStructOpsTypeTraits<CPPSTRUCT>::WithExportTextItem)
			{
				if (DefaultValue)
				{
					return ((const CPPSTRUCT*)PropertyValue)->ExportTextItem(ValueStr, *(const CPPSTRUCT*)DefaultValue, Parent, PortFlags, ExportRootScope);
				}
				else
				{
					TTypeCompatibleBytes<CPPSTRUCT> TmpDefaultValue;
					FMemory::Memzero(TmpDefaultValue.GetTypedPtr(), sizeof(CPPSTRUCT));
					if (!HasZeroConstructor())
					{
						Construct(TmpDefaultValue.GetTypedPtr());
					}

					const bool bResult = ((const CPPSTRUCT*)PropertyValue)->ExportTextItem(ValueStr, *(const CPPSTRUCT*)TmpDefaultValue.GetTypedPtr(), Parent, PortFlags, ExportRootScope);

					if (HasDestructor())
					{
						Destruct(TmpDefaultValue.GetTypedPtr());
					}

					return bResult;
				}
			}
			else
			{
				return false;
			}
		}
		virtual bool ImportTextItem(const TCHAR*& Buffer, void* Data, int32 PortFlags, class UObject* OwnerObject, FOutputDevice* ErrorText) override
		{
			check(TTraits::WithImportTextItem); // don't call this if we have indicated it is not necessary
			if constexpr (TStructOpsTypeTraits<CPPSTRUCT>::WithImportTextItem)
			{
				return ((CPPSTRUCT*)Data)->ImportTextItem(Buffer, PortFlags, OwnerObject, ErrorText);
			}
			else
			{
				return false;
			}
		}
		
		virtual bool FindInnerPropertyInstance(FName PropertyName, const void* Data, const FProperty*& OutProp, const void*& OutData) const override
		{
			check(TTraits::WithFindInnerPropertyInstance); // don't call this if we have indicated it is not necessary
			if constexpr (TStructOpsTypeTraits<CPPSTRUCT>::WithFindInnerPropertyInstance)
            {
            	return ((CPPSTRUCT*)Data)->FindInnerPropertyInstance(PropertyName, OutProp, OutData);
            }
            else
            {
            	return false;
            }
		}

		virtual TPointerToAddStructReferencedObjects AddStructReferencedObjects() override
		{
			check(TTraits::WithAddStructReferencedObjects); // don't call this if we have indicated it is not necessary
			return &AddStructReferencedObjectsOrNot<CPPSTRUCT>;
		}
		virtual bool SerializeFromMismatchedTag(struct FPropertyTag const& Tag, FArchive& Ar, void *Data) override
		{
			check(TTraits::WithSerializeFromMismatchedTag); // don't call this if we have indicated it is not allowed
			if constexpr (TStructOpsTypeTraits<CPPSTRUCT>::WithSerializeFromMismatchedTag)
			{
				if constexpr (TIsUECoreType<CPPSTRUCT>::Value)
				{
					// Custom version of SerializeFromMismatchedTag for core types, which don't have access to FPropertyTag.
					FName StructName;
					if (Tag.GetType().GetName() == NAME_StructProperty && Tag.GetType().GetParameterCount() >= 1)
					{
						StructName = Tag.GetType().GetParameterName(0);
					}
					return ((CPPSTRUCT*)Data)->SerializeFromMismatchedTag(StructName, Ar);
				}
				else
				{
					return ((CPPSTRUCT*)Data)->SerializeFromMismatchedTag(Tag, Ar);
				}
			}
			else
			{
				return false;
			}
		}
		virtual bool StructuredSerializeFromMismatchedTag(struct FPropertyTag const& Tag, FStructuredArchive::FSlot Slot, void *Data) override
		{
			check(TTraits::WithStructuredSerializeFromMismatchedTag); // don't call this if we have indicated it is not allowed
			if constexpr (TStructOpsTypeTraits<CPPSTRUCT>::WithStructuredSerializeFromMismatchedTag)
			{
				if constexpr (TIsUECoreType<CPPSTRUCT>::Value)
				{
					// Custom version of SerializeFromMismatchedTag for core types, which don't understand FPropertyTag.
					FName StructName;
					if (Tag.GetType().GetName() == NAME_StructProperty && Tag.GetType().GetParameterCount() >= 1)
					{
						StructName = Tag.GetType().GetParameterName(0);
					}
					return ((CPPSTRUCT*)Data)->SerializeFromMismatchedTag(StructName, Slot);
				}
				else
				{
					return ((CPPSTRUCT*)Data)->SerializeFromMismatchedTag(Tag, Slot);
				}
			}
			else
			{
				return false;
			}
		}

		static_assert(!(TTraits::WithSerializeFromMismatchedTag && TTraits::WithStructuredSerializeFromMismatchedTag), "Structs cannot have both WithSerializeFromMismatchedTag and WithStructuredSerializeFromMismatchedTag set");

		uint32 GetStructTypeHash(const void* Src) override
		{
			ensure(HasGetTypeHash());

			if constexpr (TModels_V<CGetTypeHashable, CPPSTRUCT>)
			{
				return GetTypeHashHelper(*(const CPPSTRUCT*)Src);
			}
			else
			{
				return 0;
			}
		}

#if WITH_EDITOR

		virtual bool CanEditChange(const FEditPropertyChain& PropertyChain, const void* Data) const override
		{
			if constexpr (TStructOpsTypeTraits<CPPSTRUCT>::WithCanEditChange)
			{
				return ((const CPPSTRUCT*)Data)->CanEditChange(PropertyChain);
			}
			else
			{
				return false;
			}
		}
#endif // WITH_EDITOR
	};

	/** Template for noexport classes to autoregister before main starts **/
	template<class CPPSTRUCT>
	struct TAutoCppStructOps
	{
		TAutoCppStructOps(FTopLevelAssetPath InName)
		{
			DeferCppStructOps(InName,new TCppStructOps<CPPSTRUCT>);
		}
	};
	#define IMPLEMENT_STRUCT(BaseName) \
		UE_DEPRECATED_MACRO(5.1, "IMPLEMENT_STRUCT has been deprecated. Use UE_IMPLEMENT_STRUCT and provide struct package name as well as struct name") static UScriptStruct::TAutoCppStructOps<F##BaseName> BaseName##_Ops(FTopLevelAssetPath(TEXT("/Script/CoreUObject"), TEXT(#BaseName))); 

	#define UE_IMPLEMENT_STRUCT(PackageNameText, BaseName) \
		static UScriptStruct::TAutoCppStructOps<F##BaseName> BaseName##_Ops(FTopLevelAssetPath(TEXT(PackageNameText), TEXT(#BaseName))); 

	DECLARE_CASTED_CLASS_INTRINSIC_NO_CTOR(UScriptStruct, UStruct, CLASS_MatchedSerializers, TEXT("/Script/CoreUObject"), CASTCLASS_UScriptStruct, COREUOBJECT_API)

	COREUOBJECT_API UScriptStruct( EStaticConstructor, int32 InSize, int32 InAlignment, EObjectFlags InFlags );
	COREUOBJECT_API explicit UScriptStruct(const FObjectInitializer& ObjectInitializer, UScriptStruct* InSuperStruct, ICppStructOps* InCppStructOps = nullptr, EStructFlags InStructFlags = STRUCT_NoFlags, SIZE_T ExplicitSize = 0, SIZE_T ExplicitAlignment = 0);
	COREUOBJECT_API explicit UScriptStruct(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	EStructFlags StructFlags;

protected:
	/** true if we have performed PrepareCppStructOps **/
	bool bPrepareCppStructOpsCompleted;
	/** Holds the Cpp ctors and dtors, sizeof, etc. Is not owned by this and is not released. **/
	ICppStructOps* CppStructOps;

	/** 
	* Similar to GetStructPathName() but works with nested structs by using just the package name and struct name
	* so a struct path name /Package/Name.Object:Struct will be flattened to /Package/Name.Struct.
	* This function is used only for generating keys for DeferredCppStructOps
	*/
	COREUOBJECT_API FTopLevelAssetPath GetFlattenedStructPathName() const;

public:

	// UObject Interface
	virtual COREUOBJECT_API void Serialize(FArchive& Ar) override;
	virtual COREUOBJECT_API void Serialize(FStructuredArchive::FRecord Record) override;

	// UStruct interface.
	virtual COREUOBJECT_API void Link(FArchive& Ar, bool bRelinkExistingProperties) override;
	virtual COREUOBJECT_API void InitializeStruct(void* Dest, int32 ArrayDim = 1) const override;
	virtual COREUOBJECT_API void DestroyStruct(void* Dest, int32 ArrayDim = 1) const override;
	virtual COREUOBJECT_API bool IsStructTrashed() const override;
	// End of UStruct interface.

	/** Sets or unsets the trashed flag on this struct */
	void COREUOBJECT_API SetStructTrashed(bool bIsTrash);

	/** 
	 * Stash a CppStructOps for future use 
	 * @param Target Name of the struct 
	 * @param InCppStructOps Cpp ops for this struct
	 */
	static COREUOBJECT_API void DeferCppStructOps(FTopLevelAssetPath Target, ICppStructOps* InCppStructOps);

	template<class CPPSTRUCT>
	static void DeferCppStructOps(FTopLevelAssetPath Target)
	{
		if constexpr (DISABLE_ABSTRACT_CONSTRUCT)
		{
			DeferCppStructOps(Target, nullptr);
		}
		else
		{
			DeferCppStructOps(Target, new UScriptStruct::TCppStructOps<CPPSTRUCT>);
		}
	}

	/** Look for the CppStructOps and hook it up **/
	virtual COREUOBJECT_API void PrepareCppStructOps();

	/** Returns the CppStructOps that can be used to do custom operations */
	FORCEINLINE ICppStructOps* GetCppStructOps() const
	{
		checkf(bPrepareCppStructOpsCompleted, TEXT("GetCppStructOps: PrepareCppStructOps() has not been called for class %s"), *GetName());
		return CppStructOps;
	}

	/** Resets currently assigned CppStructOps, called when loading a struct */
	void ClearCppStructOps()
	{
		StructFlags = EStructFlags(StructFlags & ~STRUCT_ComputedFlags);
		bPrepareCppStructOpsCompleted = false;
		CppStructOps = nullptr;
	}

	/** 
	 * If it is native, it is assumed to have defaults because it has a constructor
	 * @return true if this struct has defaults
	 */
	FORCEINLINE bool HasDefaults() const
	{
		return !!GetCppStructOps();
	}

	/**
	 * Returns whether this struct should be serialized atomically.
	 * @param	Ar	Archive the struct is going to be serialized with later on
	 */
	bool ShouldSerializeAtomically(FArchive& Ar) const
	{
		if( (StructFlags&STRUCT_Atomic) != 0)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	/** Returns true if this struct has a native serialize function */
	bool UseNativeSerialization() const
	{
		if ((StructFlags&(STRUCT_SerializeNative)) != 0)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	/** Returns true if this struct should be binary serialized for the given archive */
	COREUOBJECT_API bool UseBinarySerialization(const FArchive& Ar) const;

	/** 
	 * Serializes a specific instance of a struct 
	 *
	 * @param	Slot		The structured archive slot we are serializing to
	 * @param	Value		Pointer to memory of struct
	 * @param	Defaults	Default value for this struct, pass nullptr to not use defaults 
	 */
	COREUOBJECT_API void SerializeItem(FArchive& Ar, void* Value, void const* Defaults);
	COREUOBJECT_API void SerializeItem(FStructuredArchive::FSlot Slot, void* Value, void const* Defaults);

	/**
	 * Export script struct to a string that can later be imported
	 *
	 * @param	ValueStr		String to write to
	 * @param	Value			Actual struct being exported
	 * @param	Defaults		Default value for this struct, pass nullptr to not use defaults 
	 * @param	OwnerObject		UObject that contains this struct
	 * @param	PortFlags		EPropertyPortFlags controlling export behavior
	 * @param	ExportRootScope	The scope to create relative paths from, if the PPF_ExportsNotFullyQualified flag is passed in.  If NULL, the package containing the object will be used instead.
	 * @param	bAllowNativeOverride If true, will try to run native version of export text on the struct
	 */
	COREUOBJECT_API void ExportText(FString& ValueStr, const void* Value, const void* Defaults, UObject* OwnerObject, int32 PortFlags, UObject* ExportRootScope, bool bAllowNativeOverride = true) const;

	/**
	 * Sets value of script struct based on imported string
	 *
	 * @param	Buffer			String to read text data out of
	 * @param	Value			Struct that will be modified
	 * @param	OwnerObject		UObject that contains this struct
	 * @param	PortFlags		EPropertyPortFlags controlling import behavior
	 * @param	ErrorText		What to print import errors to
	 * @param	StructName		Name of struct, used in error display
	 * @param	bAllowNativeOverride If true, will try to run native version of export text on the struct
	 * @return Buffer after parsing has succeeded, or NULL on failure
	 */
	COREUOBJECT_API const TCHAR* ImportText(const TCHAR* Buffer, void* Value, UObject* OwnerObject, int32 PortFlags, FOutputDevice* ErrorText, const FString& StructName, bool bAllowNativeOverride = true) const;

	/**
	 * Sets value of script struct based on imported string
	 *
	 * @param	Buffer			String to read text data out of
	 * @param	Value			Struct that will be modified
	 * @param	OwnerObject		UObject that contains this struct
	 * @param	PortFlags		EPropertyPortFlags controlling import behavior
	 * @param	ErrorText		What to print import errors to
	 * @param	StructNameGetter Function to return the struct name to avoid doing work if no error message is forthcoming
	 * @param	bAllowNativeOverride If true, will try to run native version of export text on the struct
	 * @return Buffer after parsing has succeeded, or NULL on failure
	 */
	COREUOBJECT_API const TCHAR* ImportText(const TCHAR* Buffer, void* Value, UObject* OwnerObject, int32 PortFlags, FOutputDevice* ErrorText, const TFunctionRef<FString()>& StructNameGetter, bool bAllowNativeOverride = true) const;

	/**
	 * Compare two script structs
	 *
	 * @param	Dest		Pointer to memory to a struct
	 * @param	Src			Pointer to memory to the other struct
	 * @param	PortFlags	Comparison flags
	 * @return true if the structs are identical
	 */
	COREUOBJECT_API bool CompareScriptStruct(const void* A, const void* B, uint32 PortFlags) const;

	/**
	 * Copy a struct over an existing struct
	 *
	 * @param	Dest		Pointer to memory to initialize
	 * @param	Src			Pointer to memory to copy from
	 * @param	ArrayDim	Number of elements in the array
	 * @param	Stride		Stride of the array, If this default (0), then we will pull the size from the struct
	 */
	COREUOBJECT_API void CopyScriptStruct(void* Dest, void const* Src, int32 ArrayDim = 1) const;
	
	/**
	 * Reinitialize a struct in memory. This may be done by calling the native destructor and then the constructor or individually reinitializing properties
	 *
	 * @param	Dest		Pointer to memory to reinitialize
	 * @param	ArrayDim	Number of elements in the array
	 * @param	Stride		Stride of the array, only relevant if there more than one element. If this default (0), then we will pull the size from the struct
	 */
	COREUOBJECT_API void ClearScriptStruct(void* Dest, int32 ArrayDim = 1) const;

	/**
	 * Calls GetTypeHash for native structs, otherwise computes a hash of all struct members
	 * 
	 * @param Src		Pointer to instance to hash
	 * @return hashed value of Src
	 */
	virtual COREUOBJECT_API uint32 GetStructTypeHash(const void* Src) const;

	/** Used by User Defined Structs to preload this struct and any child objects */
	virtual COREUOBJECT_API void RecursivelyPreload();

	/** Returns the custom Guid assigned to this struct for User Defined Structs, or an invalid Guid */
	virtual COREUOBJECT_API FGuid GetCustomGuid() const;
	
	/** Returns the (native, c++) name of the struct */
	virtual COREUOBJECT_API FString GetStructCPPName(uint32 CPPExportFlags = 0) const;

	/**
	 * Initializes this structure to its default values
	 * @param InStructData		The memory location to initialize
	 */
	virtual COREUOBJECT_API void InitializeDefaultValue(uint8* InStructData) const;
	
	/**
	 * Provide an FProperty and data pointer when given an FName of a property that this object owns indirectly (ie FInstancedStruct)
	 * @param PropertyName	Name of the property to retrieve
	 * @param Data			Pointer to struct to retrieve property from
	 * @param OutProp		Filled with the found property
	 * @param OutData		Filled with a pointer to the data containing the property.
	 *						Not offset to property instance yet - Call ContainerPtrToValuePtr before use.
	 * @return				whether the property instance was found
	 */
	virtual COREUOBJECT_API bool FindInnerPropertyInstance(FName PropertyName, const void* Data, const FProperty*& OutProp, const void*& OutData) const;
};

/*-----------------------------------------------------------------------------
	UFunction.
-----------------------------------------------------------------------------*/

//
// Reflection data for a replicated or Kismet callable function.
//
class UFunction : public UStruct
{
	DECLARE_CASTED_CLASS_INTRINSIC_WITH_API(UFunction, UStruct, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UFunction, COREUOBJECT_API)
	DECLARE_WITHIN(UClass)
public:
	// Persistent variables.

	/** EFunctionFlags set defined for this function */
	EFunctionFlags FunctionFlags;

	// Variables in memory only.
	
	/** Number of parameters total */
	uint8 NumParms;
	/** Total size of parameters in memory */
	uint16 ParmsSize;
	/** Memory offset of return value property */
	uint16 ReturnValueOffset;
	/** Id of this RPC function call (must be FUNC_Net & (FUNC_NetService|FUNC_NetResponse)) */
	uint16 RPCId;
	/** Id of the corresponding response call (must be FUNC_Net & FUNC_NetService) */
	uint16 RPCResponseId;

	/** pointer to first local struct property in this UFunction that contains defaults */
	FProperty* FirstPropertyToInit;

#if UE_BLUEPRINT_EVENTGRAPH_FASTCALLS
	/** The event graph this function calls in to (persistent) */
	UFunction* EventGraphFunction;

	/** The state offset inside of the event graph (persistent) */
	int32 EventGraphCallOffset;
#endif

#if WITH_LIVE_CODING
	/** Pointer to the cached singleton pointer to this instance */
	UFunction** SingletonPtr;
#endif

private:
	/** C++ function this is bound to */
	FNativeFuncPtr Func;

public:
	/**
	 * Returns the native func pointer.
	 *
	 * @return The native function pointer.
	 */
	FORCEINLINE FNativeFuncPtr GetNativeFunc() const
	{
		return Func;
	}

	/**
	 * Sets the native func pointer.
	 *
	 * @param InFunc - The new function pointer.
	 */
	FORCEINLINE void SetNativeFunc(FNativeFuncPtr InFunc)
	{
		Func = InFunc;
	}

	/**
	 * Invokes the UFunction on a UObject.
	 *
	 * @param Obj    - The object to invoke the function on.
	 * @param Stack  - The parameter stack for the function call.
	 * @param Result - The result of the function.
	 */
	COREUOBJECT_API void Invoke(UObject* Obj, FFrame& Stack, RESULT_DECL);

	// Constructors.
	COREUOBJECT_API explicit UFunction(const FObjectInitializer& ObjectInitializer, UFunction* InSuperFunction, EFunctionFlags InFunctionFlags = FUNC_None, SIZE_T ParamsSize = 0 );
	COREUOBJECT_API explicit UFunction(UFunction* InSuperFunction, EFunctionFlags InFunctionFlags = FUNC_None, SIZE_T ParamsSize = 0);

	/** Initializes transient members like return value offset */
	COREUOBJECT_API void InitializeDerivedMembers();

	// UObject interface.
	COREUOBJECT_API virtual void Serialize( FArchive& Ar ) override;
	COREUOBJECT_API virtual void PostLoad() override;

	// UField interface.
	COREUOBJECT_API virtual void Bind() override;

	// UStruct interface.
	virtual UStruct* GetInheritanceSuper() const override { return nullptr;}
	COREUOBJECT_API virtual void Link(FArchive& Ar, bool bRelinkExistingProperties) override;

	/** Returns parent function if there is one, or null */
	COREUOBJECT_API UFunction* GetSuperFunction() const;

	/** Returns the return value property if there is one, or null */
	COREUOBJECT_API FProperty* GetReturnProperty() const;

	/** Returns the owning UClass* without branching */
	FORCEINLINE UClass* GetOuterUClassUnchecked() const
	{
		// declaration order mandates reinterpret_cast:
		return reinterpret_cast<UClass*>(GetOuter());
	}

	/**
	 * Used to safely check whether the passed in flag is set.
	 *
	 * @param	FlagToCheck		Class flag to check for
	 *
	 * @return	true if the passed in flag is set, false otherwise
	 *			(including no flag passed in, unless the FlagsToCheck is CLASS_AllFlags)
	 */
	FORCEINLINE bool HasAnyFunctionFlags( EFunctionFlags FlagsToCheck ) const
	{
		return (FunctionFlags&FlagsToCheck) != 0 || FlagsToCheck == FUNC_AllFlags;
	}

	/**
	 * Used to safely check whether all of the passed in flags are set.
	 *
	 * @param FlagsToCheck	Function flags to check for
	 * @return true if all of the passed in flags are set (including no flags passed in), false otherwise
	 */
	FORCEINLINE bool HasAllFunctionFlags( EFunctionFlags FlagsToCheck ) const
	{
		return ((FunctionFlags & FlagsToCheck) == FlagsToCheck);
	}

	/**
	 * Returns the flags that are ignored by default when comparing function signatures.
	 */
	FORCEINLINE static uint64 GetDefaultIgnoredSignatureCompatibilityFlags()
	{
		//@TODO: UCREMOVAL: CPF_ConstParm added as a hack to get blueprints compiling with a const DamageType parameter.
		const uint64 IgnoreFlags = CPF_PersistentInstance | CPF_ExportObject | CPF_InstancedReference 
			| CPF_ContainsInstancedReference | CPF_ComputedFlags | CPF_ConstParm | CPF_UObjectWrapper | CPF_TObjectPtr
			| CPF_NativeAccessSpecifiers | CPF_AdvancedDisplay | CPF_BlueprintVisible | CPF_BlueprintReadOnly;
		return IgnoreFlags;
	}

	/**
	 * Determines if two functions have an identical signature (note: currently doesn't allow
	 * matches with class parameters that differ only in how derived they are; there is no
	 * directionality to the call)
	 *
	 * @param	OtherFunction	Function to compare this function against.
	 *
	 * @return	true if function signatures are compatible.
	 */
	COREUOBJECT_API bool IsSignatureCompatibleWith(const UFunction* OtherFunction) const;

	/**
	 * Determines if two functions have an identical signature (note: currently doesn't allow
	 * matches with class parameters that differ only in how derived they are; there is no
	 * directionality to the call)
	 *
	 * @param	OtherFunction	Function to compare this function against.
	 * @param   IgnoreFlags     Custom flags to ignore when comparing parameters between the functions.
	 *
	 * @return	true if function signatures are compatible.
	 */
	COREUOBJECT_API bool IsSignatureCompatibleWith(const UFunction* OtherFunction, uint64 IgnoreFlags) const;
};

//
// Function definition used by dynamic delegate declarations
//
class UDelegateFunction : public UFunction
{
	DECLARE_CASTED_CLASS_INTRINSIC_WITH_API(UDelegateFunction, UFunction, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UDelegateFunction, COREUOBJECT_API)
	DECLARE_WITHIN(UObject)
public:
	COREUOBJECT_API explicit UDelegateFunction(const FObjectInitializer& ObjectInitializer, UFunction* InSuperFunction, EFunctionFlags InFunctionFlags = FUNC_None, SIZE_T ParamsSize = 0);
	COREUOBJECT_API explicit UDelegateFunction(UFunction* InSuperFunction, EFunctionFlags InFunctionFlags = FUNC_None, SIZE_T ParamsSize = 0);
};

//
// Function definition used by sparse dynamic delegate declarations
//
class USparseDelegateFunction : public UDelegateFunction
{
	DECLARE_CASTED_CLASS_INTRINSIC_WITH_API(USparseDelegateFunction, UDelegateFunction, 0, TEXT("/Script/CoreUObject"), CASTCLASS_USparseDelegateFunction, COREUOBJECT_API)
	DECLARE_WITHIN(UObject)
public:
	COREUOBJECT_API explicit USparseDelegateFunction(const FObjectInitializer& ObjectInitializer, UFunction* InSuperFunction, EFunctionFlags InFunctionFlags = FUNC_None, SIZE_T ParamsSize = 0);
	COREUOBJECT_API explicit USparseDelegateFunction(UFunction* InSuperFunction, EFunctionFlags InFunctionFlags = FUNC_None, SIZE_T ParamsSize = 0);

	COREUOBJECT_API virtual void Serialize(FArchive& Ar) override;

	FName OwningClassName;
	FName DelegateName;
};

/*-----------------------------------------------------------------------------
	UEnum.
-----------------------------------------------------------------------------*/

typedef FText(*FEnumDisplayNameFn)(int32);

/** Optional flags for the UEnum::Get*ByName() functions. */
enum class EGetByNameFlags
{
	None = 0,

	/** Outputs an warning if the enum lookup fails */
	ErrorIfNotFound		= 0x01,

	/** Does a case sensitive match */
	CaseSensitive		= 0x02,

	/** Checks the GetAuthoredNameStringByIndex value as well as normal names */
	CheckAuthoredName	= 0x04,
};

ENUM_CLASS_FLAGS(EGetByNameFlags)

//
// Reflection data for an enumeration.
//
class UEnum : public UField
{
	DECLARE_CASTED_CLASS_INTRINSIC_NO_CTOR(UEnum, UField, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UEnum, COREUOBJECT_API)
	COREUOBJECT_API UEnum(const FObjectInitializer& ObjectInitialzer);

public:
	/** How this enum is declared in C++, affects the internal naming of enum values */
	enum class ECppForm
	{
		Regular,
		Namespaced,
		EnumClass
	};

	/** This will be the true type of the enum as a string, e.g. "ENamespacedEnum::InnerType" or "ERegularEnum" or "EEnumClass" */
	FString CppType;

	// Index is the internal index into the Enum array, and is not useful outside of the Enum system
	// Value is the value set in the Enum Class in C++ or Blueprint
	// Enums can be sparse, which means that not every valid Index is a proper Value, and they are not necessarily equal
	// It is not safe to cast an Index to a Enum Class, always do that with a Value instead

	/** Gets the internal index for an enum value. Returns INDEX_None if not valid */
	FORCEINLINE int32 GetIndexByValue(int64 InValue) const
	{
		for (int32 i = 0; i < Names.Num(); ++i)
		{
			if (Names[i].Value == InValue)
			{
				return i;
			}
		}
		return INDEX_NONE;
	}

	/** Gets enum value by index in Names array. Asserts on invalid index */
	FORCEINLINE int64 GetValueByIndex(int32 Index) const
	{
		check(Names.IsValidIndex(Index));
		return Names[Index].Value;
	}

	/** Gets enum name by index in Names array. Returns NAME_None if Index is not valid. */
	COREUOBJECT_API FName GetNameByIndex(int32 Index) const;

	/** Gets index of name in enum, returns INDEX_NONE and optionally errors when name is not found. This is faster than ByNameString if the FName is exact, but will fall back if needed */
	COREUOBJECT_API int32 GetIndexByName(FName InName, EGetByNameFlags Flags = EGetByNameFlags::None) const;

	/** Gets enum name by value. Returns NAME_None if value is not found. */
	COREUOBJECT_API FName GetNameByValue(int64 InValue) const;

	/** Gets enum value by name, returns INDEX_NONE and optionally errors when name is not found. This is faster than ByNameString if the FName is exact, but will fall back if needed */
	COREUOBJECT_API int64 GetValueByName(FName InName, EGetByNameFlags Flags = EGetByNameFlags::None) const;

	/** Returns the short name at the enum index, returns empty string if invalid */
	COREUOBJECT_API FString GetNameStringByIndex(int32 InIndex) const;

	/** Gets index of name in enum, returns INDEX_NONE and optionally errors when name is not found. Handles full or short names. */
	COREUOBJECT_API int32 GetIndexByNameString(const FString& SearchString, EGetByNameFlags Flags = EGetByNameFlags::None) const;

	/** Returns the short name matching the enum Value, returns empty string if invalid */
	COREUOBJECT_API FString GetNameStringByValue(int64 InValue) const;

	/** If the enumeration is declared as UENUM(Flags), returns a string of the form A | B | C representing set bits A, B, and C. If it is not a bitfield, the result is the same as calling GetNameStringByValue*/
	COREUOBJECT_API FString GetValueOrBitfieldAsString(int64 InValue) const;

	/** Looks for a name with a given value and returns true and writes the name to Out if one was found */
	COREUOBJECT_API bool FindNameStringByValue(FString& Out, int64 InValue) const;

	/** Gets enum value by name, returns INDEX_NONE and optionally errors when name is not found. Handles full or short names */
	COREUOBJECT_API int64 GetValueByNameString(const FString& SearchString, EGetByNameFlags Flags = EGetByNameFlags::None) const;

	/**
	 * Finds the localized display name or native display name as a fallback.
	 * If called from a cooked build this will normally return the short name as Metadata is not available.
	 *
	 * @param InIndex Index of the enum value to get Display Name for
	 *
	 * @return The display name for this object, or an empty text if Index is invalid
	 */
	COREUOBJECT_API virtual FText GetDisplayNameTextByIndex(int32 InIndex) const;

	/** Version of GetDisplayNameTextByIndex that takes a value instead */
	COREUOBJECT_API FText GetDisplayNameTextByValue(int64 InValue) const;

	/** Looks for a display name with a given value and returns true and writes the name to Out if one was found */
	COREUOBJECT_API bool FindDisplayNameTextByValue(FText& Out, int64 InValue) const;

	/**
	 * Returns the unlocalized logical name originally assigned to the enum at creation.
	 * By default this is the same as the short name but it is overridden in child classes with different internal name storage.
	 * This name is consistent in cooked and editor builds and is useful for things like external data import/export.
	 *
	 * @param InIndex Index of the enum value to get Display Name for
	 *
	 * @return The author-specified name, or an empty string if Index is invalid
	 */
	COREUOBJECT_API virtual FString GetAuthoredNameStringByIndex(int32 InIndex) const;

	/** Version of GetAuthoredNameByIndex that takes a value instead */
	COREUOBJECT_API FString GetAuthoredNameStringByValue(int64 InValue) const;

	/** Looks for a display name with a given value and returns true and writes the unlocalized logical name to Out if one was found */
	COREUOBJECT_API bool FindAuthoredNameStringByValue(FString& Out, int64 InValue) const;

	/** Gets max value of Enum. Defaults to zero if there are no entries. */
	COREUOBJECT_API int64 GetMaxEnumValue() const;

	/** Checks if enum has entry with given value. Includes autogenerated _MAX entry. */
	COREUOBJECT_API bool IsValidEnumValue(int64 InValue) const;

	/** Checks if enum has entry with given name. Includes autogenerated _MAX entry. */
	COREUOBJECT_API bool IsValidEnumName(FName InName) const;

	/** Removes the Names in this enum from the primary AllEnumNames list */
	UE_DEPRECATED(5.1, "RemoveNamesFromMasterList is deprecated, please use RemoveNamesFromPrimaryList instead.")
	void RemoveNamesFromMasterList()
	{
		RemoveNamesFromPrimaryList();
	}

	/** Removes the Names in this enum from the primary AllEnumNames list */
	COREUOBJECT_API void RemoveNamesFromPrimaryList();

	/** Try to update an out-of-date enum index after an enum changes at runtime */
	COREUOBJECT_API virtual int64 ResolveEnumerator(FArchive& Ar, int64 EnumeratorIndex) const;

	/** Associate a function for looking up Enum display names by index, only intended for use by generated code */
	void SetEnumDisplayNameFn(FEnumDisplayNameFn InEnumDisplayNameFn)
	{
		EnumDisplayNameFn = InEnumDisplayNameFn;
	}

	/**
	 * Returns the type of enum: whether it's a regular enum, namespaced enum or C++11 enum class.
	 *
	 * @return The enum type.
	 */
	ECppForm GetCppForm() const
	{
		return CppForm;
	}

	void SetEnumFlags(EEnumFlags FlagsToSet)
	{
		EnumFlags |= FlagsToSet;
	}

	bool HasAnyEnumFlags(EEnumFlags InFlags) const
	{
		return EnumHasAnyFlags(EnumFlags, InFlags);
	}

	/**
	 * Checks if a enum name is fully qualified name.
	 *
	 * @param InEnumName Name to check.
	 * @return true if the specified name is full enum name, false otherwise.
	 */
	static bool IsFullEnumName(const TCHAR* InEnumName)
	{
		return !!FCString::Strstr(InEnumName, TEXT("::"));
	}

	/**
	 * Generates full name including EnumName:: given enum name.
	 *
	 * @param InEnumName Enum name.
	 * @return Full enum name.
	 */
	COREUOBJECT_API virtual FString GenerateFullEnumName(const TCHAR* InEnumName) const;

	/**
	 * Searches the list of all enum value names for the specified name
	 * @param PackageName Package where the enum was defined (/Script/ModuleName or uasset package name in case of script defined enums). If an empty (None) name is specified the function will look through all enums to find a match
	 * @param TestName Fully qualified enum value name (EEnumName::ValueName)
	 * @Param Options Optional options that may limit search results or define ambiguous search result behavior
	 * @param OutFoundEnum Optional address of a variable where the resulting UEnum object should be stored
	 * @return The value the specified name represents if found, otherwise INDEX_NONE
	 */
	COREUOBJECT_API static int64 LookupEnumName(FName PackageName, FName TestName, EFindFirstObjectOptions Options = EFindFirstObjectOptions::None, UEnum** OutFoundEnum = nullptr);

	/** searches the list of all enum value names for the specified name
	 * @return the value the specified name represents if found, otherwise INDEX_NONE
	 */
	UE_DEPRECATED(5.1, "LookupEnumName that takes only enum name is deprecated. Please use the version of this function that also takes enum package name.")
	static int64 LookupEnumName(FName TestName, UEnum** FoundEnum = nullptr)
	{
		return LookupEnumName(FName(), TestName, EFindFirstObjectOptions::None, FoundEnum);
	}

	/** 
	 * Searches the list of all enum value names for the specified name
	 * @param PackageName Package where the enum was defined (/Script/ModuleName or uasset package name in case of script defined enums). If an empty (None) name is specified the function will look through all native enums to find a match
	 * @param InTestShortName Fully qualified or short enum value name (EEnumName::ValueName or ValueName)
	 * @Param Options Optional Options that may limit search results or define ambiguous search result behavior
	 * @param OutFoundEnum Optional address of a variable where the resulting UEnum object should be stored
	 * @return The value the specified name represents if found, otherwise INDEX_NONE
	 */
	COREUOBJECT_API static int64 LookupEnumNameSlow(FName PackageName, const TCHAR* InTestShortName, EFindFirstObjectOptions Options = EFindFirstObjectOptions::None, UEnum** OutFoundEnum = nullptr);

	/** searches the list of all enum value names for the specified name
	 * @return the value the specified name represents if found, otherwise INDEX_NONE
	 */
	UE_DEPRECATED(5.1, "LookupEnumNameSlow that takes only enum name is deprecated. Please use the version of this function that also takes enum package name.")
	static int64 LookupEnumNameSlow(const TCHAR* InTestShortName, UEnum** FoundEnum = nullptr)
	{
		return LookupEnumNameSlow(FName(), InTestShortName, EFindFirstObjectOptions::None, FoundEnum);
	}

	/** parses the passed in string for a name, then searches for that name in any Enum (in any package)
	 * @param Str	pointer to string to parse; if we successfully find an enum, this pointer is advanced past the name found
	 * @return index of the value the parsed enum name matches, or INDEX_NONE if no matches
	 */
	COREUOBJECT_API static int64 ParseEnum(const TCHAR*& Str);

	/**
	 * Tests if the enum contains a MAX value
	 *
	 * @return	true if the enum contains a MAX enum, false otherwise.
	 */
	COREUOBJECT_API bool ContainsExistingMax() const;

	/**
	 * Sets the array of enums.
	 *
	 * @param InNames List of enum names.
	 * @param InCppForm The form of enum.
	 * @param bAddMaxKeyIfMissing Should a default Max item be added.
	 * @return	true unless the MAX enum already exists and isn't the last enum.
	 */
	COREUOBJECT_API virtual bool SetEnums(TArray<TPair<FName, int64>>& InNames, ECppForm InCppForm, EEnumFlags InFlags = EEnumFlags::None, bool bAddMaxKeyIfMissing = true);

	/**
	 * @return	 The number of enum names.
	 */
	int32 NumEnums() const
	{
		return Names.Num();
	}

	/**
	 * Find the longest common prefix of all items in the enumeration.
	 * 
	 * @return	the longest common prefix between all items in the enum.  If a common prefix
	 *			cannot be found, returns the full name of the enum.
	 */
	COREUOBJECT_API FString GenerateEnumPrefix() const;

#if WITH_EDITOR
	/**
	 * Finds the localized tooltip or native tooltip as a fallback.
	 *
	 * @param NameIndex Index of the enum value to get tooltip for
	 *
	 * @return The tooltip for this object.
	 */
	COREUOBJECT_API FText GetToolTipTextByIndex(int32 NameIndex) const;
#endif

#if WITH_EDITORONLY_DATA
	/**
	 * Wrapper method for easily determining whether this enum has metadata associated with it.
	 * 
	 * @param	Key			the metadata tag to check for
	 * @param	NameIndex	if specified, will search for metadata linked to a specified value in this enum; otherwise, searches for metadata for the enum itself
	 *
	 * @return true if the specified key exists in the list of metadata for this enum, even if the value of that key is empty
	 */
	COREUOBJECT_API bool HasMetaData( const TCHAR* Key, int32 NameIndex=INDEX_NONE ) const;

	/**
	 * Return the metadata value associated with the specified key.
	 * 
	 * @param	Key			the metadata tag to find the value for
	 * @param	NameIndex	if specified, will search the metadata linked for that enum value; otherwise, searches the metadata for the enum itself
	 * @param	bAllowRemap	if true, the returned value may be remapped from a .ini if the value starts with ini: Pass false when you need the exact string, including any ini:
	 *
	 * @return	the value for the key specified, or an empty string if the key wasn't found or had no value.
	 */
	COREUOBJECT_API FString GetMetaData( const TCHAR* Key, int32 NameIndex=INDEX_NONE, bool bAllowRemap=true ) const;

	/**
	 * Set the metadata value associated with the specified key.
	 * 
	 * @param	Key			the metadata tag to find the value for
	 * @param	NameIndex	if specified, will search the metadata linked for that enum value; otherwise, searches the metadata for the enum itself
	 * @param	InValue		Value of the metadata for the key
	 *
	 */
	COREUOBJECT_API void SetMetaData( const TCHAR* Key, const TCHAR* InValue, int32 NameIndex=INDEX_NONE) const;
	
	/**
	 * Remove given key meta data
	 * 
	 * @param	Key			the metadata tag to find the value for
	 * @param	NameIndex	if specified, will search the metadata linked for that enum value; otherwise, searches the metadata for the enum itself
	 *
	 */
	COREUOBJECT_API void RemoveMetaData( const TCHAR* Key, int32 NameIndex=INDEX_NONE ) const;
#endif // WITH_EDITORONLY_DATA
	
	/**
	 * @param EnumPath         Full enum path.
	 * @param EnumeratorValue  Enumerator VAlue.
	 *
	 * @return the string associated with the enumerator for the specified enum value for the enum specified by a path.
	 */
	template <typename T>
	FORCEINLINE static FString GetValueAsString( const TCHAR* EnumPath, const T EnumeratorValue)
	{
		// For the C++ enum.
		static_assert(TIsEnum<T>::Value, "Should only call this with enum types");
		return GetValueAsString_Internal(EnumPath, (int64)EnumeratorValue);
	}

	template <typename T>
	FORCEINLINE static FString GetValueAsString( const TCHAR* EnumPath, const TEnumAsByte<T> EnumeratorValue)
	{
		return GetValueAsString_Internal(EnumPath, (int64)EnumeratorValue.GetValue());
	}

	template< class T >
	FORCEINLINE static void GetValueAsString( const TCHAR* EnumPath, const T EnumeratorValue, FString& out_StringValue )
	{
		out_StringValue = GetValueAsString( EnumPath, EnumeratorValue );
	}

	template <typename T>
	FORCEINLINE static FString GetValueOrBitfieldAsString(const TCHAR* EnumPath, const T EnumeratorValue)
	{
		// For the C++ enum.
		static_assert(TIsEnum<T>::Value, "Should only call this with enum types");
		return GetValueOrBitfieldAsString_Internal(EnumPath, (int64)EnumeratorValue);
	}

	template <typename T>
	FORCEINLINE static FString GetValueOrBitfieldAsString(const TCHAR* EnumPath, const TEnumAsByte<T> EnumeratorValue)
	{
		return GetValueOrBitfieldAsString_Internal(EnumPath, (int64)EnumeratorValue.GetValue());
	}

	template< class T >
	FORCEINLINE static void GetValueOrBitfieldAsString(const TCHAR* EnumPath, const T EnumeratorValue, FString& out_StringValue)
	{
		out_StringValue = GetValueOrBitfieldAsString( EnumPath, EnumeratorValue );
	}
	/**
	 * @param EnumPath         Full enum path.
	 * @param EnumeratorValue  Enumerator Value.
	 *
	 * @return the localized display string associated with the specified enum value for the enum specified by a path
	 */
	template <typename T>
	FORCEINLINE static FText GetDisplayValueAsText( const TCHAR* EnumPath, const T EnumeratorValue )
	{
		// For the C++ enum.
		static_assert(TIsEnum<T>::Value, "Should only call this with enum types");
		return GetDisplayValueAsText_Internal(EnumPath, (int64)EnumeratorValue);
	}

	template <typename T>
	FORCEINLINE static FText GetDisplayValueAsText( const TCHAR* EnumPath, const TEnumAsByte<T> EnumeratorValue)
	{
		return GetDisplayValueAsText_Internal(EnumPath, (int64)EnumeratorValue.GetValue());
	}

	template< class T >
	FORCEINLINE static void GetDisplayValueAsText( const TCHAR* EnumPath, const T EnumeratorValue, FText& out_TextValue )
	{
		out_TextValue = GetDisplayValueAsText( EnumPath, EnumeratorValue);
	}

	/**
	 * @param EnumeratorValue  Enumerator Value.
	 *
	 * @return the name associated with the enumerator for the specified enum value for the enum specified by the template type.
	 */
	template<typename EnumType>
	FORCEINLINE static FName GetValueAsName(const EnumType EnumeratorValue)
	{
		// For the C++ enum.
		static_assert(TIsEnum<EnumType>::Value, "Should only call this with enum types");
		UEnum* EnumClass = StaticEnum<EnumType>();
		check(EnumClass != nullptr);
		return EnumClass->GetNameByValue((int64)EnumeratorValue);
	}

	template<typename EnumType>
	FORCEINLINE static FName GetValueAsName(const TEnumAsByte<EnumType> EnumeratorValue)
	{
		return GetValueAsName(EnumeratorValue.GetValue());
	}

	template<typename EnumType>
	FORCEINLINE static void GetValueAsName(const EnumType EnumeratorValue, FName& out_NameValue )
	{
		out_NameValue = GetValueAsName(EnumeratorValue);
	}

	/**
	 * @param EnumeratorValue  Enumerator Value.
	 *
	 * @return the string associated with the enumerator for the specified enum value for the enum specified by the template type.
	 */
	template<typename EnumType>
	FORCEINLINE static FString GetValueAsString(const EnumType EnumeratorValue)
	{
		// For the C++ enum.
		static_assert(TIsEnum<EnumType>::Value, "Should only call this with enum types");
		return GetValueAsName(EnumeratorValue).ToString();
	}

	template<typename EnumType>
	FORCEINLINE static FString GetValueAsString(const TEnumAsByte<EnumType> EnumeratorValue)
	{
		return GetValueAsString(EnumeratorValue.GetValue());
	}

	template<typename EnumType>
	FORCEINLINE static void GetValueAsString(const EnumType EnumeratorValue, FString& out_StringValue )
	{
		out_StringValue = GetValueAsString(EnumeratorValue );
	}

	/**
	 * @param EnumeratorValue  Enumerator Value.
	 *
	 * @return a string representing the combination of set bits in the bitfield or, if not a bitfield/flags enumeration, the string associated with the enumerator for the specified enum value for the enum specified by the template type.
	 */
	template<
		typename EnumType
		UE_REQUIRES(TIsEnum<EnumType>::Value)
	>
	FORCEINLINE static FString GetValueOrBitfieldAsString(const EnumType EnumeratorValue)
	{
		// For the C++ enum.
		static_assert(TIsEnum<EnumType>::Value, "Should only call this with enum types");
		UEnum* EnumClass = StaticEnum<EnumType>();
		check(EnumClass != nullptr);
		return EnumClass->GetValueOrBitfieldAsString((int64)EnumeratorValue);
	}

	// TEnumAsByte produces a warning if you use it with EnumClass, so this UE_REQUIRES keeps this overload
	// from being matched in that case
	template<
		typename EnumType
		UE_REQUIRES(!TIsEnumClass<EnumType>::Value)
	>
	FORCEINLINE static FString GetValueOrBitfieldAsString(const TEnumAsByte<EnumType> EnumeratorValue)
	{
		return GetValueOrBitfieldAsString(EnumeratorValue.GetValue());
	}

	template<
		typename EnumType,
		typename IntegralType
		UE_REQUIRES(TIsEnum<EnumType>::Value && std::is_integral_v<IntegralType>)
	>
	FORCEINLINE static FString GetValueOrBitfieldAsString(const IntegralType EnumeratorValue)
	{
		// For the C++ enum.
		static_assert(TIsEnum<EnumType>::Value, "Should only call this with enum types");
		UEnum* EnumClass = StaticEnum<EnumType>();
		check(EnumClass != nullptr);
		return EnumClass->GetValueOrBitfieldAsString((int64)EnumeratorValue);
	}

	template<typename EnumType>
	FORCEINLINE static void GetValueOrBitfieldAsString(const EnumType EnumeratorValue, FString& out_StringValue)
	{
		out_StringValue = GetValueOrBitfieldAsString(EnumeratorValue);
	}

	template<
		typename EnumType,
		typename IntegralType
		UE_REQUIRES(TIsEnum<EnumType>::Value && std::is_integral_v<IntegralType>)
	>
	FORCEINLINE static void GetValueOrBitfieldAsString(const IntegralType EnumeratorValue, FString& out_StringValue)
	{
		out_StringValue = GetValueOrBitfieldAsString<EnumType>(EnumeratorValue);
	}

	/**
	 * @param EnumeratorValue  Enumerator Value.
	 *
	 * @return the localized display string associated with the specified enum value for the enum specified by the template type.
	 */
	template<typename EnumType>
	FORCEINLINE static FText GetDisplayValueAsText(const EnumType EnumeratorValue )
	{
		// For the C++ enum.
		static_assert(TIsEnum<EnumType>::Value, "Should only call this with enum types");
		UEnum* EnumClass = StaticEnum<EnumType>();
		check(EnumClass != nullptr);
		return EnumClass->GetDisplayNameTextByValue((int64)EnumeratorValue);
	}

	template<typename EnumType>
	FORCEINLINE static FText GetDisplayValueAsText(const TEnumAsByte<EnumType> EnumeratorValue)
	{
		return GetDisplayValueAsText(EnumeratorValue.GetValue());
	}

	template<typename EnumType>
	FORCEINLINE static void GetDisplayValueAsText(const EnumType EnumeratorValue, FText& out_TextValue )
	{
		out_TextValue = GetDisplayValueAsText(EnumeratorValue);
	}

	// UObject interface.
	COREUOBJECT_API virtual void Serialize(FArchive& Ar) override;
	COREUOBJECT_API virtual void BeginDestroy() override;
	// End of UObject interface.

protected:
	/** List of pairs of all enum names and values. */
	TArray<TPair<FName, int64>> Names;

	/** How the enum was originally defined. */
	ECppForm CppForm;

	/** Enum flags. */
	EEnumFlags EnumFlags;

	/** pointer to function used to look up the enum's display name. Currently only assigned for UEnums generated for nativized blueprints */
	FEnumDisplayNameFn EnumDisplayNameFn;

	/** Package name this enum was in when its names were being added to the primary list */
	FName EnumPackage;

	/** lock to be taken when accessing AllEnumNames */
	static FRWLock AllEnumNamesLock;

	/** global list of all value names used by all enums in memory, used for property text import */
	static TMap<FName, TMap<FName, UEnum*> > AllEnumNames;

	/** adds the Names in this enum to the primary AllEnumNames list */
	UE_DEPRECATED(5.1, "AddNamesToMasterList is deprecated, please use AddNamesToPrimaryList instead.")
	void AddNamesToMasterList()
	{
		AddNamesToPrimaryList();
	}

	/** adds the Names in this enum to the primary AllEnumNames list */
	COREUOBJECT_API void AddNamesToPrimaryList();

private:

	/**
	 * Searches the list of all enum value names and returns a UEnum object that contains an enum value name matching the provided comparison function criteria
	 * @param PackageName Package where the enum was defined (/Script/ModuleName or uasset package name in case of script defined enums). If an empty (None) name is specified the function will look through all native enums to find a match
	 * @Param Options Options that may limit search results or define ambiguous search result behavior
	 * @param CompareNameFunction Functions used to compare the existing enum value names with the one that should be found
	 * @return UEnum object if a matching enum value name was found otherwise nullptr
	 */
	static UEnum* LookupAllEnumNamesWithOptions(FName PackageName, EFindFirstObjectOptions Options, TFunctionRef<bool(FName)> CompareNameFunction);

	FORCEINLINE static FString GetValueAsString_Internal( const TCHAR* EnumPath, const int64 EnumeratorValue)
	{
		UEnum* EnumClass = FindObject<UEnum>( nullptr, EnumPath );
		UE_CLOG( !EnumClass, LogClass, Fatal, TEXT("Couldn't find enum '%s'"), EnumPath );
		return EnumClass->GetNameStringByValue(EnumeratorValue);
	}

	FORCEINLINE static FString GetValueOrBitfieldAsString_Internal(const TCHAR* EnumPath, const int64 EnumeratorValue)
	{
		UEnum* EnumClass = FindObject<UEnum>(nullptr, EnumPath);
		UE_CLOG(!EnumClass, LogClass, Fatal, TEXT("Couldn't find enum '%s'"), EnumPath);
		return EnumClass->GetValueOrBitfieldAsString(EnumeratorValue);
	}

	FORCEINLINE static FText GetDisplayValueAsText_Internal( const TCHAR* EnumPath, const int64 EnumeratorValue )
	{
		UEnum* EnumClass = FindObject<UEnum>(nullptr, EnumPath);
		UE_CLOG(!EnumClass, LogClass, Fatal, TEXT("Couldn't find enum '%s'"), EnumPath);
		return EnumClass->GetDisplayNameTextByValue(EnumeratorValue);
	}

	/**
	 * Renames enum values to use duplicated enum name instead of base one, e.g.:
	 * 
	 * MyEnum::MyVal
	 * MyEnum::MyEnum_MAX
	 * 
	 * becomes
	 * 
	 * MyDuplicatedEnum::MyVal
	 * MyDuplicatedEnum::MyDuplicatedEnum_MAX
	 */
	void RenameNamesAfterDuplication();

	/** Gets name of enum "this" is duplicate of. If we're not duplicating, just returns "this" name. */
	FString GetBaseEnumNameOnDuplication() const;
};

/*-----------------------------------------------------------------------------
	UClass.
-----------------------------------------------------------------------------*/

/** Base definition for C++ class type traits */
struct FCppClassTypeTraitsBase
{
	enum
	{
		IsAbstract = false
	};
};


/** Defines traits for specific C++ class types */
template<class CPPCLASS>
struct TCppClassTypeTraits : public FCppClassTypeTraitsBase
{
	enum
	{
		IsAbstract = TIsAbstract<CPPCLASS>::Value
	};
};


/** Interface for accessing attributes of the underlying C++ class, for native class types */
struct ICppClassTypeInfo
{
	/** Return true if the underlying C++ class is abstract (i.e. declares at least one pure virtual function) */
	virtual bool IsAbstract() const = 0;
};


struct FCppClassTypeInfoStatic
{
	bool bIsAbstract;
};


/** Implements the type information interface for specific C++ class types */
struct FCppClassTypeInfo : ICppClassTypeInfo
{
	explicit FCppClassTypeInfo(const FCppClassTypeInfoStatic* InInfo)
		: Info(InInfo)
	{
	}

	// Non-copyable
	FCppClassTypeInfo(const FCppClassTypeInfo&) = delete;
	FCppClassTypeInfo& operator=(const FCppClassTypeInfo&) = delete;

	// ICppClassTypeInfo implementation
	virtual bool IsAbstract() const override
	{
		return Info->bIsAbstract;
	}

private:
	const FCppClassTypeInfoStatic* Info;
};


/** information about an interface a class implements */
struct FImplementedInterface
{
	/** the interface class */
	TObjectPtr<UClass> Class;
	/** the pointer offset of the interface's vtable */
	int32 PointerOffset;
	/** whether or not this interface has been implemented via K2 */
	bool bImplementedByK2;

	FImplementedInterface()
		: Class(nullptr)
		, PointerOffset(0)
		, bImplementedByK2(false)
	{}
	FImplementedInterface(UClass* InClass, int32 InOffset, bool InImplementedByK2)
		: Class(InClass)
		, PointerOffset(InOffset)
		, bImplementedByK2(InImplementedByK2)
	{}

	friend COREUOBJECT_API FArchive& operator<<(FArchive& Ar, FImplementedInterface& A);
};


/** A struct that maps a string name to a native function */
struct FNativeFunctionLookup
{
	FName Name;
	FNativeFuncPtr Pointer;

	FNativeFunctionLookup(FName InName, FNativeFuncPtr InPointer)
		:	Name(InName)
		,	Pointer(InPointer)
	{}
};


namespace EIncludeSuperFlag
{
	enum Type
	{
		ExcludeSuper,
		IncludeSuper
	};
}

struct FClassFunctionLinkInfo
{
	UFunction* (*CreateFuncPtr)();
	const char* FuncNameUTF8;
};


enum class EGetSparseClassDataMethod : uint8
{
	/** Create a new instance when this class doesn't have any sparse data of its own */
	CreateIfNull,
	/** Use the archetype instance (if possible) when this class doesn't have any sparse data of its own */
	ArchetypeIfNull,
	/** Return null when this class doesn't have any sparse data of its own */
	ReturnIfNull,
};


/**
 * An object class.
 */
class UClass : public UStruct
{
	DECLARE_CASTED_CLASS_INTRINSIC_NO_CTOR(UClass, UStruct, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UClass, COREUOBJECT_API)
	DECLARE_WITHIN_UPACKAGE()

public:
	friend class FRestoreClassInfo;
	friend class FBlueprintEditorUtils;
	friend class FBlueprintCompileReinstancer;

	typedef void		(*ClassConstructorType)				(const FObjectInitializer&);
	typedef UObject*	(*ClassVTableHelperCtorCallerType)	(FVTableHelper& Helper);
	typedef UClass* (*StaticClassFunctionType)();

	ClassConstructorType ClassConstructor;
	ClassVTableHelperCtorCallerType ClassVTableHelperCtorCaller;
	FUObjectCppClassStaticFunctions CppClassStaticFunctions;

	/** Class pseudo-unique counter; used to accelerate unique instance name generation */
	mutable int32 ClassUnique;

	/** Index of the first ClassRep that belongs to this class. Anything before that was defined by / belongs to parent classes. */
	int32 FirstOwnedClassRep = 0;

	/** Used to check if the class was cooked or not */
	bool bCooked;

	/** Used to check if the class layout is currently changing and therefore is not ready for a CDO to be created */
	bool bLayoutChanging;

	/** Class flags; See EClassFlags for more information */
	EClassFlags ClassFlags;

	/** Cast flags used to accelerate dynamic_cast<T*> on objects of this type for common T */
	EClassCastFlags ClassCastFlags;

	/** The required type for the outer of instances of this class */
	TObjectPtr<UClass> ClassWithin;

#if WITH_EDITORONLY_DATA
	/** This is the blueprint that caused the generation of this class, or null if it is a native compiled-in class */
	TObjectPtr<UObject> ClassGeneratedBy;

	/** Linked list of properties to be destroyed when this class is destroyed that couldn't be destroyed in PurgeClass **/
	FField* PropertiesPendingDestruction;

	/** Destroys properties that couldn't be destroyed in PurgeClass */
	COREUOBJECT_API void DestroyPropertiesPendingDestruction();
#endif

#if WITH_EDITOR
	/**
	 * Conditionally recompiles the class after loading, in case any dependencies were also newly loaded
	 * @param ObjLoaded	If set this is the list of objects that are currently loading, usualy GObjLoaded
	 */
	virtual void ConditionalRecompileClass(FUObjectSerializeContext* InLoadContext) {}
	virtual void FlushCompilationQueueForLevel() {}
#endif //WITH_EDITOR

	/** Which Name.ini file to load Config variables out of */
	FName ClassConfigName;

	/** List of replication records */
	TArray<FRepRecord> ClassReps;

	/** List of network relevant fields (functions) */
	TArray<UField*> NetFields;

#if WITH_EDITOR
	// Editor only properties
	COREUOBJECT_API void GetHideFunctions(TArray<FString>& OutHideFunctions) const;
	COREUOBJECT_API bool IsFunctionHidden(const TCHAR* InFunction) const;
	COREUOBJECT_API void GetAutoExpandCategories(TArray<FString>& OutAutoExpandCategories) const;
	COREUOBJECT_API bool IsAutoExpandCategory(const TCHAR* InCategory) const;
	COREUOBJECT_API void GetPrioritizeCategories(TArray<FString>& OutPrioritizedCategories) const;
	COREUOBJECT_API bool IsPrioritizeCategory(const TCHAR* InCategory) const;
	COREUOBJECT_API void GetAutoCollapseCategories(TArray<FString>& OutAutoCollapseCategories) const;
	COREUOBJECT_API bool IsAutoCollapseCategory(const TCHAR* InCategory) const;
	COREUOBJECT_API void GetClassGroupNames(TArray<FString>& OutClassGroupNames) const;
	COREUOBJECT_API bool IsClassGroupName(const TCHAR* InGroupName) const;
#endif
	/**
	 * Calls AddReferencedObjects static method on the specified object.
	 *
	 * @param This Object to call ARO on.
	 * @param Collector Reference collector.
	 */
	FORCEINLINE void CallAddReferencedObjects(UObject* This, FReferenceCollector& Collector) const
	{
		// The object must of this class type.
		check(This->IsA(this)); 
		// This should always be set to something, at the very least to UObject::ARO
		check(CppClassStaticFunctions.GetAddReferencedObjects() != nullptr);
		CppClassStaticFunctions.GetAddReferencedObjects()(This, Collector);
	}

#if WITH_EDITORONLY_DATA
	/** Calls the c++ class's DeclareCustomVersions static function (from the nearest native parent if this is not native) */
	void CallDeclareCustomVersions(FArchive& Ar) const
	{
		check(CppClassStaticFunctions.GetDeclareCustomVersions());
		CppClassStaticFunctions.GetDeclareCustomVersions()(Ar, this);
	}

	/** Calls the c++ class's AppendToClassSchema static function */
	void CallAppendToClassSchema(FAppendToClassSchemaContext& Context) const
	{
		check(CppClassStaticFunctions.GetAppendToClassSchema());
		CppClassStaticFunctions.GetAppendToClassSchema()(Context);
	}

	/** Calls the c++ class's DeclareConstructClasses static function (from the nearest native parent if this is not native) */
	void CallDeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses) const
	{
		check(CppClassStaticFunctions.GetDeclareConstructClasses());
		CppClassStaticFunctions.GetDeclareConstructClasses()(OutConstructClasses, this);
	}
#endif

	/** The class default object; used for delta serialization and object initialization */
	TObjectPtr<UObject> ClassDefaultObject;

protected:
	/** This is where we store the data that is only changed per class instead of per instance */
	void* SparseClassData;

	/** The struct used to store sparse class data. */
	TObjectPtr<UScriptStruct> SparseClassDataStruct;

public:
	/**
	 * Returns a pointer to the sidecar data structure, based on the EGetSparseClassDataMethod.
	 * @note It is only safe to mutate this data when using "CreateIfNull", as others may return archetype/default data; consider GetOrCreateSparseClassData for this use-case.
	 */
	COREUOBJECT_API const void* GetSparseClassData(const EGetSparseClassDataMethod GetMethod);

	/**
	 * Returns a pointer to the sidecar data structure. This function will create an instance of the data structure if one has been specified and it has not yet been created.
	 */
	void* GetOrCreateSparseClassData() { return const_cast<void*>(GetSparseClassData(EGetSparseClassDataMethod::CreateIfNull)); }

	/**
	 * Returns a pointer to the type of the sidecar data structure if one is specified.
	 */
	COREUOBJECT_API UScriptStruct* GetSparseClassDataStruct() const;

	COREUOBJECT_API void SetSparseClassDataStruct(UScriptStruct* InSparseClassDataStruct);

	/** 
	 * Clears the sparse class data struct for this and all child classes that directly reference it as a super-struct 
	 * This will rename the current sparse class data struct aside into the transient package
	 */
	COREUOBJECT_API void ClearSparseClassDataStruct(bool bInRecomplingOnLoad);

	/** Assemble reference token streams for all classes if they haven't had it assembled already */
	static COREUOBJECT_API void AssembleReferenceTokenStreams();

#if WITH_EDITOR
	void GenerateFunctionList(TArray<FName>& OutArray) const 
	{ 
		FUClassFuncScopeReadLock ScopeLock(FuncMapLock);
		FuncMap.GenerateKeyArray(OutArray); 
	}
#endif // WITH_EDITOR

protected:
	COREUOBJECT_API void* CreateSparseClassData();

	COREUOBJECT_API void CleanupSparseClassData();

private:
#if WITH_EDITOR
	/** Provides access to attributes of the underlying C++ class. Should never be unset. */
	TOptional<FCppClassTypeInfo> CppTypeInfo;
#endif

	/** Map of all functions by name contained in this class */
	TMap<FName, TObjectPtr<UFunction>> FuncMap;

	/** Scope lock to avoid the FuncMap being read and written to simultaneously on multiple threads. */
	mutable FUClassFuncLock FuncMapLock;

	/** A cache of all functions by name that exist in this class or a parent (superclass or interface) context */
	mutable TMap<FName, UFunction*> AllFunctionsCache;

	/** Scope lock to avoid the SuperFuncMap being read and written to simultaneously on multiple threads. */
	mutable FUClassFuncLock AllFunctionsCacheLock;

public:
	/**
	 * The list of interfaces which this class implements, along with the pointer property that is located at the offset of the interface's vtable.
	 * If the interface class isn't native, the property will be null.
	 */
	TArray<FImplementedInterface> Interfaces;

	/** GC schema, finalized in AssembleReferenceTokenStream */
	UE::GC::FSchemaOwner ReferenceSchema;

	/** This class's native functions. */
	TArray<FNativeFunctionLookup> NativeFunctionLookupTable;

public:
	// Constructors
	COREUOBJECT_API UClass(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	COREUOBJECT_API explicit UClass(const FObjectInitializer& ObjectInitializer, UClass* InSuperClass);
	COREUOBJECT_API UClass( EStaticConstructor, FName InName, uint32 InSize, uint32 InAlignment, EClassFlags InClassFlags, EClassCastFlags InClassCastFlags,
		const TCHAR* InClassConfigName, EObjectFlags InFlags, ClassConstructorType InClassConstructor,
		ClassVTableHelperCtorCallerType InClassVTableHelperCtorCaller,
		FUObjectCppClassStaticFunctions&& InCppClassStaticFunctions);

#if WITH_RELOAD
	/**
	 * Called when a class is reloading from a DLL...updates various information in-place.
	 * @param	InSize							sizeof the class
	 * @param	InClassFlags					Class flags for the class
	 * @param	InClassCastFlags				Cast Flags for the class
	 * @param	InConfigName					Config Name
	 * @param	InClassConstructor				Pointer to InternalConstructor<TClass>
	 * @param	TClass_Super_StaticClass		Static class of the super class
	 * @param	TClass_WithinClass_StaticClass	Static class of the WithinClass
	 */
	COREUOBJECT_API bool HotReloadPrivateStaticClass(
		uint32			InSize,
		EClassFlags		InClassFlags,
		EClassCastFlags	InClassCastFlags,
		const TCHAR*    InConfigName,
		ClassConstructorType InClassConstructor,
		ClassVTableHelperCtorCallerType InClassVTableHelperCtorCaller,
		FUObjectCppClassStaticFunctions&& InCppClassStaticFunctions,
		class UClass* TClass_Super_StaticClass,
		class UClass* TClass_WithinClass_StaticClass
		);


	/**
	* Replace a native function in the  internal native function table
	* @param	InName							name of the function
	* @param	InPointer						pointer to the function
	* @param	bAddToFunctionRemapTable		For C++ hot-reloading, UFunctions are patched in a deferred manner and this should be true
	*											For script hot-reloading, script integrations may have a many to 1 mapping of UFunction to native pointer
	*											because dispatch is shared, so the C++ remap table does not work in this case, and this should be false
	* @return	true if the function was found and replaced, false if it was not
	*/
	COREUOBJECT_API bool ReplaceNativeFunction(FName InName, FNativeFuncPtr InPointer, bool bAddToFunctionRemapTable);
#endif

	/**
	 * If there are potentially multiple versions of this class (e.g. blueprint generated classes), this function will return the authoritative version, which should be used for references
	 *
	 * @return The version of this class that references should be stored to
	 */
	COREUOBJECT_API virtual UClass* GetAuthoritativeClass();
	const UClass* GetAuthoritativeClass() const { return const_cast<UClass*>(this)->GetAuthoritativeClass(); }

	/**
	 * Add a native function to the internal native function table
	 * @param	InName							name of the function
	 * @param	InPointer						pointer to the function
	 */
	COREUOBJECT_API void AddNativeFunction(const ANSICHAR* InName, FNativeFuncPtr InPointer);

	/**
	 * Add a native function to the internal native function table, but with a unicode name. Used when generating code from blueprints, 
	 * which can have unicode identifiers for functions and properties.
	 * @param	InName							name of the function
	 * @param	InPointer						pointer to the function
	 */
	COREUOBJECT_API void AddNativeFunction(const WIDECHAR* InName, FNativeFuncPtr InPointer);

	/** Add a function to the function map */
	void AddFunctionToFunctionMap(UFunction* Function, FName FuncName)
	{
		{
			FUClassFuncScopeWriteLock ScopeLock(FuncMapLock);
			FuncMap.Add(FuncName, Function);
		}
		{
			// Remove from the function cache if it exists
			FUClassFuncScopeWriteLock ScopeLock(AllFunctionsCacheLock);
			AllFunctionsCache.Remove(FuncName);
		}
	}

	COREUOBJECT_API void CreateLinkAndAddChildFunctionsToMap(const FClassFunctionLinkInfo* Functions, uint32 NumFunctions);

	/** Remove a function from the function map */
	void RemoveFunctionFromFunctionMap(UFunction* Function)
	{
		{
			FUClassFuncScopeWriteLock ScopeLock(FuncMapLock);
			FuncMap.Remove(Function->GetFName());
		}
		{
			// Remove from the function cache if it exists
			FUClassFuncScopeWriteLock ScopeLock(AllFunctionsCacheLock);
			AllFunctionsCache.Remove(Function->GetFName());
		}
	}

	/** Clears the function name caches, in case things have changed */
	COREUOBJECT_API void ClearFunctionMapsCaches();

	/** Looks for a given function name */
	COREUOBJECT_API UFunction* FindFunctionByName(FName InName, EIncludeSuperFlag::Type IncludeSuper = EIncludeSuperFlag::IncludeSuper) const;

	// UObject interface.
	COREUOBJECT_API virtual void Serialize(FArchive& Ar) override;
	COREUOBJECT_API virtual void PostLoad() override;
	COREUOBJECT_API virtual void FinishDestroy() override;
	COREUOBJECT_API virtual void DeferredRegister(UClass *UClassStaticClass,const TCHAR* PackageName,const TCHAR* InName) override;
	COREUOBJECT_API virtual bool Rename(const TCHAR* NewName = nullptr, UObject* NewOuter = nullptr, ERenameFlags Flags = REN_None) override;
	COREUOBJECT_API virtual void TagSubobjects(EObjectFlags NewFlags) override;
	COREUOBJECT_API virtual void PostInitProperties() override;
	static COREUOBJECT_API void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	COREUOBJECT_API virtual FRestoreForUObjectOverwrite* GetRestoreForUObjectOverwrite() override;
	COREUOBJECT_API virtual FString GetDesc() override;
	COREUOBJECT_API virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	UE_DEPRECATED(5.4, "Implement the version that takes FAssetRegistryTagsContext instead.")
	COREUOBJECT_API virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
#if WITH_EDITOR
	COREUOBJECT_API virtual void PostLoadAssetRegistryTags(const FAssetData& InAssetData, TArray<FAssetRegistryTag>& OutTagsAndValuesToUpdate) const;
#endif // WITH_EDITOR
	virtual bool IsAsset() const override { return false; }	
	virtual bool IsNameStableForNetworking() const override { return true; } // For now, assume all classes have stable net names
	COREUOBJECT_API virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
	// End of UObject interface.

	// UField interface.
	COREUOBJECT_API virtual void Bind() override;
	COREUOBJECT_API virtual const TCHAR* GetPrefixCPP() const override;
	// End of UField interface.

	// UStruct interface.
	COREUOBJECT_API virtual void Link(FArchive& Ar, bool bRelinkExistingProperties) override;
	COREUOBJECT_API virtual void SetSuperStruct(UStruct* NewSuperStruct) override;
	COREUOBJECT_API virtual bool IsStructTrashed() const override;
	// End of UStruct interface.

	/**
	 * Returns class path name as a package + class FName pair
	 */
	FORCEINLINE FTopLevelAssetPath GetClassPathName() const
	{
		return GetStructPathName();
	}

	/**
	 * Utility function that tries to find a type (class/struct/enum) given a path name or a short name however it will throw a warning (with a callstack) if it's the latter.
	 * Useful for tracking down and fixing remaining places that use short class names.
	 * This function is slow and should not be used in performance critical situations
	 * @param TypeClass Class of the expected type. Must be a subclass of UField
	 * @param InShortNameOrPathName Class name
	 * @return Pointer to a type object if successful, nullptr otherwise
	 */
	static COREUOBJECT_API UField* TryFindTypeSlow(UClass* TypeClass, const FString& InPathNameOrShortName, EFindFirstObjectOptions InOptions = EFindFirstObjectOptions::None);

	/**
	 * Utility function that tries to find a type (class/struct/enum) given a path name or a short name however it will throw a warning (with a callstack) if it's the latter.
	 * Useful for tracking down and fixing remaining places that use short class names.
	 * This function is slow and should not be used in performance critical situations
	 * @param InShortNameOrPathName Class name
	 * @return Pointer to a type object if successful, false otherwise
	 */
	template <typename T>
	static T* TryFindTypeSlow(const FString& InShortNameOrPathName, EFindFirstObjectOptions InOptions = EFindFirstObjectOptions::None)
	{
		return (T*)TryFindTypeSlow(T::StaticClass(), InShortNameOrPathName, InOptions);
	}

	/**
	 * Utility function that tries to find a type (class/struct/enum) given a path name or a short name however it will throw a warning (with a callstack) if it's the latter.
	 * Useful for tracking down and fixing remaining places that use short class names.
	 * This function is slow and should not be used in performance critical situations
	 * This version of TryFindType will not assert when saving package or when collecting garbage
	 * @param TypeClass Class of the expected type. Must be a subclass of UField
	 * @param InShortNameOrPathName Class name
	 * @return Pointer to a type object if successful, nullptr otherwise
	 */
	static COREUOBJECT_API UField* TryFindTypeSlowSafe(UClass* TypeClass, const FString& InPathNameOrShortName, EFindFirstObjectOptions InOptions = EFindFirstObjectOptions::None);

	/**
	 * Utility function that tries to find a type (class/struct/enum) given a path name or a short name however it will throw a warning (with a callstack) if it's the latter.
	 * Useful for tracking down and fixing remaining places that use short class names.
	 * This function is slow and should not be used in performance critical situations
	 * This version of TryFindType will not assert when saving package or when collecting garbage
	 * @param InShortNameOrPathName Class name
	 * @return Pointer to a type object if successful, nullptr otherwise
	 */
	template <typename T>
	static T* TryFindTypeSlowSafe(const FString& InShortNameOrPathName, EFindFirstObjectOptions InOptions = EFindFirstObjectOptions::None)
	{
		return (T*)TryFindTypeSlowSafe(T::StaticClass(), InShortNameOrPathName, InOptions);
	}

	/**
	 * Tries to convert short class name to class path name. This will only work if the class exists in memory
	 * This function is slow and should not be used in performance critical situations
	 * @param TypeClass Expected Class of the type that needs to be converted (UEnum / UStruct / UClass)
	 * @param InShortClassName Short class name. If this parameter is already a path name then it's going to be returned as FTopLevelAssetPath
	 * @param AmbiguousMessageVerbosity Verbosity with which to log a message if class name is ambiguous 
	 * @param AmbiguousClassMessage Additional message to log when class name is ambiguous (e.g. current operation) 
	 * @return Class path name if successful. Empty FTopLevelAssetPath otherwise.
	 */
	static COREUOBJECT_API FTopLevelAssetPath TryConvertShortTypeNameToPathName(UClass* TypeClass, const FString& InShortTypeName, ELogVerbosity::Type AmbiguousMessageVerbosity = ELogVerbosity::NoLogging, const TCHAR* AmbiguousClassMessage = nullptr);

	/**
	 * Tries to convert short class name to class path name. This will only work if the class exists in memory
	 * @param InShortClassName Short class name. If this parameter is already a path name then it's going to be returned as FTopLevelAssetPath
	 * @param AmbiguousMessageVerbosity Verbosity with which to log a message if class name is ambiguous
	 * @param AmbiguousClassMessage Additional message to log when class name is ambiguous (e.g. current operation)
	 * @return Class path name if successful. Empty FTopLevelAssetPath otherwise.
	 */
	template <typename T>
	static FTopLevelAssetPath TryConvertShortTypeNameToPathName(const FString& InShortTypeName, ELogVerbosity::Type AmbiguousMessageVerbosity = ELogVerbosity::NoLogging, const TCHAR* AmbiguousClassMessage = nullptr)
	{
		return TryConvertShortTypeNameToPathName(T::StaticClass(), InShortTypeName, AmbiguousMessageVerbosity, AmbiguousClassMessage);
	}

	/**
	 * Tries to fix an export path containing short class name. Will not modify the input InOutExportPathToFix if a
	 * fixup was not necessary. Optionally modifies it if a fixup is necessary but unsuccessful.
	 * @param InOutExportPathToFix Export path (Class'/Path/To.Object')
	 * @param AmbiguousMessageVerbosity Verbosity with which to log a message if class name is ambiguous
	 * @param AmbiguousClassMessage Additional message to log when class name is ambiguous (e.g. current operation)
	 * @param bClearOnError If the fixup is necessary but unsuccessful, if bClearOnError, clear the value and return
	 *        true, otherwise leave the value unchanged and return false.
	 * @return True if the short path was successfully fixed. False if the provided export path did not contain short
	 *         class name or the short path could not be fixed.
	 */
	static COREUOBJECT_API bool TryFixShortClassNameExportPath(FString& InOutExportPathToFix,
		ELogVerbosity::Type AmbiguousMessageVerbosity = ELogVerbosity::NoLogging,
		const TCHAR* AmbiguousClassMessage = nullptr, bool bClearOnError = false);

	/**
	 * Returns the ObjectName portion of a ClassPath name: "/Path/To.Object" is converted to "Object". 
	 * Returns the full string if it is already a ShortTypeName.
	 */
	static COREUOBJECT_API FString ConvertPathNameToShortTypeName(FStringView InClassPathOrShortTypeName);

	/**
	 * Takes a FullName (from e.g. AssetData.GetFullName or UObject.GetFullName) in either ShortTypeFullName or PathFullName form
	 * ShortTypeFullName: "ClassObjectName /PackagePath/PackageShortName.ObjectName:SubObjectName"
	 * PathFullName: "/ClassPath/ClassPackage.ClassObjectName /PackagePath/PackageShortName.ObjectName:SubObjectName"
	 * Converts it to ShortTypeFullName if not already in that format and returns it
	 */
	static COREUOBJECT_API FString ConvertFullNameToShortTypeFullName(FStringView InFullName);

	/**
	 * Returns whether the given stringview is in ShortTypeName form: No directory separators "/" or object separators ".", SUBOBJECT_DELIMITER
	 * Returns true for empty string.
	 */
	static COREUOBJECT_API bool IsShortTypeName(FStringView ClassPathOrShortTypeName);

#if WITH_EDITOR
	/** Provides access to C++ type info. */
	const ICppClassTypeInfo* GetCppTypeInfo() const
	{
		return CppTypeInfo ? &CppTypeInfo.GetValue() : nullptr;
	}
#endif

	/** Sets C++ type information. Should not be NULL. */
	void SetCppTypeInfoStatic(const FCppClassTypeInfoStatic* InCppTypeInfoStatic)
	{
#if WITH_EDITOR
		check(InCppTypeInfoStatic);
		CppTypeInfo.Emplace(InCppTypeInfoStatic);
#endif
	}
	
	/**
	 * Translates the hardcoded script config names (engine, editor, input and 
	 * game) to their global pendants and otherwise uses config(myini) name to
	 * look for a game specific implementation and creates one based on the
	 * default if it doesn't exist yet.
	 *
	 * @return	name of the class specific ini file
	 */
	COREUOBJECT_API const FString GetConfigName() const;

	/** Returns parent class, the parent of a Class is always another class */
	UClass* GetSuperClass() const
	{
		return (UClass*)GetSuperStruct();
	}

	/** Feedback context for default property import **/
	static COREUOBJECT_API class FFeedbackContext& GetDefaultPropertiesFeedbackContext();

	/** Returns amount of memory used by default object */
	int32 GetDefaultsCount()
	{
		return ClassDefaultObject != nullptr ? GetPropertiesSize() : 0;
	}

	/**
	 * Get the default object from the class
	 * @param	bCreateIfNeeded if true (default) then the CDO is created if it is null
	 * @return		the CDO for this class
	 */
	UObject* GetDefaultObject(bool bCreateIfNeeded = true) const
	{
		if (ClassDefaultObject == nullptr && bCreateIfNeeded)
		{
			InternalCreateDefaultObjectWrapper();
		}

		return ClassDefaultObject;
	}

	/**
	 * Called after PostInitProperties during object construction to allow class specific initialization of an object instance.
	 */
	virtual void PostInitInstance(UObject* InObj, FObjectInstancingGraph* InstanceGraph) {}

	/**
	 * Called during PostLoad of an object to allow class specific PostLoad operations of an object instance.
	 */
	virtual void PostLoadInstance(UObject* InObj) {}

	/**
	 * Helper method to assist with initializing object properties from an explicit list.
	 *
	 * @param	InStruct			the current scope for which the given property list applies
	 * @param	DataPtr				destination address (where to start copying values to)
	 * @param	DefaultDataPtr		source address (where to start copying the defaults data from)
	 */
	virtual void InitPropertiesFromCustomList(uint8* DataPtr, const uint8* DefaultDataPtr) {}

	/**
	 * Allows class to provide data to the object initializer that can affect how native class subobjects are created.
	 */
	virtual void SetupObjectInitializer(FObjectInitializer& ObjectInitializer) const {}

	/**
	 * Some classes may not support creating assets of their type
	 */
	virtual bool CanCreateAssetOfClass() const
	{
		return true;
	}

	/**
	 * Get the name of the CDO for the this class
	 * @return The name of the CDO
	 */
	COREUOBJECT_API FName GetDefaultObjectName() const;

	/** Returns memory used to store temporary data on an instance, used by blueprints */
	virtual uint8* GetPersistentUberGraphFrame(UObject* Obj, UFunction* FuncToCheck) const
	{
		return nullptr;
	}

	/** Creates memory to store temporary data */
	virtual void CreatePersistentUberGraphFrame(UObject* Obj, bool bCreateOnlyIfEmpty = false, bool bSkipSuperClass = false, UClass* OldClass = nullptr) const
	{
	}
	
	/** Clears memory to store temporary data */
	virtual void DestroyPersistentUberGraphFrame(UObject* Obj, bool bSkipSuperClass = false) const
	{
	}

	/**
	 * Get the default object from the class and cast to a particular type
	 * @return		the CDO for this class
	 */
	template<class T>
	T* GetDefaultObject() const
	{
		UObject *Ret = GetDefaultObject();
		check(Ret->IsA(T::StaticClass()));
		return (T*)Ret;
	}

	/** Searches for the default instanced object (often a component) by name **/
	COREUOBJECT_API UObject* GetDefaultSubobjectByName(FName ToFind);

	/** Adds a new default instance map item **/
	void AddDefaultSubobject(UObject* NewSubobject, const UClass* BaseClass)
	{
		// this component must be a derived class of the base class
		check(NewSubobject->IsA(BaseClass));
		// the outer of the component must be of my class or some superclass of me
		check(IsChildOf(NewSubobject->GetOuter()->GetClass()));
	}

	/**
	 * Gets all default instanced objects (often components).
	 *
	 * @param OutDefaultSubobjects An array to be filled with default subobjects.
	 */
	COREUOBJECT_API void GetDefaultObjectSubobjects(TArray<UObject*>& OutDefaultSubobjects);

	/**
	 * Used to safely check whether the passed in flag is set.
	 *
	 * @param	FlagsToCheck		Class flag(s) to check for
	 *
	 * @return	true if the passed in flag is set, false otherwise
	 *			(including no flag passed in, unless the FlagsToCheck is CLASS_AllFlags)
	 */
	FORCEINLINE bool HasAnyClassFlags( EClassFlags FlagsToCheck ) const
	{
		return EnumHasAnyFlags(ClassFlags, FlagsToCheck) != 0;
	}

	/**
	 * Used to safely check whether all of the passed in flags are set.
	 *
	 * @param FlagsToCheck	Class flags to check for
	 * @return true if all of the passed in flags are set (including no flags passed in), false otherwise
	 */
	FORCEINLINE bool HasAllClassFlags( EClassFlags FlagsToCheck ) const
	{
		return EnumHasAllFlags(ClassFlags, FlagsToCheck);
	}

	/**
	 * Gets the class flags.
	 *
	 * @return	The class flags.
	 */
	FORCEINLINE EClassFlags GetClassFlags() const
	{
		return ClassFlags;
	}

	/**
	 * Used to safely check whether the passed in flag is set.
	 *
	 * @param	FlagToCheck		the cast flag to check for (value should be one of the EClassCastFlags enums)
	 *
	 * @return	true if the passed in flag is set, false otherwise
	 *			(including no flag passed in)
	 */
	FORCEINLINE bool HasAnyCastFlag(EClassCastFlags FlagToCheck) const
	{
		return (ClassCastFlags&FlagToCheck) != 0;
	}
	FORCEINLINE bool HasAllCastFlags(EClassCastFlags FlagsToCheck) const
	{
		return (ClassCastFlags&FlagsToCheck) == FlagsToCheck;
	}

	COREUOBJECT_API FString GetDescription() const;

	/**
	 * Assembles the token stream for realtime garbage collection by combining the per class only
	 * token stream for each class in the class hierarchy. This is only done once and duplicate
	 * work is avoided by using an object flag.
	 * @param bForce Assemble the stream even if it has been already assembled (deletes the old one)
	 */
	COREUOBJECT_API void AssembleReferenceTokenStream(bool bForce = false);

	/** 
	 * This will return whether or not this class implements the passed in class / interface 
	 *
	 * @param SomeClass - the interface to check and see if this class implements it
	 */
	COREUOBJECT_API bool ImplementsInterface(const class UClass* SomeInterface) const;

	/** serializes the passed in object as this class's default object using the given archive slot
	 * @param Object the object to serialize as default
	 * @param Slot the structured archive slot to serialize from
	 */
	COREUOBJECT_API virtual void SerializeDefaultObject(UObject* Object, FStructuredArchive::FSlot Slot);

	/** serializes the passed in object as this class's default object using the given archive
	 * @param Object the object to serialize as default
	 * @param Ar the archive to serialize from
	 */
	virtual void SerializeDefaultObject(UObject* Object, FArchive& Ar) final
	{
		SerializeDefaultObject(Object, FStructuredArchiveFromArchive(Ar).GetSlot());
	}

	/** serializes the associated sparse class data for the passed in object using the given archive slot. This should only be called if the class has an associated sparse data structure.
	 * @param Slot the structured archive slot to serialize from
	 */
	COREUOBJECT_API void SerializeSparseClassData(FStructuredArchive::FSlot Slot);

	/** Wraps the PostLoad() call for the class default object.
	 * @param Object the default object to call PostLoad() on
	 */
	virtual void PostLoadDefaultObject(UObject* Object) { Object->PostLoad(); }

	/** 
	 * Purges out the properties of this class in preparation for it to be regenerated
	 * @param bRecompilingOnLoad - true if we are recompiling on load
	 *
	 * In editor, properties are not freed until DestroyPropertiesPendingDestruction is called.
	 */
	COREUOBJECT_API virtual void PurgeClass(bool bRecompilingOnLoad);

	/**
	 * Finds the common base class that parents the two classes passed in.
	 *
	 * @param InClassA		the first class to find the common base for
	 * @param InClassB		the second class to find the common base for
	 * @return				the common base class or NULL
	 */
	static COREUOBJECT_API UClass* FindCommonBase(UClass* InClassA, UClass* InClassB);

	/**
	 * Finds the common base class that parents the array of classes passed in.
	 *
	 * @param InClasses		the array of classes to find the common base for
	 * @return				the common base class or NULL
	 */
	static COREUOBJECT_API UClass* FindCommonBase(const TArray<UClass*>& InClasses);

	/**
	 * Determines if the specified function has been implemented in a Blueprint
	 *
	 * @param InFunctionName	The name of the function to test
	 * @return					True if the specified function exists and is implemented in a blueprint generated class
	 */
	COREUOBJECT_API virtual bool IsFunctionImplementedInScript(FName InFunctionName) const;

	/**
	 * Checks if the property exists on this class or a parent class.
	 * @param InProperty	The property to check if it is contained in this or a parent class.
	 * @return				True if the property exists on this or a parent class.
	 */
	COREUOBJECT_API virtual bool HasProperty(const FProperty* InProperty) const;

	/** Finds the object that is used as the parent object when serializing properties, overridden for blueprints */
	virtual UObject* FindArchetype(const UClass* ArchetypeClass, const FName ArchetypeName) const { return nullptr; }

	/** Returns archetype object for CDO */
	COREUOBJECT_API virtual UObject* GetArchetypeForCDO() const;

	/** Returns archetype for sparse class data */
	COREUOBJECT_API const void* GetArchetypeForSparseClassData() const;

	/** Returns the struct used by the sparse class data archetype */
	COREUOBJECT_API UScriptStruct* GetSparseClassDataArchetypeStruct() const;

	/** Returns whether the sparse class data on this instance overrides that of its archetype (in type or value) */
	UE_DEPRECATED(5.5, "Replace with UE::Reflection::DoesSparseClassDataOverrideArchetype(Class, [](const FProperty*){return true;})")
	COREUOBJECT_API bool OverridesSparseClassDataArchetype() const;

	/**
	* Returns all objects that should be preloaded before the class default object is serialized at load time. Only used by the EDL.
	*
	* @param OutDeps		All objects that should be preloaded before the class default object is serialized at load time.
	*/
	virtual void GetDefaultObjectPreloadDependencies(TArray<UObject*>& OutDeps) {}

	/**
	 * Initializes the ClassReps and NetFields arrays used by replication.
	 * This happens lazily based on the CLASS_ReplicationDataIsSetUp flag,
	 * and will generally occur in Link or PostLoad. It's possible that replicated UFunctions
	 * will load after their owning class, so UFunction::PostLoad will clear the flag on its owning class
	 * to force lazy initialization next time the data is needed.
	 * Also happens after blueprint compiliation.
	 */
	COREUOBJECT_API void SetUpRuntimeReplicationData();

	/**
	 * Helper function for determining if the given class is compatible with structured archive serialization
	 */
	static COREUOBJECT_API bool IsSafeToSerializeToStructuredArchives(UClass* InClass);

private:
	/** 
	 * This signature intentionally hides the method declared in UObjectBaseUtility to make it private.
	 * Call IsChildOf instead; Hidden because calling IsA on a class almost always indicates an error where the caller should use IsChildOf
	 */
	bool IsA(const UClass* Parent) const
	{
		return UObject::IsA(Parent);
	}

	/**
	 * This signature intentionally hides the method declared in UObject to make it private.
	 * Call ImplementsInterface instead; Hidden because calling Implements on a class almost always indicates an error where the caller should use ImplementsInterface
	 */
	template <typename T>
	bool Implements() const
	{
		return UObject::Implements<T>();
	}

	/**
	 * This signature intentionally hides the method declared in UObject to make it private.
	 * Call FindFunctionByName instead; This method will search for a function declared in UClass instead of the class it was called on
	 */
	UFunction* FindFunction(FName InName) const
	{
		return UObject::FindFunction(InName);
	}

	/** 
	 * This signature intentionally hides the method declared in UObject to make it private.
	 * Call FindFunctionByName instead; This method will search for a function declared in UClass instead of the class it was called on
	 */
	UFunction* FindFunctionChecked(FName InName) const
	{
		return UObject::FindFunctionChecked(InName);
	}

	/**
	 * Tests if all properties tagged with Replicate were registered in GetLifetimeReplicatedProps
	 */
	COREUOBJECT_API void ValidateRuntimeReplicationData();
	COREUOBJECT_API void AssembleReferenceTokenStreamInternal(bool bForce = false);
	COREUOBJECT_API void InternalCreateDefaultObjectWrapper() const;
protected:
	/**
	 * Get the default object from the class, creating it if missing, if requested or under a few other circumstances
	 * @return		the CDO for this class
	 **/
	COREUOBJECT_API virtual UObject* CreateDefaultObject();
};

/**
 * Helper template to call the default constructor for a class
 */
template<class T>
void InternalConstructor( const FObjectInitializer& X )
{ 
	T::__DefaultConstructor(X);
}


/**
 * Helper template to call the vtable ctor caller for a class
 */
template<class T>
UObject* InternalVTableHelperCtorCaller(FVTableHelper& Helper)
{
	return T::__VTableCtorCaller(Helper);
}

COREUOBJECT_API void InitializePrivateStaticClass(
	class UClass* TClass_Super_StaticClass,
	class UClass* TClass_PrivateStaticClass,
	class UClass* TClass_WithinClass_StaticClass,
	const TCHAR* PackageName,
	const TCHAR* Name
	);

/**
 * Helper template allocate and construct a UClass
 *
 * @param PackageName name of the package this class will be inside
 * @param Name of the class
 * @param ReturnClass reference to pointer to result. This must be PrivateStaticClass.
 * @param RegisterNativeFunc Native function registration function pointer.
 * @param InSize Size of the class
 * @param InAlignment Alignment of the class
 * @param InClassFlags Class flags
 * @param InClassCastFlags Class cast flags
 * @param InConfigName Class config name
 * @param InClassConstructor Class constructor function pointer
 * @param InClassVTableHelperCtorCaller Class constructor function for vtable pointer
 * @param InCppClassStaticFunctions Function pointers for the class's version of Unreal's reflected static functions
 * @param InSuperClassFn Super class function pointer
 * @param WithinClass Within class
 */
COREUOBJECT_API void GetPrivateStaticClassBody(
	const TCHAR* PackageName,
	const TCHAR* Name,
	UClass*& ReturnClass,
	void(*RegisterNativeFunc)(),
	uint32 InSize,
	uint32 InAlignment,
	EClassFlags InClassFlags,
	EClassCastFlags InClassCastFlags,
	const TCHAR* InConfigName,
	UClass::ClassConstructorType InClassConstructor,
	UClass::ClassVTableHelperCtorCallerType InClassVTableHelperCtorCaller,
	FUObjectCppClassStaticFunctions&& InCppClassStaticFunctions,
	UClass::StaticClassFunctionType InSuperClassFn,
	UClass::StaticClassFunctionType InWithinClassFn);

/*-----------------------------------------------------------------------------
	FObjectInstancingGraph.
-----------------------------------------------------------------------------*/

enum class EInstancePropertyValueFlags
{
	None                   = 0x00,

	// if set, then this property causes an instance to be created, otherwise this is just a pointer to a uobject that should be remapped if the object is instanced for some other property
	CausesInstancing       = 0x01,

	// if set, instance the reference to the subobjectroot, so far only delegates remap a self reference
	AllowSelfReference     = 0x02,

	// if set, then we do not create a new instance, but we will reassign one if there is already a mapping in the table
	DoNotCreateNewInstance = 0x04
};

ENUM_CLASS_FLAGS(EInstancePropertyValueFlags)

enum class EObjectInstancingGraphOptions
{
	None = 0x00,

	// if set, start with component instancing disabled
	DisableInstancing = 0x01,

	// if set, instance only subobject template values
	InstanceTemplatesOnly = 0x02,
};

ENUM_CLASS_FLAGS(EObjectInstancingGraphOptions)

struct FObjectInstancingGraph
{
public:

	/** 
	 * Default Constructor 
	 * @param bDisableInstancing - if true, start with component instancing disabled
	**/
	COREUOBJECT_API explicit FObjectInstancingGraph(bool bDisableInstancing = false);

	/**
	 * Constructor with options
	 * @param InOptions Additional options to modify the behavior of this graph
	**/
	COREUOBJECT_API explicit FObjectInstancingGraph(EObjectInstancingGraphOptions InOptions);

	/**
	 * Standard constructor
	 *
	 * @param	DestinationSubobjectRoot	the top-level object that is being created
	 * @param	InOptions					Additional options to modify the behavior of this graph
	 */
	COREUOBJECT_API explicit FObjectInstancingGraph( class UObject* DestinationSubobjectRoot, EObjectInstancingGraphOptions InOptions = EObjectInstancingGraphOptions::None);

	/**
	 * Returns whether this instancing graph has a valid destination root.
	 */
	bool HasDestinationRoot() const
	{
		return GetDestinationRoot() != nullptr;
	}

	/**
	 * Returns the DestinationRoot for this instancing graph.
	 */
	const UObject* GetDestinationRoot() const
	{
		return DestinationRoot;
	}

	/**
	 * Sets the DestinationRoot for this instancing graph.
	 *
	 * @param	DestinationSubobjectRoot	the top-level object that is being created
	 * @param	InSourceRoot	Archetype of DestinationSubobjectRoot
	 */
	COREUOBJECT_API void SetDestinationRoot( class UObject* DestinationSubobjectRoot, class UObject* InSourceRoot = nullptr );

	/**
	 * Finds the destination object instance corresponding to the specified source object.
	 *
	 * @param	SourceObject			the object to find the corresponding instance for
	 */
	COREUOBJECT_API class UObject* GetDestinationObject(class UObject* SourceObject);

	/**
	 * Returns the component that has SourceComponent as its archetype, instancing the component as necessary.
	 *
	 * @param	SourceComponent		the component to find the corresponding component instance for
	 * @param	CurrentValue		the component currently assigned as the value for the component property
	 *								being instanced.  Used when updating archetypes to ensure that the new instanced component
	 *								replaces the existing component instance in memory.
	 * @param	CurrentObject		the object that owns the component property currently being instanced;  this is NOT necessarily the object
	 *								that should be the Outer for the new component.
	 * @param	bIsTransient		is this for a transient property?
	 * @param	bCausesInstancing	if true, then this property causes an instance to be created...if false, this is just a pointer to a uobject that should be remapped if the object is instanced for some other property
	 * @param	bAllowSelfReference If true, instance the reference to the subobjectroot, so far only delegates remap a self reference
	 *
	 * @return	As with GetInstancedSubobject, above, but also deals with archetype creation and a few other special cases
	 */
	UE_DEPRECATED(5.2, "This overload of InstancePropertyValue has been deprecated, please call the overload that takes flags instead.")
	class UObject* InstancePropertyValue( class UObject* SourceComponent, class UObject* CurrentValue, class UObject* CurrentObject, bool bIsTransient, bool bCausesInstancing = false, bool bAllowSelfReference = false )
	{
		EInstancePropertyValueFlags Flags = EInstancePropertyValueFlags::None;
		if (bCausesInstancing)
		{
			Flags |= EInstancePropertyValueFlags::CausesInstancing;
		}
		if (bAllowSelfReference)
		{
			Flags |= EInstancePropertyValueFlags::AllowSelfReference;
		}
		return InstancePropertyValue(SourceComponent, CurrentValue, CurrentObject, Flags);
	}

	/**
	 * Returns the component that has SourceComponent as its archetype, instancing the component as necessary.
	 *
	 * @param	SourceComponent		the component to find the corresponding component instance for
	 * @param	CurrentValue		the component currently assigned as the value for the component property
	 *								being instanced.  Used when updating archetypes to ensure that the new instanced component
	 *								replaces the existing component instance in memory.
	 * @param	CurrentObject		the object that owns the component property currently being instanced;  this is NOT necessarily the object
	 *								that should be the Outer for the new component.
	 * @param	Flags				reinstancing flags - see EInstancePropertyValueFlags
	 *
	 * @return	As with GetInstancedSubobject, above, but also deals with archetype creation and a few other special cases
	 */
	COREUOBJECT_API class UObject* InstancePropertyValue( class UObject* SourceComponent, class UObject* CurrentValue, class UObject* CurrentObject, EInstancePropertyValueFlags Flags = EInstancePropertyValueFlags::None );

	/**
	 * Adds a partially built object instance to the map(s) of source objects to their instances.
	 * @param	ObjectInstance  Object that was just allocated, but has not been constructed yet
	 * @param	InArchetype     Archetype of ObjectInstance
	 */
	COREUOBJECT_API void AddNewObject(class UObject* ObjectInstance, class UObject* InArchetype = nullptr);

	/**
	 * Adds an object instance to the map of source objects to their instances.  If there is already a mapping for this object, it will be replaced
	 * and the value corresponding to ObjectInstance's archetype will now point to ObjectInstance.
	 *
	 * @param	ObjectInstance  the object that should be added as the corresopnding instance for ObjectSource
	 * @param	InArchetype     Archetype of ObjectInstance
	 */
	COREUOBJECT_API void AddNewInstance(class UObject* ObjectInstance, class UObject* InArchetype = nullptr);

	/**
	 * Retrieves a list of objects that have the specified Outer
	 *
	 * @param	SearchOuter		the object to retrieve object instances for
	 * @param	out_Components	receives the list of objects contained by SearchOuter
	 */
	COREUOBJECT_API void RetrieveObjectInstances( class UObject* SearchOuter, TArray<class UObject*>& out_Objects );

	/**
	 * Allows looping over instances that were created during this instancing.
	 *
	 * @param	Pred		the object to retrieve object instances for
	 */
	template <typename Predicate>
	void ForEachObjectInstance(Predicate Pred)
	{
		for (TMap<UObject*, UObject*>::TIterator It(SourceToDestinationMap); It; ++It)
		{
			UObject* InstancedObject = It.Value();
			Pred(InstancedObject);
		}
	}

	/**
	 * Enables / disables component instancing.
	 */
	void EnableSubobjectInstancing( bool bEnabled )
	{
		if (bEnabled)
		{
			InstancingOptions &= ~EObjectInstancingGraphOptions::DisableInstancing;
		}
		else
		{
			InstancingOptions |= EObjectInstancingGraphOptions::DisableInstancing;
		}
	}

	/**
	 * Returns whether component instancing is enabled
	 */
	bool IsSubobjectInstancingEnabled() const
	{
		return !(InstancingOptions & EObjectInstancingGraphOptions::DisableInstancing);
	}

	/**
	 * Sets whether DestinationRoot is currently being loaded from disk.
	 */
	void SetLoadingObject( bool bIsLoading )
	{
		bLoadingObject = bIsLoading;
	}

	/**
	 * Adds a member variable property that should not instantiate subobjects
	 */
	void AddPropertyToSubobjectExclusionList(const FProperty* Property)
	{
		SubobjectInstantiationExclusionList.Add(Property);
	}

	/**
	 * Checks if a member variable property is in subobject instantiation exclusion list
	 */
	bool IsPropertyInSubobjectExclusionList(const FProperty* Property) const
	{
		return SubobjectInstantiationExclusionList.Contains(Property);
	}

private:
	/**
	 * Returns whether DestinationRoot corresponds to an archetype object.
	 *
	 * @param	bUserGeneratedOnly	true indicates that we only care about cases where the user selected "Create [or Update] Archetype" in the editor
	 *								false causes this function to return true even if we are just loading an archetype from disk
	 */
	bool IsCreatingArchetype( bool bUserGeneratedOnly=true ) const
	{
		// if we only want cases where we are creating an archetype in response to user input, return false if we are in fact just loading the object from disk
		return bCreatingArchetype && (!bUserGeneratedOnly || !bLoadingObject);
	}

	/**
	 * Returns whether DestinationRoot is currently being loaded from disk.
	 */
	bool IsLoadingObject() const
	{
		return bLoadingObject;
	}

	/**
	 * Returns the component that has SourceComponent as its archetype, instancing the component as necessary.
	 *
	 * @param	SourceComponent		the component to find the corresponding component instance for
	 * @param	CurrentValue		the component currently assigned as the value for the component property
	 *								being instanced.  Used when updating archetypes to ensure that the new instanced component
	 *								replaces the existing component instance in memory.
	 * @param	CurrentObject		the object that owns the component property currently being instanced;  this is NOT necessarily the object
	 *								that should be the Outer for the new component.
	 * @param	Flags				reinstancing flags - see EInstancePropertyValueFlags
	 *
	 * @return	if SourceComponent is contained within SourceRoot, returns a pointer to a unique component instance corresponding to
	 *			SourceComponent if SourceComponent is allowed to be instanced in this context, or NULL if the component isn't allowed to be
	 *			instanced at this time (such as when we're a client and the component isn't loaded on clients)
	 *			if SourceComponent is not contained by SourceRoot, return INVALID_OBJECT, indicating that the that has SourceComponent as its ObjectArchetype, or NULL if SourceComponent is not contained within
	 *			SourceRoot.
	 */
	COREUOBJECT_API class UObject* GetInstancedSubobject( class UObject* SourceSubobject, class UObject* CurrentValue, class UObject* CurrentObject, EInstancePropertyValueFlags Flags );

	/**
	 * The root of the object tree that is the source used for instancing components;
	 * - when placing an instance of an actor class, this would be the actor class default object
	 * - when placing an instance of an archetype, this would be the archetype
	 * - when creating an archetype, this would be the actor instance
	 * - when duplicating an object, this would be the duplication source
	 */
	class		UObject*						SourceRoot;

	/**
	 * The root of the object tree that is the destination used for instancing components
	 * - when placing an instance of an actor class, this would be the placed actor
	 * - when placing an instance of an archetype, this would be the placed actor
	 * - when creating an archetype, this would be the actor archetype
	 * - when updating an archetype, this would be the source archetype
	 * - when duplicating an object, this would be the copied object (destination)
	 */
	class		UObject*						DestinationRoot;

	/** Subobject instancing options */
	EObjectInstancingGraphOptions				InstancingOptions;

	/**
	 * Indicates whether we are currently instancing components for an archetype.  true if we are creating or updating an archetype.
	 */
	bool										bCreatingArchetype;

	/**
	 * true when loading object data from disk.
	 */
	bool										bLoadingObject;

	/**
	 * Maps the source (think archetype) to the destination (think instance)
	 */
	TMap<class UObject*,class UObject*>			SourceToDestinationMap;

	/** List of member variable properties that should not instantiate subobjects */
	TSet<const FProperty*>						SubobjectInstantiationExclusionList;
};

Expose_TNameOf(FObjectInstancingGraph);

Expose_TNameOf(FObjectInstancingGraph*);

// UFunction interface.

inline UFunction* UFunction::GetSuperFunction() const
{
	UStruct* Result = GetSuperStruct();
	checkSlow(!Result || Result->IsA<UFunction>());
	return (UFunction*)Result;
}


// UObject.h

/**
 * Returns true if this object implements the interface T, false otherwise.
 */
template<class T>
FORCEINLINE bool UObject::Implements() const
{
	UClass const* const MyClass = GetClass();
	return MyClass && MyClass->ImplementsInterface(T::StaticClass());
}

// UObjectGlobals.h

/**
 * Gets the default object of a class.
 *
 * In most cases, class default objects should not be modified. This method therefore returns
 * an immutable pointer. If you need to modify the default object, use GetMutableDefault instead.
 *
 * @param Class - The class to get the CDO for.
 *
 * @return Class default object (CDO).
 *
 * @see GetMutableDefault
 */
template< class T > 
inline const T* GetDefault(UClass *Class)
{
	check(Class->GetDefaultObject()->IsA(T::StaticClass()));
	return (const T*)Class->GetDefaultObject();
}

/**
 * Gets the mutable default object of a class.
 *
 * @param Class - The class to get the CDO for.
 *
 * @return Class default object (CDO).
 *
 * @see GetDefault
 */
template< class T > 
inline T* GetMutableDefault(UClass *Class)
{
	check(Class->GetDefaultObject()->IsA(T::StaticClass()));
	return (T*)Class->GetDefaultObject();
}

struct FStructUtils
{
	COREUOBJECT_API static bool ArePropertiesTheSame(const FProperty* A, const FProperty* B, bool bCheckPropertiesNames);

	/** Do structures have exactly the same memory layout */
	COREUOBJECT_API static bool TheSameLayout(const UStruct* StructA, const UStruct* StructB, bool bCheckPropertiesNames = false);

	/** Locates a named structure in the package with the given name. Not expected to fail */
	COREUOBJECT_API static UStruct* FindStructureInPackageChecked(const TCHAR* StructName, const TCHAR* PackageName);

#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
	/** Looks for uninitialized script struct pointers. Returns the number found */
	COREUOBJECT_API static int32 AttemptToFindUninitializedScriptStructMembers();

#if WITH_EDITORONLY_DATA
	/** Looks for short type names within struct metadata. Returns the number found */
	COREUOBJECT_API static int32 AttemptToFindShortTypeNamesInMetaData();
#endif // WITH_EDITORONLY_DATA
#endif // !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
};

// Helper struct to test if member initialization tests work properly
struct FTestUninitializedScriptStructMembersTest
{
	UObject* UninitializedObjectReference;
	UObject* InitializedObjectReference = nullptr;
	float UnusedValue;
};

/*-----------------------------------------------------------------------------
	Mirrors of mirror structures in Object.h. These are used by generated code 
	to facilitate correct offsets and alignments for structures containing these
	odd types.
-----------------------------------------------------------------------------*/

template <typename T, bool bHasStaticStruct = TModels_V<CStaticStructProvider, T>>
struct TBaseStructureBase
{
	static UScriptStruct* Get()
	{
		return T::StaticStruct();
	}
};

template <typename T>
struct TBaseStructureBase<T, false>
{
};

template <typename T>
struct TBaseStructure : TBaseStructureBase<T>
{
};

template<> struct TBaseStructure<FIntPoint> 
{
	static COREUOBJECT_API UScriptStruct* Get(); 
};

template<> struct TBaseStructure<FIntVector> 
{ 
	static COREUOBJECT_API UScriptStruct* Get(); 
};

template<> struct TBaseStructure<FIntVector4>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

template<> struct TBaseStructure<FLinearColor>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

template<> struct TBaseStructure<FColor>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

template<> struct TBaseStructure<FRandomStream>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

template<> struct TBaseStructure<FGuid>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

template<> struct TBaseStructure<FFallbackStruct>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

template<> struct TBaseStructure<FInterpCurvePointFloat>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

template<> struct TBaseStructure<FInterpCurvePointVector2D>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

template<> struct TBaseStructure<FInterpCurvePointVector>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

template<> struct TBaseStructure<FInterpCurvePointQuat>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

template<> struct TBaseStructure<FInterpCurvePointTwoVectors>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

template<> struct TBaseStructure<FInterpCurvePointLinearColor>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

template<> struct TBaseStructure<FFloatRangeBound>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

template<> struct TBaseStructure<FFloatRange>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

template<> struct TBaseStructure<FDoubleRangeBound>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

template<> struct TBaseStructure<FDoubleRange>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

template<> struct TBaseStructure<FInt32RangeBound>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

template<> struct TBaseStructure<FInt32Range>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

template<> struct TBaseStructure<FFloatInterval>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

template<> struct TBaseStructure<FDoubleInterval>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

template<> struct TBaseStructure<FInt32Interval>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

struct FFrameNumber;

template<> struct TBaseStructure<FFrameNumber>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

struct FFrameTime;

template<> struct TBaseStructure<FFrameTime>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

struct FSoftObjectPath;

template<> struct TBaseStructure<FSoftObjectPath>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

struct FSoftClassPath;

template<> struct TBaseStructure<FSoftClassPath>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

struct FPrimaryAssetType;

template<> struct TBaseStructure<FPrimaryAssetType>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

struct FPrimaryAssetId;

template<> struct TBaseStructure<FPrimaryAssetId>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

struct FDateTime;

template<> struct TBaseStructure<FDateTime>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

struct FPolyglotTextData;

template<> struct TBaseStructure<FPolyglotTextData>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

struct FAssetBundleData;

template<> struct TBaseStructure<FAssetBundleData>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

template<> struct TBaseStructure<FTestUninitializedScriptStructMembersTest>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

struct FTopLevelAssetPath;

template<> struct TBaseStructure<FTopLevelAssetPath>
{
	static COREUOBJECT_API UScriptStruct* Get();
};

// TBaseStructure for explicit core variant types only. e.g. FVector3d returns "Vector3d" struct. 
template< class T > struct TVariantStructure
{
	static_assert(sizeof(T) == 0, "Unsupported for this type. Did you mean to use TBaseStructure?");
};

#define UE_DECLARE_CORE_VARIANT_TYPE(VARIANT, CORE)														\
template<> struct TBaseStructure<::F##CORE> { COREUOBJECT_API static UScriptStruct* Get(); };			\
template<> struct TVariantStructure<::F##VARIANT##f> { COREUOBJECT_API static UScriptStruct* Get(); };	\
template<> struct TVariantStructure<::F##VARIANT##d> { COREUOBJECT_API static UScriptStruct* Get(); };

UE_DECLARE_CORE_VARIANT_TYPE(Vector2,	Vector2D);
UE_DECLARE_CORE_VARIANT_TYPE(Vector3,	Vector);
UE_DECLARE_CORE_VARIANT_TYPE(Vector4,	Vector4);
UE_DECLARE_CORE_VARIANT_TYPE(Plane4,	Plane);
UE_DECLARE_CORE_VARIANT_TYPE(Quat4,		Quat);
UE_DECLARE_CORE_VARIANT_TYPE(Rotator3,	Rotator);
UE_DECLARE_CORE_VARIANT_TYPE(Transform3,Transform);
UE_DECLARE_CORE_VARIANT_TYPE(Matrix44,	Matrix);
UE_DECLARE_CORE_VARIANT_TYPE(Box2,		Box2D);
UE_DECLARE_CORE_VARIANT_TYPE(Ray3,		Ray);
UE_DECLARE_CORE_VARIANT_TYPE(Sphere3, Sphere);

#undef UE_DECLARE_CORE_VARIANT_TYPE

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
