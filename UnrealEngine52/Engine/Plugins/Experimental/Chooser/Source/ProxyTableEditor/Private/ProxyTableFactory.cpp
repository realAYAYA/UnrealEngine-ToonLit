// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProxyTableFactory.h"
#include "ProxyTable.h"

UProxyTableFactory::UProxyTableFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UProxyTable::StaticClass();
}

bool UProxyTableFactory::ConfigureProperties()
{
	return true;
}

UObject* UProxyTableFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return NewObject<UProxyTable>(InParent, Class, Name, Flags | RF_Public | RF_Standalone | RF_Transactional | RF_LoadCompleted);
}