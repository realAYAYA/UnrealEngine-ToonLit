// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/ScriptArray.h"
#include "CoreTypes.h"
#include "HAL/CriticalSection.h"
#include "HAL/Platform.h"
#include "Logging/LogMacros.h"
#include "Misc/AssertionMacros.h"
#include "Templates/TypeHash.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPathId.h"

class UClass;
class UPackage;

#define UE_WITH_OBJECT_HANDLE_LATE_RESOLVE WITH_EDITORONLY_DATA
#define UE_WITH_OBJECT_HANDLE_TRACKING WITH_EDITORONLY_DATA

DECLARE_LOG_CATEGORY_EXTERN(LogObjectHandle, Log, All);

/**
 * FObjectRef represents a heavyweight reference that contains the specific pieces of information needed to reference an object
 * (or null) that may or may not be loaded yet.  The expectation is that given the imports of a package we have loaded, we should
 * be able to create an FObjectRef to objects in a package we haven't yet loaded.  For this reason, FObjectRef has to be derivable
 * from the serialized (not transient) contents of an FObjectImport.
 */
struct FObjectRef
{
	FName PackageName;
	FName ClassPackageName;
	FName ClassName;
	FObjectPathId ObjectPath;

	bool operator==(const FObjectRef&& Other) const
	{
		return (PackageName == Other.PackageName) && (ObjectPath == Other.ObjectPath) && (ClassPackageName == Other.ClassPackageName) && (ClassName == Other.ClassName);
	}

	/** Returns the path to the class for this object. */
	FString GetClassPathName(EObjectFullNameFlags Flags = EObjectFullNameFlags::IncludeClassPackage) const
	{
		TStringBuilder<FName::StringBufferSize> Result;
		AppendClassPathName(Result, Flags);
		return FString(Result);
	}

	/** Appends the path to the class for this object to the builder, does not reset builder. */
	COREUOBJECT_API void AppendClassPathName(FStringBuilderBase& OutClassPathNameBuilder, EObjectFullNameFlags Flags = EObjectFullNameFlags::IncludeClassPackage) const;

	/** Returns the name of the object in the form: ObjectName */
	COREUOBJECT_API FName GetFName() const;

	/** Returns the full path for the object in the form: ObjectPath */
	FString GetPathName() const
	{
		TStringBuilder<FName::StringBufferSize> Result;
		AppendPathName(Result);
		return FString(Result);
	}

	/** Appends the path to the object to the builder, does not reset builder. */
	COREUOBJECT_API void AppendPathName(FStringBuilderBase& OutPathNameBuilder) const;

	/** Returns the full name for the object in the form: Class ObjectPath */
	FString GetFullName(EObjectFullNameFlags Flags = EObjectFullNameFlags::None) const
	{
		TStringBuilder<256> FullName;
		GetFullName(FullName, Flags);
		return FString(FullName);
	}

	/** Populates OutFullNameBuilder with the full name for the object in the form: Class ObjectPath */
	void GetFullName(FStringBuilderBase& OutFullNameBuilder, EObjectFullNameFlags Flags = EObjectFullNameFlags::None) const
	{
		OutFullNameBuilder.Reset();
		AppendClassPathName(OutFullNameBuilder, Flags);
		OutFullNameBuilder.AppendChar(TEXT(' '));
		AppendPathName(OutFullNameBuilder);
	}

	/** Returns the name for the asset in the form: Class'ObjectPath' */
	FString GetExportTextName() const
	{
		TStringBuilder<256> ExportTextName;
		GetExportTextName(ExportTextName);
		return FString(ExportTextName);
	}

	/** Populates OutExportTextNameBuilder with the name for the object in the form: Class'ObjectPath' */
	void GetExportTextName(FStringBuilderBase& OutExportTextNameBuilder) const
	{
		OutExportTextNameBuilder.Reset();
		AppendClassPathName(OutExportTextNameBuilder);
		OutExportTextNameBuilder.AppendChar(TEXT('\''));
		AppendPathName(OutExportTextNameBuilder);
		OutExportTextNameBuilder.AppendChar(TEXT('\''));
	}
};

inline bool IsObjectRefNull(const FObjectRef& ObjectRef) { return ObjectRef.PackageName.IsNone() && ObjectRef.ObjectPath.IsNone(); }

COREUOBJECT_API FObjectRef MakeObjectRef(const UObject* Object);
COREUOBJECT_API FObjectRef MakeObjectRef(struct FPackedObjectRef ObjectRef);
COREUOBJECT_API UObject* ResolveObjectRef(const FObjectRef& ObjectRef, uint32 LoadFlags = LOAD_None);
COREUOBJECT_API UClass* ResolveObjectRefClass(const FObjectRef& ObjectRef, uint32 LoadFlags = LOAD_None);


/**
 * FPackedObjectRef represents a lightweight reference that can fit in the space of a pointer and be able to refer to an object
 * (or null) that may or may not be loaded without pointing to its location in memory (even if it is currently loaded).
 */
struct FPackedObjectRef
{
	// Must be 0 for a reference to null.
	// The least significant bit must always be 1 in a non-null reference.
	UPTRINT EncodedRef;
};

inline bool IsPackedObjectRefNull(FPackedObjectRef ObjectRef) { return !ObjectRef.EncodedRef; }

COREUOBJECT_API FPackedObjectRef MakePackedObjectRef(const UObject* Object);
COREUOBJECT_API FPackedObjectRef MakePackedObjectRef(const FObjectRef& ObjectRef);
COREUOBJECT_API UObject* ResolvePackedObjectRef(FPackedObjectRef ObjectRef, uint32 LoadFlags = LOAD_None);
COREUOBJECT_API UClass* ResolvePackedObjectRefClass(FPackedObjectRef ObjectRef, uint32 LoadFlags = LOAD_None);

inline bool operator==(FPackedObjectRef LHS, FPackedObjectRef RHS) { return LHS.EncodedRef == RHS.EncodedRef; }
inline bool operator!=(FPackedObjectRef LHS, FPackedObjectRef RHS) { return LHS.EncodedRef != RHS.EncodedRef; }
inline uint32 GetTypeHash(FPackedObjectRef ObjectRef) { return GetTypeHash(ObjectRef.EncodedRef); }

/**
 * FObjectHandle is either a packed object ref or the resolved pointer to an object.  Depending on configuration
 * when you create a handle, it may immediately be resolved to a pointer.
 */
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE

struct FObjectHandleInternal
{
	UPTRINT PointerOrRef;
};
using FObjectHandle = FObjectHandleInternal;

inline bool operator==(FObjectHandle LHS, FObjectHandle RHS);
inline bool operator!=(FObjectHandle LHS, FObjectHandle RHS);
inline uint32 GetTypeHash(FObjectHandle Handle);

#else

using FObjectHandle = UObject*;
//NOTE: operator==, operator!=, GetTypeHash fall back to the default on UObject* or void* through coercion.

#endif

inline FObjectHandle MakeObjectHandle(FPackedObjectRef ObjectRef);
inline FObjectHandle MakeObjectHandle(const FObjectRef& ObjectRef);
inline FObjectHandle MakeObjectHandle(UObject* Object);

inline bool IsObjectHandleNull(FObjectHandle Handle);
inline bool IsObjectHandleResolved(FObjectHandle Handle);

/** Read the handle as a pointer without checking if it is resolved. Invalid to call for unresolved handles. */
inline UObject* ReadObjectHandlePointerNoCheck(FObjectHandle Handle);
/** Read the handle as a packed object ref without checking if it is unresolved. Invalid to call for resolved handles. */
inline FPackedObjectRef ReadObjectHandlePackedObjectRefNoCheck(FObjectHandle Handle);

inline UObject* ResolveObjectHandle(FObjectHandle& Handle);
inline UObject* ResolveObjectHandleNoRead(FObjectHandle& Handle);

/** Resolves an ObjectHandle without checking if already resolved. Invalid to call for resolved handles */
inline UObject* ResolveObjectHandleNoReadNoCheck(FObjectHandle& Handle);
inline UClass* ResolveObjectHandleClass(FObjectHandle Handle);

/** Read the handle as a pointer if resolved, and otherwise return null. */
inline UObject* ReadObjectHandlePointer(FObjectHandle Handle);
/** Read the handle as a packed object ref if unresolved, and otherwise return the null packed object ref. */
inline FPackedObjectRef ReadObjectHandlePackedObjectRef(FObjectHandle Handle);

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
inline FObjectRef MakeObjectRef(FObjectHandle Handle);
inline FPackedObjectRef MakePackedObjectRef(FObjectHandle Handle);
#endif


/**
 * FObjectHandles can optionally support tracking.  Because of the low level nature of object handles, anything that
 * registers itself for these callbacks should ensure that it is:
 * 1) error free (ie: should not cause exceptions even in unusual circumstances)
 * 2) fault tolerant (ie: could be called at a time when an exception has happened)
 * 3) thread-safe (ie: could be called from any thread)
 * 4) high performance (ie: will be called many times)
 */
#if UE_WITH_OBJECT_HANDLE_TRACKING

 /**
  * Callback notifying when an object value is read from a handle.  Fired regardless of whether the handle
  * was resolved as part of the read operation or not and whether the object being read is null or not.
  *
  * @param ReadObject	The object that was read from a handle.
  */
DECLARE_DELEGATE_OneParam(FObjectHandleReadDelegate, UObject* ReadObject);

/**
 * Callback notifying when a class is resolved from an object handle or object reference.
 * Classes are resolved either independently for a given handle/reference or as part of each object resolve.
 *
 * @param SourceRef		The object reference (either user provided or internally computed from a handle) from which the class was resolved.
 * @param ClassPackage	The package containing the resolved class.
 * @param Class			The resolved class.
 */
DECLARE_DELEGATE_ThreeParams(FObjectHandleClassResolvedDelegate, const FObjectRef& SourceRef, UPackage* ObjectPackage, UClass* Class);

 /**
  * Callback notifying when a object handle is resolved.
  *
  * @param SourceRef		The object reference (either user provided or internally computed from a handle) from which the class was resolved.
  * @param ClassPackage	The package containing the resolved class.
  * @param Class			The resolved class.
  */
DECLARE_DELEGATE_ThreeParams(FObjectHandleReferenceResolvedDelegate, const FObjectRef& SourceRef, UPackage* ObjectPackage, UObject* Object);

/**
 * Callback notifying when an object was loaded through an object handle.  Will not notify you about global object loads, just ones that occur
 * as the byproduct of resolving an ObjectHandle.
 *
 * @param SourceRef		The object reference (either user provided or internally computed from a handle) from which the class was resolved.
 * @param ClassPackage	The package containing the resolved class.
 * @param Class			The resolved class.
 */
DECLARE_DELEGATE_ThreeParams(FObjectHandleReferenceLoadedDelegate, const FObjectRef& SourceRef, UPackage* ObjectPackage, UObject* Object);

/**
 * Installs a new callback for notifications that an object value has been read from a handle.
 *
 * @param Function		The new handle read callback to install.
 * @return				The DelegateHandle so that you can remove the callback at a later date.
 */
COREUOBJECT_API FDelegateHandle AddObjectHandleReadCallback(FObjectHandleReadDelegate Delegate);
COREUOBJECT_API void RemoveObjectHandleReadCallback(FDelegateHandle DelegateHandle);

/**
 * Installs a new callback for notifications that a class has been resolved from an object handle or object reference.
 *
 * @param Function		The new class resolved callback to install.
 * @return				The DelegateHandle so that you can remove the callback at a later date.
 */
COREUOBJECT_API FDelegateHandle AddObjectHandleClassResolvedCallback(FObjectHandleClassResolvedDelegate Callback);
COREUOBJECT_API void RemoveObjectHandleClassResolvedCallback(FDelegateHandle DelegateHandle);

/**
 * Installs a new callback for notifications that an object has been resolved from an object handle or object reference.
 *
 * @param Function		The new object resolved callback to install.
 * @return				The DelegateHandle so that you can remove the callback at a later date.
 */
COREUOBJECT_API FDelegateHandle AddObjectHandleReferenceResolvedCallback(FObjectHandleReferenceResolvedDelegate Callback);
COREUOBJECT_API void RemoveObjectHandleReferenceResolvedCallback(FDelegateHandle DelegateHandle);

/**
 * Installs a new callback for notifications that an object has been loaded from an object handle or object reference.
 *
 * @param Function		The new object resolved callback to install.
 * @return				The DelegateHandle so that you can remove the callback at a later date.
 */
COREUOBJECT_API FDelegateHandle AddObjectHandleReferenceLoadedCallback(FObjectHandleReferenceLoadedDelegate Callback);
COREUOBJECT_API void RemoveObjectHandleReferenceLoadedCallback(FDelegateHandle DelegateHandle);
#endif


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct FObjectHandlePackageDebugData
{
	FMinimalName PackageName;
	FScriptArray ObjectPaths;
	FScriptArray DataClassDescriptors;
	uint8 _Padding[sizeof(FRWLock) + sizeof(FScriptMap)];
};

struct FObjectHandleDataClassDescriptor
{
	FMinimalName PackageName;
	FMinimalName ClassName;
};

namespace ObjectHandle_Private
{
	constexpr uint32 ObjectPathIdShift = 1;
	constexpr uint32 ObjectPathIdMask = 0x00FF'FFFF;

	constexpr uint32 DataClassDescriptorIdShift = 25;
	constexpr uint32 DataClassDescriptorIdMask = 0x0000'00FF;

	constexpr uint32 PackageIdShift = 33;
	constexpr uint32 PackageIdMask = 0x7FFF'FFFF;

#if UE_WITH_OBJECT_HANDLE_TRACKING
	DECLARE_MULTICAST_DELEGATE_OneParam(FObjectHandleReadEvent, UObject* Object);
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FClassReferenceResolvedEvent, const FObjectRef& ObjectRef, UPackage* Package, UClass* Class);
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FObjectHandleReferenceResolvedEvent, const FObjectRef& SourceRef, UPackage* ObjectPackage, UObject* Object);
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FObjectHandleReferenceLoadedEvent, const FObjectRef& SourceRef, UPackage* ObjectPackage, UObject* Object);

	struct FObjectHandleEvents
	{
		FObjectHandleReadEvent ObjectHandleReadEvent;
		FClassReferenceResolvedEvent ClassReferenceResolvedEvent;
		FObjectHandleReferenceResolvedEvent ObjectHandleReferenceResolvedEvent;
		FObjectHandleReferenceLoadedEvent ObjectHandleReferenceLoadedEvent;

		inline void BeginUsing() { UsingCount++; }
		inline void EndUsing() { --UsingCount; }
		inline bool IsUsing() const { return UsingCount > 0; }

	private:
		std::atomic<int32> UsingCount;
	};

	extern COREUOBJECT_API std::atomic<int32> ObjectHandleEventIndex;
	extern COREUOBJECT_API FObjectHandleEvents ObjectHandleEvents[2];

	inline FObjectHandleEvents& BeginReadingEvents()
	{
		// Quick spin lock to try and begin using the events, normally this wont actually spin, in like 99.999% of the time.
		while (true)
		{
			// Start by getting the current event index from the double buffer.
			const int32 InitialEventIndex = ObjectHandleEventIndex;
			// Grab the event set at that index.
			FObjectHandleEvents& Events = ObjectHandleEvents[InitialEventIndex];
			// Begin using the events, this will signal we're using them.
			Events.BeginUsing();
			// Ok - now we're going to check that we're *still* using the same index.  If we are
			// then we know that it didn't change out from under us in between getting Events, and calling BeginUsing().
			if (InitialEventIndex == ObjectHandleEventIndex)
			{
				return Events;
			}
			// If the check above failed, then we need to cease using the events, because we're about to try this again.
			Events.EndUsing();
		}
	}

	inline void OnHandleRead(UObject* Object)
	{
		FObjectHandleEvents& Events = BeginReadingEvents();
		Events.ObjectHandleReadEvent.Broadcast(Object);
		Events.EndUsing();
	}
	
	inline void OnClassReferenceResolved(const FObjectRef& ObjectRef, UPackage* Package, UClass* Class)
	{
		FObjectHandleEvents& Events = BeginReadingEvents();
		Events.ClassReferenceResolvedEvent.Broadcast(ObjectRef, Package, Class);
		Events.EndUsing();
	}

	inline void OnReferenceResolved(const FObjectRef& ObjectRef, UPackage* Package, UObject* Object)
	{
		FObjectHandleEvents& Events = BeginReadingEvents();
		Events.ObjectHandleReferenceResolvedEvent.Broadcast(ObjectRef, Package, Object);
		Events.EndUsing();
	}

	inline void OnReferenceLoaded(const FObjectRef& ObjectRef, UPackage* Package, UObject* Object)
	{
		FObjectHandleEvents& Events = BeginReadingEvents();
		Events.ObjectHandleReferenceLoadedEvent.Broadcast(ObjectRef, Package, Object);
		Events.EndUsing();
	}
#else
	inline void OnHandleRead(UObject* Object) {}
	inline void OnClassReferenceResolved(const FObjectRef& ObjectRef, UPackage* Package, UClass* Class) {}
	inline void OnReferenceResolved(const FObjectRef& ObjectRef, UPackage* Package, UObject* Object) {}
	inline void OnReferenceLoaded(const FObjectRef& ObjectRef, UPackage* Package, UObject* Object) {}
#endif
}

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE

inline bool IsObjectHandleNull(FObjectHandle Handle) { return !Handle.PointerOrRef; }
inline bool IsObjectHandleResolved(FObjectHandle Handle) { return !(Handle.PointerOrRef & 1); }

inline UObject* ReadObjectHandlePointerNoCheck(FObjectHandle Handle)
{
	return reinterpret_cast<UObject*>(Handle.PointerOrRef);
}

inline FPackedObjectRef ReadObjectHandlePackedObjectRefNoCheck(FObjectHandle Handle)
{
	return {Handle.PointerOrRef};
}

inline FObjectHandle MakeObjectHandle(FPackedObjectRef ObjectRef) { return {ObjectRef.EncodedRef}; }
inline FObjectHandle MakeObjectHandle(const FObjectRef& ObjectRef) { return MakeObjectHandle(MakePackedObjectRef(ObjectRef)); }
inline FObjectHandle MakeObjectHandle(UObject* Object) { return {UPTRINT(Object)}; }

inline bool operator==(FObjectHandle LHS, FObjectHandle RHS)
{
	if (IsObjectHandleResolved(LHS) == IsObjectHandleResolved(RHS))
	{
		return LHS.PointerOrRef == RHS.PointerOrRef;
	}
	else
	{
		return MakePackedObjectRef(LHS) == MakePackedObjectRef(RHS);
	}
}
inline bool operator!=(FObjectHandle LHS, FObjectHandle RHS)
{
	return !(LHS == RHS);
}

inline uint32 GetTypeHash(FObjectHandle Handle)
{
	checkf(IsObjectHandleResolved(Handle), TEXT("Cannot hash an unresolved handle."));
	return GetTypeHash(ReadObjectHandlePointerNoCheck(Handle));
}

#else

inline bool IsObjectHandleNull(FObjectHandle Handle) { return !Handle; }
inline bool IsObjectHandleResolved(FObjectHandle Handle) { return true; }

inline UObject* ReadObjectHandlePointerNoCheck(FObjectHandle Handle) { return Handle; }
inline FPackedObjectRef ReadObjectHandlePackedObjectRefNoCheck(FObjectHandle Handle) { return FPackedObjectRef(); }

inline FObjectHandle MakeObjectHandle(FPackedObjectRef ObjectRef) { return ResolvePackedObjectRef(ObjectRef); }
inline FObjectHandle MakeObjectHandle(const FObjectRef& ObjectRef) { return ResolveObjectRef(ObjectRef); }
inline FObjectHandle MakeObjectHandle(UObject* Object) { return Object; }

#endif

inline UObject* ResolveObjectHandleNoRead(FObjectHandle& Handle)
{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	FObjectHandle LocalHandle = Handle;
	if (IsObjectHandleResolved(LocalHandle))
	{
		UObject* ResolvedObject = ReadObjectHandlePointerNoCheck(LocalHandle);
		return ResolvedObject;
	}
	else
	{
		LocalHandle = MakeObjectHandle(ResolvePackedObjectRef(ReadObjectHandlePackedObjectRefNoCheck(LocalHandle)));
		UObject* ResolvedObject = ReadObjectHandlePointerNoCheck(LocalHandle);
		Handle = LocalHandle;
		return ResolvedObject;
	}
#else
	return ReadObjectHandlePointerNoCheck(Handle);
#endif
}

inline UObject* ResolveObjectHandleNoReadNoCheck(FObjectHandle& Handle)
{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	FObjectHandle LocalHandle = Handle;
	LocalHandle = MakeObjectHandle(ResolvePackedObjectRef(ReadObjectHandlePackedObjectRefNoCheck(LocalHandle)));
	UObject* ResolvedObject = ReadObjectHandlePointerNoCheck(LocalHandle);
	Handle = LocalHandle;
	return ResolvedObject;
	
#else
	return ReadObjectHandlePointerNoCheck(Handle);
#endif
}

inline UObject* ResolveObjectHandle(FObjectHandle& Handle)
{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE || UE_WITH_OBJECT_HANDLE_TRACKING
	UObject* ResolvedObject = ResolveObjectHandleNoRead(Handle);
	ObjectHandle_Private::OnHandleRead(ResolvedObject);
	return ResolvedObject;
#else
	return ReadObjectHandlePointerNoCheck(Handle);
#endif
}

inline UClass* ResolveObjectHandleClass(FObjectHandle Handle)
{
	if (IsObjectHandleResolved(Handle))
	{
		UObject* Obj = ReadObjectHandlePointerNoCheck(Handle);
		return Obj != nullptr ? Obj->GetClass() : nullptr;
	}
	else
	{
		// @TODO: OBJPTR: This should be cached somewhere instead of resolving on every call
		return ResolvePackedObjectRefClass(ReadObjectHandlePackedObjectRefNoCheck(Handle));
	}
}

inline UObject* ReadObjectHandlePointer(FObjectHandle Handle)
{
	if (IsObjectHandleResolved(Handle))
	{
		UObject* ResolvedObject = ReadObjectHandlePointerNoCheck(Handle);
		ObjectHandle_Private::OnHandleRead(ResolvedObject);
		return ResolvedObject;
	}
	return nullptr;
}

inline FPackedObjectRef ReadObjectHandlePackedObjectRef(FObjectHandle Handle)
{
	return !IsObjectHandleResolved(Handle) ? ReadObjectHandlePackedObjectRefNoCheck(Handle) : FPackedObjectRef();
}

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
inline FObjectRef MakeObjectRef(FObjectHandle Handle)
{
	if (IsObjectHandleResolved(Handle))
	{
		return MakeObjectRef(ReadObjectHandlePointerNoCheck(Handle));
	}
	else
	{
		return MakeObjectRef(ReadObjectHandlePackedObjectRefNoCheck(Handle));
	}
}

inline FPackedObjectRef MakePackedObjectRef(FObjectHandle Handle)
{
	if (IsObjectHandleResolved(Handle))
	{
		return MakePackedObjectRef(ReadObjectHandlePointerNoCheck(Handle));
	}
	else
	{
		return ReadObjectHandlePackedObjectRefNoCheck(Handle);
	}
}
#endif
