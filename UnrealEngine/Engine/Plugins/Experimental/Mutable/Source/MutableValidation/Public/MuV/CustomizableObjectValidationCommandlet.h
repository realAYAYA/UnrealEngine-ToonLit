// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "Containers/Array.h"
#include "MuCO/CustomizableObjectInstance.h"

#include "CustomizableObjectValidationCommandlet.generated.h"

class UCustomizableObject;

UCLASS()
class UCustomizableObjectValidationCommandlet : public UCommandlet 
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
	
private:

	/** Customizable Object to be tested */
	UPROPERTY()
	UCustomizableObject* ToTestCustomizableObject = nullptr;
	
	/** Customizable Object Instance currently being updated*/
	UPROPERTY()
	UCustomizableObjectInstance* InstanceBeingUpdated = nullptr;

	/** Array of COI to be generated with randomized parameter values*/
	UPROPERTY()
	TArray<UCustomizableObjectInstance*> InstancesToProcess;
	
	/** Handle to be able to unbind OnInstanceUpdated(...) from instance end of update delegate.*/
	FDelegateHandle OnInstanceUpdateHandle;

	/** Did any of the instances fail the UpdateSkeletalMesh process?*/
	bool bInstanceFailedUpdate = false;

	// Instance update delegate
	FInstanceUpdateDelegate InstanceUpdateDelegate;

	/** Callback invoked once the currently updating instance has done the update.
	 * @paragm Result is a container that provides us with data related with the instance updating process.
	 */
	UFUNCTION()
	void OnInstanceUpdate(const FUpdateContext& Result);
};