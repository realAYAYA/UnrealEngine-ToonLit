// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "PCGSettingsWithDynamicInputs.generated.h"

/**
* UPCGSettings subclass with functionality to dynamically add/remove input pins
*/
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGSettingsWithDynamicInputs: public UPCGSettings
{
	GENERATED_BODY()

public:
	virtual FName GetDynamicInputPinsBaseLabel() const { return NAME_None; }

#if WITH_EDITOR
	/** Validate custom pin properties */
	virtual bool CustomPropertiesAreValid(const FPCGPinProperties& CustomProperties) { return true; }
	
	/** User driven event to add a dynamic source pin */
	virtual void OnUserAddDynamicInputPin();
	/** Overridden logic to add a default source pin */
	virtual void AddDefaultDynamicInputPin() PURE_VIRTUAL(PCGDynamicSettings::AddDefaultSourcePin, );
	/** Check if the pin to remove is dynamic */
	virtual bool CanUserRemoveDynamicInputPin(int32 PinIndex);
	/** User driven event to remove a dynamic source pin */
	virtual void OnUserRemoveDynamicInputPin(UPCGNode* InOutNode, int32 PinIndex);

	/** Get the number of static input pins. */
	int32 GetStaticInputPinNum() const;
	/** Get the number of dynamic input pins. */
	int32 GetDynamicInputPinNum() const { return DynamicInputPinProperties.Num(); }
#endif // WITH_EDITOR

protected:
#if WITH_EDITOR
	/** Add a new dynamic source pin with the specified properties */
	void AddDynamicInputPin(FPCGPinProperties&& CustomProperties);
#endif // WITH_EDITOR

	/** The input pin properties that are statically defined by the client class */
	virtual TArray<FPCGPinProperties> StaticInputPinProperties() const;

	/** Dynamic pin properties that the user can add or remove from */
	UPROPERTY()
	TArray<FPCGPinProperties> DynamicInputPinProperties;

	//~Begin UPCGSettings interface
	/** A concatenation of the static and dynamic input pin properties */
	virtual TArray<FPCGPinProperties> InputPinProperties() const override final;
	//~End UPCGSettings interface
};