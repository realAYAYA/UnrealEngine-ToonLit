// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "UObject/Interface.h"
#include "UObject/ObjectKey.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "TypedElementDataStorageCompatibilityInterface.generated.h"

class AActor;

UINTERFACE(MinimalAPI)
class UTypedElementDataStorageCompatibilityInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface to provide compatibility with existing systems that don't directly
 * support the data storage.
 */
class ITypedElementDataStorageCompatibilityInterface
{
	GENERATED_BODY()

public:
	/**
	 * @section Type-agnostic functions
	 * These allow compatibility with any type. These do eventually fall back to the explicit versions.
	 * Any references given are non-owning so it's up to the caller to deregister the object after it's no longer
	 * available.
	 */

	/** 
	 * Adds a reference to an existing object to the data storage. The data storage does NOT take ownership of the object and
	 * the caller is responsible for managing the life cycle of the object. The address is only used for associating the object
	 * with a row and to setup the initial row data.
	 */
	template<typename ObjectType>
	TypedElementRowHandle AddCompatibleObject(ObjectType Object);
	template<typename ObjectType>
	TypedElementRowHandle AddCompatibleObject(ObjectType Object, TypedElementTableHandle Table);

	/** Removes a previously registered object from the data storage. */
	template<typename ObjectType>
	void RemoveCompatibleObject(ObjectType Object);

	template<typename ObjectType>
	TypedElementRowHandle FindRowWithCompatibleObject(const ObjectType Object) const;


	/**
	 * @section Explicit functions
	 * These are functions that work on specific types.
	 */

	/** Adds a UObject to the data storage. */
	virtual TypedElementRowHandle AddCompatibleObjectExplicit(UObject* Object) = 0;
	virtual TypedElementRowHandle AddCompatibleObjectExplicit(UObject* Object, TypedElementTableHandle Table) = 0;
	/** Adds an actor to the data storage. */
	virtual TypedElementRowHandle AddCompatibleObjectExplicit(AActor* Actor) = 0;
	virtual TypedElementRowHandle AddCompatibleObjectExplicit(AActor* Actor, TypedElementTableHandle Table) = 0;
	/** Adds an FStruct to the data storage. */
	virtual TypedElementRowHandle AddCompatibleObjectExplicit(void* Object, TWeakObjectPtr<const UScriptStruct> TypeInfo) = 0;
	virtual TypedElementRowHandle AddCompatibleObjectExplicit(void* Object, TWeakObjectPtr<const UScriptStruct> TypeInfo, TypedElementTableHandle Table) = 0;

	/** Removes a UObject from the data storage. */
	virtual void RemoveCompatibleObjectExplicit(UObject* Object) = 0;
	/** Removes an actor from the data storage. */
	virtual void RemoveCompatibleObjectExplicit(AActor* Actor) = 0;
	/** Removes an FStruct from the data storage. */
	virtual void RemoveCompatibleObjectExplicit(void* Object) = 0;

	/** Finds a previously stored UObject. If not found an invalid row handle will be returned. */
	virtual TypedElementRowHandle FindRowWithCompatibleObjectExplicit(const UObject* Object) const = 0;
	/** 
	 * Finds a previously stored actor based on the object key. If not found an invalid row handle will be returned.
	 * While FindRowWithCompatibleObject(const TObjectKey<const UObject> Object) can also be used, this call will be slightly
	 * faster if it's already known that the target is an actor.
	 */
	virtual TypedElementRowHandle FindRowWithCompatibleObjectExplicit(const AActor* Actor) const = 0;
	/** Finds a previously stored FStructe . If not found an invalid row handle will be returned. */
	virtual TypedElementRowHandle FindRowWithCompatibleObjectExplicit(const void* Object) const = 0;
};

template<typename Type> Type* GetRawPointer(const TWeakObjectPtr<Type> Object)	{ return Object.Get(); }
template<typename Type> Type* GetRawPointer(const TObjectPtr<Type> Object)		{ return Object.Get(); }
template<typename Type> Type* GetRawPointer(const TStrongObjectPtr<Type> Object){ return Object.Get(); }
template<typename Type> Type* GetRawPointer(const TObjectKey<Type> Object)		{ return Object.ResolveObjectPtr(); }
template<typename Type> Type* GetRawPointer(const TUniquePtr<Type> Object)		{ return Object.Get(); }
template<typename Type> Type* GetRawPointer(const TSharedPtr<Type> Object)		{ return Object.Get(); }
template<typename Type> Type* GetRawPointer(Type* Object)						{ return Object; }
template<typename Type> Type* GetRawPointer(Type& Object)						{ return &Object; }

template<typename ObjectType>
TypedElementRowHandle ITypedElementDataStorageCompatibilityInterface::AddCompatibleObject(ObjectType Object)
{
	auto RawPointer = GetRawPointer(Object);
	using BaseType = std::remove_cv_t<std::remove_pointer_t<decltype(RawPointer)>>;

	if constexpr (std::is_same_v<BaseType, AActor> || std::is_same_v<BaseType, UObject>)
	{
		return AddCompatibleObjectExplicit(RawPointer);
	}
	else
	{
		return AddCompatibleObjectExplicit(RawPointer, BaseType::StaticStruct());
	}
}

template<typename ObjectType>
TypedElementRowHandle ITypedElementDataStorageCompatibilityInterface::AddCompatibleObject(ObjectType Object, TypedElementTableHandle Table)
{
	auto RawPointer = GetRawPointer(Object);
	using BaseType = std::remove_cv_t<std::remove_pointer_t<decltype(RawPointer)>>;

	if constexpr (std::is_same_v<BaseType, AActor> || std::is_same_v<BaseType, UObject>)
	{
		return AddCompatibleObjectExplicit(RawPointer, Table);
	}
	else
	{
		return AddCompatibleObjectExplicit(RawPointer, BaseType::StaticStruct(), Table);
	}
}

template<typename ObjectType>
void ITypedElementDataStorageCompatibilityInterface::RemoveCompatibleObject(ObjectType Object)
{
	RemoveCompatibleObjectExplicit(GetRawPointer(Object));
}

template<typename ObjectType>
TypedElementRowHandle ITypedElementDataStorageCompatibilityInterface::FindRowWithCompatibleObject(const ObjectType Object) const
{
	return FindRowWithCompatibleObjectExplicit(GetRawPointer(Object));
}

template<typename Subsystem>
struct TTypedElementSubsystemTraits final
{
	template<typename T, typename = void>
	struct HasRequiresGameThreadVariable 
	{ 
		static constexpr bool bAvailable = false; 
	};
	template<typename T>
	struct HasRequiresGameThreadVariable <T, decltype((void)T::bRequiresGameThread)>
	{ 
		static constexpr bool bAvailable = true; 
	};

	template<typename T, typename = void>
	struct HasIsHotReloadableVariable
	{ 
		static constexpr bool bAvailable = false;
	};
	template<typename T>
	struct HasIsHotReloadableVariable <T, decltype((void)T::bIsHotReloadable)>
	{ 
		static constexpr bool bAvailable = true;
	};

	static constexpr bool RequiresGameThread()
	{
		if constexpr (HasRequiresGameThreadVariable<Subsystem>::bAvailable)
		{
			return Subsystem::bRequiresGameThread;
		}
		else
		{
			static_assert(HasRequiresGameThreadVariable<Subsystem>::bAvailable, "Subsystem provided to the Typed Elements did not "
				"have a 'static constexpr bool bRequiresGameThread = true|false` declared or have a specialization for "
				"TTypedElementSubsystemTraits.");
			return true;
		}
	}

	static constexpr bool IsHotReloadable()
	{
		if constexpr (HasIsHotReloadableVariable<Subsystem>::bAvailable)
		{
			return Subsystem::bIsHotReloadable;
		}
		else
		{
			static_assert(HasIsHotReloadableVariable<Subsystem>::bAvailable, "Subsystem provided to the Typed Elements did not "
				"have a 'static constexpr bool bIsHotReloadable = true|false` declared or have a specialization for "
				"TTypedElementSubsystemTraits.");
			return false;
		}
	}
};
