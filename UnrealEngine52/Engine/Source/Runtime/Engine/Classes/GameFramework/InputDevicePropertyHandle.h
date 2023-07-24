// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InputDevicePropertyHandle.generated.h"

/** A handle to an active input device property that is being used by the InputDeviceSubsytem. */
USTRUCT(BlueprintType)
struct ENGINE_API FInputDevicePropertyHandle
{
	friend class UInputDeviceSubsystem;

	GENERATED_BODY()

	FInputDevicePropertyHandle();
	~FInputDevicePropertyHandle() = default;

	/** Returns true if this handle is valid */
	bool IsValid() const;

	/** An invalid Input Device Property handle */
	static FInputDevicePropertyHandle InvalidHandle;

	uint32 GetTypeHash() const;

	ENGINE_API friend uint32 GetTypeHash(const FInputDevicePropertyHandle& InHandle);

	bool operator==(const FInputDevicePropertyHandle& Other) const;
	bool operator!=(const FInputDevicePropertyHandle& Other) const;

	FString ToString() const;

private:

	// Private constructor because we don't want any other users to make a valid device property handle.
	FInputDevicePropertyHandle(uint32 InternalID);

	/** Static function to get a unique device handle. */
	static FInputDevicePropertyHandle AcquireValidHandle();

	/** The internal ID of this handle. Populated by the private constructor in AcquireValidHandle */
	UPROPERTY()
	uint32 InternalId;
};
