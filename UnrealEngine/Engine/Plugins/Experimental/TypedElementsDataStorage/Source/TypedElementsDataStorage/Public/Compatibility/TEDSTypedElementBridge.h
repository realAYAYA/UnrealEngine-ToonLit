// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Framework/TypedElementHandle.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "UObject/UObjectGlobals.h"

#include "TEDSTypedElementBridge.generated.h"

class UTypedElementRegistry;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTEDSTypedElementBridgeEnable, bool /*bEnabled*/);

/**
 * This class is responsible for running queries that will ensure TypedElementHandles
 * are cleaned up when TEDS is shut down.
 */
UCLASS(Transient)
class UTEDSTypedElementBridge : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()
public:
	~UTEDSTypedElementBridge() override = default;
	virtual uint8 GetOrder() const override;
	virtual void PreRegister(ITypedElementDataStorageInterface& DataStorage) override;
	virtual void PreShutdown(ITypedElementDataStorageInterface& DataStorage) override;
	virtual void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;

	static TYPEDELEMENTSDATASTORAGE_API FOnTEDSTypedElementBridgeEnable& OnEnabled();
	static TYPEDELEMENTSDATASTORAGE_API bool IsEnabled();

private:
	void RegisterQuery_NewUObject(ITypedElementDataStorageInterface& DataStorage);
	void UnregisterQuery_NewUObject(ITypedElementDataStorageInterface& DataStorage);
	void CleanupTypedElementColumns(ITypedElementDataStorageInterface& DataStorage);
	void HandleOnEnabled(IConsoleVariable* CVar);
	
	TypedElementQueryHandle RemoveTypedElementRowHandleQuery = TypedElementDataStorage::InvalidQueryHandle;
	FDelegateHandle DebugEnabledDelegateHandle;
};

// A column which contains a TypedElementHandle
// TypedElement, in this context, refers to the interface-based
// TypedElements instead of the data-based TypedElementDataStorage (TEDS)
USTRUCT()
struct FTEDSTypedElementColumn : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
	
	FTypedElementHandle Handle;
};