// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UObject/GCObject.h"
#include "UObject/ObjectPtr.h"

class FReferenceCollector;
class SWindow;

class FTypedElementsDataStorageRevisionControlModule : public IModuleInterface, public FGCObject
{
public:
	~FTypedElementsDataStorageRevisionControlModule() override = default;

	//
	// IModuleInterface
	//

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	
	//
	// FGCObject
	//
	
	void AddReferencedObjects(FReferenceCollector& Collector) override;
	FString GetReferencerName() const override;
};
