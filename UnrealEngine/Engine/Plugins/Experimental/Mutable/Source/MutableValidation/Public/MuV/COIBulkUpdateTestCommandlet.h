// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Commandlets/Commandlet.h"
#include "MuV/ValidationUtils.h"

#include "COIBulkUpdateTestCommandlet.generated.h"

/**
 * Testing commandlet designed to allow the mass update of instances in a given project path.
 * It will run in sequence where each of the instances will get updated after their CO gets compiled.
 *
 * Example commandline that updates all instances found in the plugin content directory (and all subdirectories):
 * -run=COIBulkUpdateTest -EnablePlugins=MutableTesting -InstancesPackagePath="/MutableTesting" -AllowCommandletRendering
 */
UCLASS()
class UCOIBulkUpdateTestCommandlet : public UCommandlet 
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;

private:

	/** Helper object that handles the update of the instance for us.
	 * It allows us to work with the instance update as if it was a sync operation */
	UPROPERTY()
	TObjectPtr<UCOIUpdater> InstanceUpdater = nullptr;
};
