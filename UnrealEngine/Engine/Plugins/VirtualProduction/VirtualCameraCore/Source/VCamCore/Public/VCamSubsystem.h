// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/Subsystem.h"
#include "VCamSubsystem.generated.h"

class UVCamComponent;

/** Base class for auto instanced and initialized subsystem that share the lifetime of UVCamComponents. */
UCLASS(Abstract, Blueprintable, Within = VCamComponent)
class VCAMCORE_API UVCamSubsystem : public USubsystem
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	UVCamComponent* GetVCamComponent() const;

	/** Called by UVCamComponent::Update (its equivalent of Tick). */
	virtual void OnUpdate(float DeltaTime) {}
};
