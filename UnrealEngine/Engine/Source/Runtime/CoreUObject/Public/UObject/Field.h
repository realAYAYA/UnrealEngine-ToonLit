// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Field.h: Declares FField property system fundamentals
=============================================================================*/

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/StringFwd.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "HAL/PlatformMath.h"
#include "HAL/ThreadSafeCounter.h"
#include "HAL/UnrealMemory.h"
#include "Internationalization/Text.h"
#include "Math/RandomStream.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CString.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/Guid.h"
#include "Misc/Optional.h"
#include "Serialization/Archive.h"
#include "Serialization/StructuredArchive.h"
#include "Serialization/StructuredArchiveAdapters.h"
#include "Serialization/StructuredArchiveSlots.h"
#include "Templates/EnableIf.h"
#include "Templates/IsAbstract.h"
#include "Templates/IsEnum.h"
#include "Templates/TypeHash.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/UnrealTypeTraits.h"
#include "UObject/CoreNative.h"
#include "UObject/GarbageCollection.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/PersistentObjectPtr.h"
#include "UObject/Script.h"
#include "UObject/SparseDelegate.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealNames.h"
#include "UObject/WeakObjectPtr.h"

#include <type_traits>

class FField;
class FFieldVariant;
class FProperty;
class FReferenceCollector;
class UClass;
class UField;
class UPackage;
class UStruct;

/**
  * Object representing a type of an FField struct. 
  * Mimics a subset of UObject reflection functions.
  */
class FFieldClass
{
	UE_NONCOPYABLE(FFieldClass);

	/** Name of this field class */
	FName Name;
	/** Unique Id of this field class (for casting) */
	uint64 Id;
	/** Cast flags used for casting to other classes */
	uint64 CastFlags;
	/** Class flags */
	EClassFlags ClassFlags;
	/** Super of this class */
	FFieldClass* SuperClass;	
	/** Default instance of this class */
	FField* DefaultObject;
	/** Pointer to a function that can construct an instance of this class */
	FField* (*ConstructFn)(const FFieldVariant&, const FName&, EObjectFlags);
	/** Counter for generating runtime unique names */
	FThreadSafeCounter UnqiueNameIndexCounter;

	/** Creates a default object instance of this class */
	COREUOBJECT_API FField* ConstructDefaultObject();

public:

	/** Gets the list of all field classes in existance */
	static COREUOBJECT_API TArray<FFieldClass*>& GetAllFieldClasses();
	/** Gets a mapping of all field class names to the actuall class objects */
	static COREUOBJECT_API TMap<FName, FFieldClass*>& GetNameToFieldClassMap();

	COREUOBJECT_API explicit FFieldClass(const TCHAR* InCPPName, uint64 InId, uint64 InCastFlags, FFieldClass* InSuperClass, FField* (*ConstructFnPtr)(const FFieldVariant&, const FName&, EObjectFlags));
	COREUOBJECT_API ~FFieldClass();

	inline FString GetName() const
	{
		return Name.ToString();
	}
	inline FName GetFName() const
	{
		return Name;
	}
	inline uint64 GetId() const
	{
		return Id;
	}
	inline uint64 GetCastFlags() const
	{
		return CastFlags;
	}
	inline bool HasAnyCastFlags(const uint64 InCastFlags) const
	{
		return !!(CastFlags & InCastFlags);
	}
	inline bool HasAllCastFlags(const uint64 InCastFlags) const
	{
		return (CastFlags & InCastFlags) == InCastFlags;
	}
	inline bool IsChildOf(const FFieldClass* InClass) const
	{
		const uint64 OtherClassId = InClass->GetId();
		return OtherClassId ? !!(CastFlags & OtherClassId) : IsChildOf_Walk(InClass);
	}
	COREUOBJECT_API FString GetDescription() const;
	COREUOBJECT_API FText GetDisplayNameText() const;
	FField* Construct(const FFieldVariant& InOwner, const FName& InName, EObjectFlags InFlags = RF_NoFlags) const
	{
		return ConstructFn(InOwner, InName, InFlags);
	}

	FFieldClass* GetSuperClass() const
	{
		return SuperClass;
	}

	FField* GetDefaultObject()
	{
		if (!DefaultObject)
		{
			DefaultObject = ConstructDefaultObject();
			check(DefaultObject);
		}
		return DefaultObject;
	}

	bool HasAnyClassFlags(EClassFlags FlagsToCheck) const
	{
		return EnumHasAnyFlags(ClassFlags, FlagsToCheck) != 0;
	}

	int32 GetNextUniqueNameIndex()
	{
		return UnqiueNameIndexCounter.Increment();
	}

	friend FArchive& operator << (FArchive& Ar, FFieldClass& InField)
	{
		check(false);
		return Ar;
	}
	COREUOBJECT_API friend FArchive& operator << (FArchive& Ar, FFieldClass*& InOutFieldClass);

private:
	bool IsChildOf_Walk(const FFieldClass* InBaseClass) const
	{
		for (const FFieldClass* TempField = this; TempField; TempField = TempField->GetSuperClass())
		{
			if (TempField == InBaseClass)
			{
				return true;
			}
		}
		return false;
	}
};

#if !CHECK_PUREVIRTUALS
	#define DECLARE_FIELD_NEW_IMPLEMENTATION(TClass) \
		ThisClass* Mem = (ThisClass*)FMemory::Malloc(InSize); \
		new (Mem) TClass(EC_InternalUseOnlyConstructor, TClass::StaticClass()); \
		return Mem; 
#else
	#define DECLARE_FIELD_NEW_IMPLEMENTATION(TClass) \
			ThisClass* Mem = (ThisClass*)FMemory::Malloc(InSize); \
			return Mem; 
#endif

#define DECLARE_FIELD(TClass, TSuperClass, TStaticFlags) \
	DECLARE_FIELD_API(TClass, TSuperClass, TStaticFlags, NO_API)

#define DECLARE_FIELD_API(TClass, TSuperClass, TStaticFlags, TRequiredAPI) \
private: \
	TClass& operator=(TClass&&);   \
	TClass& operator=(const TClass&);   \
public: \
	typedef TSuperClass Super;\
	typedef TClass ThisClass;\
	TClass(EInternal InInernal, FFieldClass* InClass) \
		: Super(EC_InternalUseOnlyConstructor, InClass) \
	{ \
	} \
	static TRequiredAPI FFieldClass* StaticClass(); \
	static FField* Construct(const FFieldVariant& InOwner, const FName& InName, EObjectFlags InObjectFlags); \
	inline static constexpr uint64 StaticClassCastFlagsPrivate() \
	{ \
		return uint64(TStaticFlags); \
	} \
	inline static constexpr uint64 StaticClassCastFlags() \
	{ \
		return uint64(TStaticFlags) | Super::StaticClassCastFlags(); \
	} \
	inline void* operator new(const size_t InSize, void* InMem) \
	{ \
		return InMem; \
	} \
	inline void* operator new(const size_t InSize) \
	{ \
		DECLARE_FIELD_NEW_IMPLEMENTATION(TClass) \
	} \
	inline void operator delete(void* InMem) noexcept \
	{ \
		FMemory::Free(InMem); \
	} \
	friend FArchive &operator<<( FArchive& Ar, ThisClass*& Res ) \
	{ \
		return Ar << (FField*&)Res; \
	} \
	friend void operator<<(FStructuredArchive::FSlot InSlot, ThisClass*& Res) \
	{ \
		InSlot << (FField*&)Res; \
	}

#if !CHECK_PUREVIRTUALS
	#define IMPLEMENT_FIELD_CONSTRUCT_IMPLEMENTATION(TClass) \
		FField* Instance = new TClass(InOwner, InName, InFlags); \
		return Instance; 
#else
	#define IMPLEMENT_FIELD_CONSTRUCT_IMPLEMENTATION(TClass) \
		return nullptr;
#endif

#define IMPLEMENT_FIELD(TClass) \
FField* TClass::Construct(const FFieldVariant& InOwner, const FName& InName, EObjectFlags InFlags) \
{ \
	IMPLEMENT_FIELD_CONSTRUCT_IMPLEMENTATION(TClass) \
} \
FFieldClass* TClass::StaticClass() \
{ \
	static FFieldClass StaticFieldClass(TEXT(#TClass), TClass::StaticClassCastFlagsPrivate(), TClass::StaticClassCastFlags(), TClass::Super::StaticClass(), &TClass::Construct); \
	return &StaticFieldClass; \
} \

class FField;
class FLinkerLoad;
class FProperty;
class UObject;

/**
 * Special container that can hold either UObject or FField.
 * Exposes common interface of FFields and UObjects for easier transition from UProperties to FProperties.
 * DO NOT ABUSE. IDEALLY THIS SHOULD ONLY BE FFIELD INTERNAL STRUCTURE FOR HOLDING A POINTER TO THE OWNER OF AN FFIELD.
 */
class FFieldVariant
{
	union FFieldObjectUnion
	{
		FField* Field;
		UObject* Object;
	} Container;

	static constexpr uintptr_t UObjectMask = 0x1;

	void ConditionallyMarkAsReachable()
	{
		if (IsUObject() && ToUObjectUnsafe() && UE::GC::GIsIncrementalReachabilityPending)
		{
			UE::GC::MarkAsReachable(ToUObjectUnsafe());
		}
	}
	
public:

	FFieldVariant()
	{
		Container.Field = nullptr;
	}

	FFieldVariant(const FField* InField)
	{
		Container.Field = const_cast<FField*>(InField);
		check(!IsUObject());
	}

	template <
		typename T,
		decltype(ImplicitConv<const UObject*>(std::declval<T>()))* = nullptr
	>
	FFieldVariant(T&& InObject)
	{
		Container.Object = const_cast<UObject*>(ImplicitConv<const UObject*>(InObject));
		Container.Object = (UObject*)((uintptr_t)Container.Object | UObjectMask);
		ConditionallyMarkAsReachable();
	}

	FFieldVariant(TYPE_OF_NULLPTR)
		: FFieldVariant()
	{
	}

	FFieldVariant(const FFieldVariant& Other)
		: Container(Other.Container)
	{
		ConditionallyMarkAsReachable();
	}
	
	FFieldVariant& operator=(const FFieldVariant& Other)
	{
		Container = Other.Container;
		ConditionallyMarkAsReachable();
		return *this;
	}
	
	FFieldVariant(FFieldVariant&& Other)
		: Container(Other.Container)
	{
		ConditionallyMarkAsReachable();
	}
	
	FFieldVariant& operator=(FFieldVariant&& Other)
	{
		Container = Other.Container;
		ConditionallyMarkAsReachable();
		return *this;
	}

	inline bool IsUObject() const
	{
		return (uintptr_t)Container.Object & UObjectMask;
	}
	inline bool IsValid() const
	{
		return !!ToUObjectUnsafe();
	}
	COREUOBJECT_API bool IsValidLowLevel() const;
	inline operator bool() const
	{
		return IsValid();
	}
	COREUOBJECT_API bool IsA(const UClass* InClass) const;
	COREUOBJECT_API bool IsA(const FFieldClass* InClass) const;
	template <typename T>
	bool IsA() const
	{
		static_assert(sizeof(T) > 0, "T must not be an incomplete type");
		return IsA(T::StaticClass());
	}

	template <typename T>
	T* Get() const
	{
		static_assert(sizeof(T) > 0, "T must not be an incomplete type");
		if (IsA(T::StaticClass()))
		{
			if constexpr (std::is_base_of_v<UObject, T>)
			{
				return static_cast<T*>(ToUObjectUnsafe());
			}
			else
			{
				return static_cast<T*>(Container.Field);
			}
		}
		return nullptr;
	}

	UObject* ToUObject() const
	{
		if (IsUObject())
		{
			return ToUObjectUnsafe();
		}
		else
		{
			return nullptr;
		}
	}
	FField* ToField() const
	{
		if (!IsUObject())
		{
			return Container.Field;
		}
		else
		{
			return nullptr;
		}
	}
	/** FOR INTERNAL USE ONLY: Function that returns the owner as FField without checking if it's actually an FField */
	FORCEINLINE FField* ToFieldUnsafe() const
	{
		return Container.Field;
	}
	/** FOR INTERNAL USE ONLY: Function that returns the owner as UObject without checking if it's actually a UObject */
	FORCEINLINE UObject* ToUObjectUnsafe() const
	{
		return (UObject*)((uintptr_t)Container.Object & ~UObjectMask);
	}

	void* GetRawPointer() const
	{
		return Container.Field;
	}
	COREUOBJECT_API FFieldVariant GetOwnerVariant() const;
	COREUOBJECT_API UClass* GetOwnerClass() const;
	COREUOBJECT_API FString GetFullName() const;
	COREUOBJECT_API FString GetPathName() const;
	COREUOBJECT_API FString GetName() const;
	COREUOBJECT_API FString GetClassName() const;
	COREUOBJECT_API FName GetFName() const;
	COREUOBJECT_API bool IsNative() const;
	COREUOBJECT_API UPackage* GetOutermost() const;

	bool operator==(const FFieldVariant& Other) const
	{
		return Container.Field == Other.Container.Field;
	}
	bool operator!=(const FFieldVariant& Other) const
	{
		return Container.Field != Other.Container.Field;
	}

#if WITH_EDITORONLY_DATA
	COREUOBJECT_API bool HasMetaData(const FName& Key) const;
#endif

	/** Support comparison functions that make this usable as a KeyValue for a TSet<> */
	friend uint32 GetTypeHash(const FFieldVariant& InFieldVariant)
	{
		return GetTypeHash(InFieldVariant.GetRawPointer());
	}
};

/**
 * Base class of reflection data objects.
 */
class FField
{
	UE_NONCOPYABLE(FField);

	/** Pointer to the class object representing the type of this FField */
	FFieldClass* ClassPrivate;

public:
	typedef FField Super;
	typedef FField ThisClass;
	typedef FField BaseFieldClass;	
	typedef FFieldClass FieldTypeClass;

	static COREUOBJECT_API FFieldClass* StaticClass();

	inline static constexpr uint64 StaticClassCastFlagsPrivate()
	{
		return uint64(CASTCLASS_UField);
	}
	inline static constexpr uint64 StaticClassCastFlags()
	{
		return uint64(CASTCLASS_UField);
	}

	/** Owner of this field */
	FFieldVariant Owner;

	/** Next Field in the linked list */
	FField* Next;

	/** Name of this field */
	FName NamePrivate;

	/** Object flags */
	EObjectFlags FlagsPrivate;

	// Constructors.
	COREUOBJECT_API FField(EInternal InInernal, FFieldClass* InClass);
	COREUOBJECT_API FField(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
#if WITH_EDITORONLY_DATA
	COREUOBJECT_API explicit FField(UField* InField);
#endif // WITH_EDITORONLY_DATA
	COREUOBJECT_API virtual ~FField();

	// Begin UObject interface: the following functions mimic UObject interface for easier transition from UProperties to FProperties
	COREUOBJECT_API virtual void Serialize(FArchive& Ar);
	COREUOBJECT_API virtual void PostLoad();
	COREUOBJECT_API virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps);
	COREUOBJECT_API virtual void BeginDestroy();	
	COREUOBJECT_API virtual void AddReferencedObjects(FReferenceCollector& Collector);
	COREUOBJECT_API bool IsRooted() const;
	COREUOBJECT_API bool IsNative() const;
	COREUOBJECT_API bool IsValidLowLevel() const;
	COREUOBJECT_API bool IsIn(const UObject* InOwner) const;
	COREUOBJECT_API bool IsIn(const FField* InOwner) const;
	COREUOBJECT_API FLinkerLoad* GetLinker() const;	
	// End UObject interface

	// Begin UField interface.
	COREUOBJECT_API virtual void AddCppProperty(FProperty* Property);
	COREUOBJECT_API virtual void Bind();
	// End UField interface

	/** Constructs a new field given its class */
	static COREUOBJECT_API FField* Construct(const FFieldVariant& InOwner, const FName& InName, EObjectFlags InFlags);
	/** Constructs a new field given the name of its class */
	static COREUOBJECT_API FField* Construct(const FName& FieldTypeName, const FFieldVariant& InOwner, const FName& InName, EObjectFlags InFlags);
	/** Tries to construct a new field given the name of its class. Returns null if the type does not exist. */
	static COREUOBJECT_API FField* TryConstruct(const FName& FieldTypeName, const FFieldVariant& InOwner, const FName& InName, EObjectFlags InFlags);

	/** Fixups after duplicating a Field */
	COREUOBJECT_API virtual void PostDuplicate(const FField& InField);

protected:
	/**
	* Set the object flags directly
	**/
	void SetFlagsTo(EObjectFlags NewFlags)
	{
		checkfSlow((NewFlags & ~RF_AllFlags) == 0, TEXT("%s flagged as 0x%x but is trying to set flags to RF_AllFlags"), *GetFName().ToString(), (int32)FlagsPrivate);
		FlagsPrivate = NewFlags;
	}
public:
	/**
	* Retrieve the object flags directly
	*
	* @return Flags for this object
	**/
	EObjectFlags GetFlags() const
	{
		checkfSlow((FlagsPrivate & ~RF_AllFlags) == 0, TEXT("%s flagged as RF_AllFlags"), *GetFName().ToString());
		return FlagsPrivate;
	}

	void SetFlags(EObjectFlags NewFlags)
	{
		checkSlow(!(NewFlags & (RF_MarkAsNative | RF_MarkAsRootSet))); // These flags can't be used outside of constructors / internal code
		SetFlagsTo(GetFlags() | NewFlags);
	}
	void ClearFlags(EObjectFlags NewFlags)
	{
		checkSlow(!(NewFlags & (RF_MarkAsNative | RF_MarkAsRootSet)) || NewFlags == RF_AllFlags); // These flags can't be used outside of constructors / internal code
		SetFlagsTo(GetFlags() & ~NewFlags);
	}
	/**
	* Used to safely check whether any of the passed in flags are set.
	*
	* @param FlagsToCheck	Object flags to check for.
	* @return				true if any of the passed in flags are set, false otherwise  (including no flags passed in).
	*/
	bool HasAnyFlags(EObjectFlags FlagsToCheck) const
	{
		checkSlow(!(FlagsToCheck & (RF_MarkAsNative | RF_MarkAsRootSet)) || FlagsToCheck == RF_AllFlags); // These flags can't be used outside of constructors / internal code
		return (GetFlags() & FlagsToCheck) != 0;
	}
	/**
	* Used to safely check whether all of the passed in flags are set.
	*
	* @param FlagsToCheck	Object flags to check for
	* @return true if all of the passed in flags are set (including no flags passed in), false otherwise
	*/
	bool HasAllFlags(EObjectFlags FlagsToCheck) const
	{
		checkSlow(!(FlagsToCheck & (RF_MarkAsNative | RF_MarkAsRootSet)) || FlagsToCheck == RF_AllFlags); // These flags can't be used outside of constructors / internal code
		return ((GetFlags() & FlagsToCheck) == FlagsToCheck);
	}

	inline FFieldClass* GetClass() const
	{
		return ClassPrivate;
	}
	inline uint64 GetCastFlags() const
	{
		return GetClass()->GetCastFlags();
	}

	inline bool IsA(const FFieldClass* FieldType) const
	{
		check(FieldType);
		return GetClass()->IsChildOf(FieldType);
	}

	template<typename T>
	bool IsA() const
	{
		if constexpr (!!(T::StaticClassCastFlagsPrivate()))
		{
			return !!(GetCastFlags() & T::StaticClassCastFlagsPrivate());
		}
		else
		{
			return GetClass()->IsChildOf(T::StaticClass());
		}
	}

	UE_DEPRECATED(5.0, "HasAnyCastFlags is deprecated. Not all FField has CastFlag. Use IsA instead.")
	inline bool HasAnyCastFlags(const uint64 InCastFlags) const
	{
		return !!(GetCastFlags() & InCastFlags);
	}

	UE_DEPRECATED(5.0, "HasAllCastFlags is deprecated. Not all FField has CastFlag. Use IsA instead.")
	inline bool HasAllCastFlags(const uint64 InCastFlags) const
	{
		return (GetCastFlags() & InCastFlags) == InCastFlags;
	}

	void AppendName(FString& ResultString) const
	{
		GetFName().AppendString(ResultString);
	}

	/** Gets the owner container for this field */
	FFieldVariant GetOwnerVariant() const
	{
		return Owner;
	}

	/** Goes up the outer chain to look for a UObject. This function is used in GC so for performance reasons it has to be inlined */
	FORCEINLINE UObject* GetOwnerUObject() const
	{
		FFieldVariant TempOuter = Owner;
		while (!TempOuter.IsUObject() && TempOuter.IsValid())
		{
			// It's ok to use the 'Unsafe' variant of ToField here since we just checked IsUObject above
			TempOuter = TempOuter.ToFieldUnsafe()->Owner;
		}
		return TempOuter.ToUObject();
	}

	/** Internal function for quickly getting the owner of this object as UObject. FOR INTERNAL USE ONLY */
	FORCEINLINE UObject* InternalGetOwnerAsUObjectUnsafe() const
	{
		return Owner.ToUObjectUnsafe();
	}

	/** Goes up the outer chain to look for a UClass */
	COREUOBJECT_API UClass* GetOwnerClass() const;

	/** Goes up the outer chain to look for a UStruct */
	COREUOBJECT_API UStruct* GetOwnerStruct() const;

	/** Goes up the outer chain to look for a UField */
	COREUOBJECT_API UField* GetOwnerUField() const;

	/** Goes up the outer chain to look for the outermost package */
	COREUOBJECT_API UPackage* GetOutermost() const;

	/** Goes up the outer chain to look for the outer of the specified type */
	COREUOBJECT_API UObject* GetTypedOwner(UClass* Target) const;

	/** Goes up the outer chain to look for the outer of the specified type */
	COREUOBJECT_API FField* GetTypedOwner(FFieldClass* Target) const;

	template <typename T>
	T* GetOwner() const
	{
		static_assert(sizeof(T) > 0, "T must not be an incomplete type");
		return Owner.Get<T>();
	}

	template <typename T>
	FUNCTION_NON_NULL_RETURN_START
	T* GetOwnerChecked() const
	FUNCTION_NON_NULL_RETURN_END
	{
		static_assert(sizeof(T) > 0, "T must not be an incomplete type");
		T* Result = Owner.Get<T>();
		check(Result);
		return Result;
	}

	template <typename T>
	T* GetTypedOwner() const
	{
		static_assert(sizeof(T) > 0, "T must not be an incomplete type");
		return static_cast<T*>(GetTypedOwner(T::StaticClass()));
	}

	FORCEINLINE FName GetFName() const
	{
		if (this != nullptr)
		{
			return NamePrivate;
		}
		else
		{
			return NAME_None;
		}
	}

	FORCEINLINE FString GetName() const
	{
		if (this != nullptr)
		{
			return NamePrivate.ToString();
		}
		else
		{
			return TEXT("None");
		}
	}

	FORCEINLINE void GetName(FString& OutName) const
	{
		if (this != nullptr)
		{
			return NamePrivate.ToString(OutName);
		}
		else
		{
			OutName = TEXT("None");
		}
	}

	COREUOBJECT_API void Rename(const FName& NewName);

	COREUOBJECT_API FString GetPathName(const UObject* StopOuter = nullptr) const;
	COREUOBJECT_API void GetPathName(const UObject* StopOuter, FStringBuilderBase& ResultString) const;
	COREUOBJECT_API FString GetFullName() const;

	/**
	 * Returns a human readable string that was assigned to this field at creation.
	 * By default this is the same as GetName() but it can be overridden if that is an internal-only name.
	 * This name is consistent in editor/cooked builds, is not localized, and is useful for data import/export.
	 */
	COREUOBJECT_API FString GetAuthoredName() const;

	/** Returns an inner field by name if the field has any */
	virtual FField* GetInnerFieldByName(const FName& InName)
	{
		return nullptr;
	}

	/** Fills the provided array with all inner fields this field owns (recursively) */
	virtual void GetInnerFields(TArray<FField*>& OutFields)
	{
	}

#if WITH_EDITORONLY_DATA

private:
	/** Editor-only meta data map */
	TMap<FName, FString>* MetaDataMap;

public:

	/**
	* Walks up the chain of packages until it reaches the top level, which it ignores.
	*
	* @param	bStartWithOuter		whether to include this object's name in the returned string
	* @return	string containing the path name for this object, minus the outermost-package's name
	*/
	COREUOBJECT_API FString GetFullGroupName(bool bStartWithOuter) const;

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
	* @return The value associated with the key if it exists, null otherwise
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
	COREUOBJECT_API const FText GetMetaDataText(const TCHAR* MetaDataKey, const FString LocalizationNamespace = FString(), const FString LocalizationKey = FString()) const;
	COREUOBJECT_API const FText GetMetaDataText(const FName& MetaDataKey, const FString LocalizationNamespace = FString(), const FString LocalizationKey = FString()) const;

	/**
	* Sets the metadata value associated with the key
	*
	* @param Key The key to lookup in the metadata
	* @return The value associated with the key
	*/
	COREUOBJECT_API void SetMetaData(const TCHAR* Key, const TCHAR* InValue);
	COREUOBJECT_API void SetMetaData(const FName& Key, const TCHAR* InValue);

	COREUOBJECT_API void SetMetaData(const TCHAR* Key, FString&& InValue);
	COREUOBJECT_API void SetMetaData(const FName& Key, FString&& InValue);

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
	* @return the int value stored in the metadata.
	*/
	int32 GetIntMetaData(const TCHAR* Key) const
	{
		const FString& INTString = GetMetaData(Key);
		int32 Value = FCString::Atoi(*INTString);
		return Value;
	}
	int32 GetIntMetaData(const FName& Key) const
	{
		const FString& INTString = GetMetaData(Key);
		int32 Value = FCString::Atoi(*INTString);
		return Value;
	}

	/**
	* Find the metadata value associated with the key
	* and return float
	* @param Key The key to lookup in the metadata
	* @return the float value stored in the metadata.
	*/
	float GetFloatMetaData(const TCHAR* Key) const
	{
		const FString& FLOATString = GetMetaData(Key);
		// FString == operator does case insensitive comparison
		float Value = FCString::Atof(*FLOATString);
		return Value;
	}
	float GetFloatMetaData(const FName& Key) const
	{
		const FString& FLOATString = GetMetaData(Key);
		// FString == operator does case insensitive comparison
		float Value = FCString::Atof(*FLOATString);
		return Value;
	}

	/**
	* Find the metadata value associated with the key
	* and return double
	* @param Key The key to lookup in the metadata
	* @return the float value stored in the metadata.
	*/
	double GetDoubleMetaData(const TCHAR* Key) const
	{
		const FString& DOUBLEString = GetMetaData(Key);
		// FString == operator does case insensitive comparison
		double Value = FCString::Atod(*DOUBLEString);
		return Value;
	}
	double GetDoubleMetaData(const FName& Key) const
	{
		const FString& DOUBLEString = GetMetaData(Key);
		// FString == operator does case insensitive comparison
		double Value = FCString::Atod(*DOUBLEString);
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

	/** Gets all metadata associated with this field */
	COREUOBJECT_API const TMap<FName, FString>* GetMetaDataMap() const;

	/** Append the given metadata to this field */
	COREUOBJECT_API void AppendMetaData(const TMap<FName, FString>& MetaDataMapToAppend);

	/** Copies all metadata from Source Field to Dest Field */
	static COREUOBJECT_API void CopyMetaData(const FField* InSourceField, FField* InDestField);

	/** Creates a new FField from existing UField */
	static COREUOBJECT_API FField* CreateFromUField(UField* InField);
	
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnConvertCustomUFieldToFField, FFieldClass*, UField*, FField*&);
	/** Gets a delegate to convert custom UField types to FFields */
	static COREUOBJECT_API FOnConvertCustomUFieldToFField& GetConvertCustomUFieldToFFieldDelegate();

#endif // WITH_EDITORONLY_DATA

	/** Duplicates an FField */
	static COREUOBJECT_API FField* Duplicate(const FField* InField, FFieldVariant DestOwner, const FName DestName = NAME_None, EObjectFlags FlagMask = RF_AllFlags, EInternalObjectFlags InternalFlagsMask = EInternalObjectFlags_AllFlags);

	/** Generates a name for a Field of a given type. Each generated name is unique in the current runtime */
	static COREUOBJECT_API FName GenerateFFieldName(FFieldVariant InOwner, FFieldClass* InClass);
};

// Support for casting between different FFIeld types

template<typename FieldType>
FORCEINLINE FieldType* CastField(FField* Src)
{
	return Src && Src->IsA<FieldType>() ? static_cast<FieldType*>(Src) : nullptr;
}

template<typename FieldType>
FORCEINLINE const FieldType* CastField(const FField* Src)
{
	return Src && Src->IsA<FieldType>() ? static_cast<const FieldType*>(Src) : nullptr;
}

template<typename FieldType>
FORCEINLINE FieldType* ExactCastField(FField* Src)
{
	return (Src && (Src->GetClass() == FieldType::StaticClass())) ? static_cast<FieldType*>(Src) : nullptr;
}

template<typename FieldType>
FORCEINLINE FieldType* ExactCastField(const FField* Src)
{
	return (Src && (Src->GetClass() == FieldType::StaticClass())) ? static_cast<const FieldType*>(Src) : nullptr;
}

template<typename FieldType>
FUNCTION_NON_NULL_RETURN_START
FORCEINLINE FieldType* CastFieldChecked(FField* Src)
FUNCTION_NON_NULL_RETURN_END
{
#if !DO_CHECK
	return (FieldType*)Src;
#else
	FieldType* CastResult = Src && Src->IsA<FieldType>() ? static_cast<FieldType*>(Src) : nullptr;
	checkf(CastResult, TEXT("CastFieldChecked failed with 0x%016llx"), (int64)(PTRINT)Src);
	return CastResult;
#endif // !DO_CHECK
}

template<typename FieldType>
FUNCTION_NON_NULL_RETURN_START
FORCEINLINE const FieldType* CastFieldChecked(const FField* Src)
FUNCTION_NON_NULL_RETURN_END
{
#if !DO_CHECK
	return (const FieldType*)Src;
#else
	const FieldType* CastResult = Src && Src->IsA<FieldType>() ? static_cast<const FieldType*>(Src) : nullptr;
	checkf(CastResult, TEXT("CastFieldChecked failed with 0x%016llx"), (int64)(PTRINT)Src);
	return CastResult;
#endif // !DO_CHECK
}

template<typename FieldType>
FORCEINLINE FieldType* CastFieldCheckedNullAllowed(FField* Src)
{
#if !DO_CHECK
	return (FieldType*)Src;
#else
	FieldType* CastResult = Src && Src->IsA<FieldType>() ? static_cast<FieldType*>(Src) : nullptr;
	checkf(CastResult || !Src, TEXT("CastFieldCheckedNullAllowed failed with 0x%016llx"), (int64)(PTRINT)Src);
	return CastResult;
#endif // !DO_CHECK
}

template<typename FieldType>
FORCEINLINE const FieldType* CastFieldCheckedNullAllowed(const FField* Src)
{
#if !DO_CHECK
	return (const FieldType*)Src;
#else
	const FieldType* CastResult = Src && Src->IsA<FieldType>() ? static_cast<const FieldType*>(Src) : nullptr;
	checkf(CastResult || !Src, TEXT("CastFieldCheckedNullAllowed failed with 0x%016llx"), (int64)(PTRINT)Src);
	return CastResult;
#endif // !DO_CHECK
}

/**
 * Helper function for serializing FField to an archive. This function fully serializes the field and its properties.
 */
template <typename FieldType>
inline void SerializeSingleField(FArchive& Ar, FieldType*& Field, FFieldVariant Owner)
{
	if (Ar.IsLoading())
	{
		FName PropertyTypeName;
		Ar << PropertyTypeName;
		if (PropertyTypeName != NAME_None)
		{
			Field = CastField<FieldType>(FField::Construct(PropertyTypeName, Owner, NAME_None, RF_NoFlags));
			check(Field);
			Field->Serialize(Ar);
		}
		else
		{
			Field = nullptr;
		}
	}
	else
	{
		FName PropertyTypeName = Field ? Field->GetClass()->GetFName() : NAME_None;		
		Ar << PropertyTypeName;
		if (Field)
		{
			Field->Serialize(Ar);
		}
	}
}

/**
 * Gets the name of the provided field. If the field pointer is null, the result is "none"
 */
inline FName GetFNameSafe(const FField* InField)
{
	if (InField)
	{
		return InField->GetFName();
	}
	else
	{
		return NAME_None;
	}
}
/**
 * Gets the name of the provided field. If the field pointer is null, the result is "none"
 */
inline FString GetNameSafe(const FField* InField)
{
	if (InField)
	{
		return InField->GetName();
	}
	else
	{
		return TEXT("none");
	}
}
/**
 * Gets the full name of the provided field. If the field pointer is null, the result is "none"
 */
COREUOBJECT_API FString GetFullNameSafe(const FField* InField);
/**
 * Gets the path name of the provided field. If the field pointer is null, the result is "none"
 */
COREUOBJECT_API FString GetPathNameSafe(const FField* InField);
/** 
 * Finds a field given a path to the field (Package.Class[:Subobject:...]:FieldName)
 */
COREUOBJECT_API FField* FindFPropertyByPath(const TCHAR* InFieldPath);
/**
 * Templated version of FindFieldByPath
 */
template <typename FieldType>
inline FieldType* FindFProperty(const TCHAR* InFieldPath)
{
	FField* FoundField = FindFPropertyByPath(InFieldPath);
	return CastField<FieldType>(FoundField);
}

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
