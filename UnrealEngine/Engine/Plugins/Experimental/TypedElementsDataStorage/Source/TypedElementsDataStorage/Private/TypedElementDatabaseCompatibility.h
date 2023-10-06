// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageCompatibilityInterface.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtr.h"

#include "TypedElementDatabaseCompatibility.generated.h"

class AActor;
class ITypedElementDataStorageInterface;
struct FMassActorManager;

UCLASS()
class TYPEDELEMENTSDATASTORAGE_API UTypedElementDatabaseCompatibility
	: public UObject
	, public ITypedElementDataStorageCompatibilityInterface
{
	GENERATED_BODY()
public:
	~UTypedElementDatabaseCompatibility() override = default;

	void Initialize(ITypedElementDataStorageInterface* StorageInterface);
	void Deinitialize();

	TypedElementRowHandle AddCompatibleObjectExplicit(UObject* Object) override;
	TypedElementRowHandle AddCompatibleObjectExplicit(UObject* Object, TypedElementTableHandle Table) override;
	TypedElementRowHandle AddCompatibleObjectExplicit(AActor* Actor) override;
	TypedElementRowHandle AddCompatibleObjectExplicit(AActor* Actor, TypedElementTableHandle Table) override;
	TypedElementRowHandle AddCompatibleObjectExplicit(void* Object, TWeakObjectPtr<const UScriptStruct> TypeInfo);
	TypedElementRowHandle AddCompatibleObjectExplicit(void* Object, TWeakObjectPtr<const UScriptStruct> TypeInfo, TypedElementTableHandle Table);
	
	void RemoveCompatibleObjectExplicit(UObject* Object) override;
	void RemoveCompatibleObjectExplicit(AActor* Actor) override;
	void RemoveCompatibleObjectExplicit(void* Object) override;

	TypedElementRowHandle FindRowWithCompatibleObjectExplicit(const UObject* Object) const override;
	TypedElementRowHandle FindRowWithCompatibleObjectExplicit(const AActor* Actor) const override;
	TypedElementRowHandle FindRowWithCompatibleObjectExplicit(const void* Object) const override;

private:
	void Prepare();
	void Reset();
	void CreateStandardArchetypes();
	
	void Tick();
	void TickPendingActorRegistration(UWorld* EditorWorld);
	void TickPendingUObjectRegistration();
	void TickPendingExternalObjectRegistration();
	void TickActorSync();

	void OnPostEditChangeProperty(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent);
	
	template<typename AddressType>
	struct PendingRegistration
	{
	private:
		TArray<AddressType> Addresses;
		TArray<TypedElementRowHandle> ReservedRowHandles;

	public:
		void Add(TypedElementRowHandle ReservedRowHandle, AddressType Address);
		bool IsEmpty() const;
		int32 Num() const;
		TArrayView<AddressType> GetAddresses();
		TArrayView<TypedElementRowHandle> GetReservedRowHandles();

		void RemoveInvalidEntries(ITypedElementDataStorageInterface& Storage, const TFunctionRef<bool(const AddressType&)>& Validator);
		void ProcessEntries(ITypedElementDataStorageInterface& Storage, TypedElementTableHandle Table,
			const TFunctionRef<void(TypedElementRowHandle, const AddressType&)>& SetupRowCallback);
		void Reset();
	};
	struct ExternalObjectRegistration
	{
		void* Object;
		TWeakObjectPtr<const UScriptStruct> TypeInfo;
	};
	TMap<TypedElementTableHandle, PendingRegistration<TWeakObjectPtr<AActor>>> ActorsPendingRegistration;
	TMap<TypedElementTableHandle, PendingRegistration<TWeakObjectPtr<UObject>>> UObjectsPendingRegistration;
	TMap<TypedElementTableHandle, PendingRegistration<ExternalObjectRegistration>> ExternalObjectsPendingRegistration;
	
	TypedElementTableHandle StandardActorTable{ TypedElementInvalidTableHandle };
	TypedElementTableHandle StandardActorWithTransformTable{ TypedElementInvalidTableHandle };
	TypedElementTableHandle StandardUObjectTable{ TypedElementInvalidTableHandle };
	TypedElementTableHandle StandardExternalObjectTable{ TypedElementInvalidTableHandle };
	ITypedElementDataStorageInterface* Storage{ nullptr };
	TSharedPtr<FMassActorManager> ActorSubsystem;

	TMap<void*, TypedElementRowHandle> ReverseObjectLookup;

	/**
	 * Reference of actors that need to be fully synced from the world to the database.
	 * May have duplicates
	 * Caution: Could point to actors that have been GC-ed
	 */
	TArray<TObjectKey<const AActor>> ActorsNeedingFullSync;

	FDelegateHandle PostEditChangePropertyDelegateHandle;
};