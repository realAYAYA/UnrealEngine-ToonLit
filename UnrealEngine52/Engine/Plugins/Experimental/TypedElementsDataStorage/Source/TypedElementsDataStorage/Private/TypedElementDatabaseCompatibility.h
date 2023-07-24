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

	void AddCompatibleObject(AActor* Actor) override;
	void RemoveCompatibleObject(AActor* Actor) override;

private:
	void Prepare();
	void Reset();
	void CreateStandardArchetypes();
	
	void AddPendingActors();
	
	TArray<TWeakObjectPtr<AActor>> ActorsPendingRegistration;
	
	TypedElementTableHandle StandardActorTable{ TypedElementInvalidTableHandle };
	TypedElementTableHandle StandardActorWithTransformTable{ TypedElementInvalidTableHandle };
	ITypedElementDataStorageInterface* Storage{ nullptr };
	TSharedPtr<FMassActorManager> ActorSubsystem;
};