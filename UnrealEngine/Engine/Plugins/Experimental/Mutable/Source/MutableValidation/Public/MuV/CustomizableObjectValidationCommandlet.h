// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "Containers/Array.h"
#include "MuCO/CustomizableObjectInstance.h"

#include "CustomizableObjectValidationCommandlet.generated.h"

// Forward declarations
class UCustomizableObject;
class UCOIUpdater;
class ITargetPlatform;

UCLASS()
class UCustomizableObjectValidationCommandlet : public UCommandlet 
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
	
private:
	
	/**
	 * Extracts the targeted compilation platform provided by the user. It will look for "-CompilationPlatformName="PlatformName".
	 * Examples : -CompilationPlatformName=WindowsEditor or -CompilationPlatformName=Switch
	 * @param Params The arguments provided to this commandlet.
	 * @return The target platform to be used for the CO compilation.
	 */
	ITargetPlatform* ParseCompilationPlatform(const FString& Params) const;

	/** Customizable Object to be tested */
	UPROPERTY()
	TObjectPtr<UCustomizableObject> ToTestCustomizableObject = nullptr;

	/** Array of COI to be generated with randomized parameter values */
	UPROPERTY()
	TArray<TObjectPtr<UCustomizableObjectInstance>> InstancesToProcess;

	/** Helper object designed to aid in the update of the CO Instances.*/
	UPROPERTY()
	TObjectPtr<UCOIUpdater> InstanceUpdater;
};