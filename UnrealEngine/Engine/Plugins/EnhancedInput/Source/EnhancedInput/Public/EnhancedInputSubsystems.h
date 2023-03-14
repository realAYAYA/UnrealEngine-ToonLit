// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputSubsystemInterface.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Subsystems/EngineSubsystem.h"

#include "EnhancedInputSubsystems.generated.h"

// Per local player input subsystem
UCLASS()
class ENHANCEDINPUT_API UEnhancedInputLocalPlayerSubsystem : public ULocalPlayerSubsystem, public IEnhancedInputSubsystemInterface
{
	GENERATED_BODY()

public:

	// Begin IEnhancedInputSubsystemInterface
	virtual UEnhancedPlayerInput* GetPlayerInput() const override;
	virtual void ControlMappingsRebuiltThisFrame() override;
	// End IEnhancedInputSubsystemInterface

	/** A delegate that will be called when control mappings have been rebuilt this frame. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnControlMappingsRebuilt);

	/**
	 * Blueprint Event that is called at the end of any frame that Control Mappings have been rebuilt.
	 */
	UPROPERTY(BlueprintAssignable, DisplayName=OnControlMappingsRebuilt, Category = "Input")
	FOnControlMappingsRebuilt ControlMappingsRebuiltDelegate;
};
