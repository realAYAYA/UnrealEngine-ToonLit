// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/ScriptArray.h"
#include "CoreTypes.h"
#include "HAL/CriticalSection.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "Templates/TypeHash.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectHandleTracking.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectRef.h"
#include "UObject/PackedObjectRef.h"


class UClass;
class UPackage;

/**
 * FObjectHandle is either a packed object ref or the resolved pointer to an object.  Depending on configuration
 * when you create a handle, it may immediately be resolved to a pointer.
 */
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE

namespace UE::CoreUObject::Private
{
	struct FObjectHandlePrivate;
}

using FObjectHandle = UE::CoreUObject::Private::FObjectHandlePrivate;

#else

using FObjectHandle = UObject*;
//NOTE: operator==, operator!=, GetTypeHash fall back to the default on UObject* or void* through coercion.

#endif

inline bool IsObjectHandleNull(FObjectHandle Handle);
inline bool IsObjectHandleResolved(FObjectHandle Handle);
inline bool IsObjectHandleTypeSafe(FObjectHandle Handle);

//Private functions that forced public due to inlining.
namespace UE::CoreUObject::Private
{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE

	struct FObjectHandlePrivate
	{
		//Stores either FPackedObjectRef or a UObject*
		UPTRINT PointerOrRef;

		explicit inline operator bool() const
		{
			return PointerOrRef != 0;
		}
	};

	/* Returns the packed object ref for this object IF one exists otherwise returns a null PackedObjectRef */
	COREUOBJECT_API FPackedObjectRef FindExistingPackedObjectRef(const UObject* Object);

	/* Creates and ObjectRef from a packed object ref*/
	COREUOBJECT_API FObjectRef MakeObjectRef(FPackedObjectRef Handle);

#endif

	///these functions are always defined regardless of UE_WITH_OBJECT_HANDLE_LATE_RESOLVE value

	/* Makes a resolved FObjectHandle from an UObject. */
	inline FObjectHandle MakeObjectHandle(UObject* Object);

	/* Returns the UObject from Handle and the handle is updated cache the resolved UObject */
	inline UObject* ResolveObjectHandle(FObjectHandle& Handle);

	/* Returns the UClass for UObject store in Handle. Handle is not resolved */
	inline UClass* ResolveObjectHandleClass(FObjectHandle Handle);

	/* Returns the UObject from Handle and the handle is updated cache the resolved UObject.
	 * Does not cause ObjectHandleTracking to fire a read event
	 */
	inline UObject* ResolveObjectHandleNoRead(FObjectHandle& Handle);

	/** Resolves an ObjectHandle without checking if already resolved. Invalid to call for resolved handles */
	inline UObject* ResolveObjectHandleNoReadNoCheck(FObjectHandle& Handle);

	/** Read the handle as a pointer without checking if it is resolved. Invalid to call for unresolved handles. */
	inline UObject* ReadObjectHandlePointerNoCheck(FObjectHandle Handle);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* return true if handle is null */
inline bool IsObjectHandleNull(FObjectHandle Handle)
{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	return !Handle.PointerOrRef;
#else
	return !Handle;
#endif
}

/* checks if a handle is resolved. 
 * nullptr counts as resolved
 * all handles are resolved when late resolved is off
 */
inline bool IsObjectHandleResolved(FObjectHandle Handle)
{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	return !(Handle.PointerOrRef & 1);
#else
	return true;
#endif
}

/* return true if a handle is type safe.
 * null and unresolved handles are considered safe
 */ 
inline bool IsObjectHandleTypeSafe(FObjectHandle Handle)
{
#if UE_WITH_OBJECT_HANDLE_TYPE_SAFETY
	return IsObjectHandleNull(Handle) || !IsObjectHandleResolved(Handle) || !UE::CoreUObject::Private::HasAnyFlags(UE::CoreUObject::Private::ReadObjectHandlePointerNoCheck(Handle), static_cast<int32>(RF_HasPlaceholderType));
#else
	return true;
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
inline bool operator==(UE::CoreUObject::Private::FObjectHandlePrivate LHS, UE::CoreUObject::Private::FObjectHandlePrivate RHS)
{
	using namespace UE::CoreUObject::Private;

	bool LhsResolved = IsObjectHandleResolved(LHS);
	bool RhsResolved = IsObjectHandleResolved(RHS);

	//if both resolved or both unresolved compare the uintptr
	if (LhsResolved == RhsResolved)
	{
		return LHS.PointerOrRef == RHS.PointerOrRef;
	}

	//only one side can be resolved
	if (LhsResolved)
	{
		//both sides can't be null as resolved status would have be true for both
		const UObject* Obj = ReadObjectHandlePointerNoCheck(LHS);
		if (!Obj)
		{
			return false;
		}

		//if packed ref empty then can't be equal as RHS is an unresolved pointer
		FPackedObjectRef PackedLhs = FindExistingPackedObjectRef(Obj);
		if (PackedLhs.EncodedRef == 0)
		{
			return false;
		}
		return PackedLhs.EncodedRef == RHS.PointerOrRef;

	}
	else
	{
		//both sides can't be null as resolved status would have be true for both
		const UObject* Obj = ReadObjectHandlePointerNoCheck(RHS);
		if (!Obj)
		{
			return false;
		}

		//if packed ref empty then can't be equal as RHS is an unresolved pointer
		FPackedObjectRef PackedRhs = FindExistingPackedObjectRef(Obj);
		if (PackedRhs.EncodedRef == 0)
		{
			return false;
		}
		return PackedRhs.EncodedRef == LHS.PointerOrRef;
	}

}

inline bool operator!=(UE::CoreUObject::Private::FObjectHandlePrivate LHS, UE::CoreUObject::Private::FObjectHandlePrivate RHS)
{
	return !(LHS == RHS);
}

inline uint32 GetTypeHash(UE::CoreUObject::Private::FObjectHandlePrivate Handle)
{
	using namespace UE::CoreUObject::Private;

	if (Handle.PointerOrRef == 0)
	{
		return 0;
	}

	if (IsObjectHandleResolved(Handle))
	{
		const UObject* Obj = ReadObjectHandlePointerNoCheck(Handle);

		FPackedObjectRef PackedObjectRef = FindExistingPackedObjectRef(Obj);
		if (PackedObjectRef.EncodedRef == 0)
		{
			return GetTypeHash(Obj);
		}
		return GetTypeHash(PackedObjectRef.EncodedRef);
	}
	return GetTypeHash(Handle.PointerOrRef);
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace UE::CoreUObject::Private
{
	inline FObjectHandle MakeObjectHandle(UObject* Object)
	{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
		{ return { UPTRINT(Object) }; }
#else
		return Object;
#endif
	}

	inline UObject* ResolveObjectHandle(FObjectHandle& Handle)
	{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE || UE_WITH_OBJECT_HANDLE_TRACKING
		UObject* ResolvedObject = ResolveObjectHandleNoRead(Handle);
		UE::CoreUObject::Private::OnHandleRead(ResolvedObject);
		return ResolvedObject;
#else
		return ReadObjectHandlePointerNoCheck(Handle);
#endif
	}

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	inline FPackedObjectRef ReadObjectHandlePackedObjectRefNoCheck(FObjectHandle Handle)
	{
		return { Handle.PointerOrRef };
	}
#endif

	inline UClass* ResolveObjectHandleClass(FObjectHandle Handle)
	{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
		if (IsObjectHandleResolved(Handle))
		{
			UObject* Obj = ReadObjectHandlePointerNoCheck(Handle);
			return Obj != nullptr ? UE::CoreUObject::Private::GetClass(Obj) : nullptr;
		}
		else
		{
			// @TODO: OBJPTR: This should be cached somewhere instead of resolving on every call
			FPackedObjectRef PackedObjectRef = ReadObjectHandlePackedObjectRefNoCheck(Handle);
			FObjectRef ObjectRef = MakeObjectRef(PackedObjectRef);
			return ObjectRef.ResolveObjectRefClass();
		}
#else
		UObject* Obj = ReadObjectHandlePointerNoCheck(Handle);
		return Obj != nullptr ? UE::CoreUObject::Private::GetClass(Obj) : nullptr;
#endif
	}

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
			FPackedObjectRef PackedObjectRef = ReadObjectHandlePackedObjectRefNoCheck(LocalHandle);
			FObjectRef ObjectRef = MakeObjectRef(PackedObjectRef);
			UObject* ResolvedObject = ObjectRef.Resolve();
			Handle = MakeObjectHandle(ResolvedObject);
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
		FPackedObjectRef PackedObjectRef = ReadObjectHandlePackedObjectRefNoCheck(LocalHandle);
		FObjectRef ObjectRef = MakeObjectRef(PackedObjectRef);
		UObject* ResolvedObject = ObjectRef.Resolve();
		LocalHandle = MakeObjectHandle(ResolvedObject);
		Handle = LocalHandle;
		return ResolvedObject;
#else
		return ReadObjectHandlePointerNoCheck(Handle);
#endif
	}

	inline UObject* ReadObjectHandlePointerNoCheck(FObjectHandle Handle)
	{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
		return reinterpret_cast<UObject*>(Handle.PointerOrRef);
#else
		return Handle;
#endif
	}

	//Natvis structs
	struct FObjectHandlePackageDebugData
	{
		FMinimalName PackageName;
		FScriptArray ObjectDescriptors;
		uint8 _Padding[sizeof(FRWLock)];
	};

	struct FObjectHandleDataClassDescriptor
	{
		FMinimalName PackageName;
		FMinimalName ClassName;
	};

	struct FObjectPathIdDebug
	{
		uint32 Index = 0;
		uint32 Number = 0;

		static constexpr uint32 WeakObjectMask = ~((~0u) >> 1);       //most significant bit
		static constexpr uint32 SimpleNameMask = WeakObjectMask >> 1; //second most significant bits
	};

	struct FObjectDescriptorDebug
	{
		FObjectPathIdDebug ObjectPath;
		FObjectHandleDataClassDescriptor ClassDescriptor;
	};

	struct FStoredObjectPathDebug
	{
		static constexpr const int32 NumInlineElements = 3;
		int32 NumElements;

		union
		{
			FMinimalName Short[NumInlineElements];
			FMinimalName* Long;
		};
	};

	inline constexpr uint32 ObjectIdShift = 1;
	inline constexpr uint32 PackageIdShift = 33;
	inline constexpr uint32 PackageIdMask = 0x7FFF'FFFF;

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
	//forward declarations
	void InitObjectHandles(int32 Size);
	void FreeObjectHandle(const UObjectBase* Object);
	void UpdateRenamedObject(const UObject* Obj, FName NewName, UObject* NewOuter);
	UE::CoreUObject::Private::FPackedObjectRef MakePackedObjectRef(const UObject* Object);
#endif
}
