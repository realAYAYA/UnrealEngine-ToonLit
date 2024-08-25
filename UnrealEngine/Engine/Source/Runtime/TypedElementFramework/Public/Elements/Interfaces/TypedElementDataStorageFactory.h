// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TypedElementDataStorageFactory.generated.h"

class ITypedElementDataStorageCompatibilityInterface;
class ITypedElementDataStorageInterface;
class ITypedElementDataStorageUiInterface;

/**
 * Base class that can be used to register various elements, such as queries and widgets, with
 * the Typed Elements Data Storage.
 */
UCLASS(MinimalAPI)
class UTypedElementDataStorageFactory : public UObject
{
	GENERATED_BODY()

public:
	~UTypedElementDataStorageFactory() override = default;

	/** 
	 * Returns the order registration will be executed. Factories with a lower number will be executed
	 * before factories with a higher number.
	 */
	virtual uint8 GetOrder() const { return 127; }

	/**
	 * All factories will have this called before any Register functions on any factories are called
	 */
	virtual void PreRegister(ITypedElementDataStorageInterface& DataStorage) {}

	virtual void RegisterTables(ITypedElementDataStorageInterface& DataStorage) {}
	virtual void RegisterTables(ITypedElementDataStorageInterface& DataStorage, ITypedElementDataStorageCompatibilityInterface& DataStorageCompatibility) {}
	virtual void RegisterTickGroups(ITypedElementDataStorageInterface& DataStorage) const {}
	virtual void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) {}

	virtual void RegisterRegistrationFilters(ITypedElementDataStorageCompatibilityInterface& DataStorageCompatibility) const {}
	virtual void RegisterDealiaser(ITypedElementDataStorageCompatibilityInterface& DataStorageCompatibility) const {}
	
	virtual void RegisterWidgetPurposes(ITypedElementDataStorageUiInterface& DataStorageUi) const {}
	virtual void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
		ITypedElementDataStorageUiInterface& DataStorageUi) const {}

	/**
	 * Called in reverse order before the DataStorage object is shut down
	 */
	virtual void PreShutdown(ITypedElementDataStorageInterface& DataStorage) {}
};
