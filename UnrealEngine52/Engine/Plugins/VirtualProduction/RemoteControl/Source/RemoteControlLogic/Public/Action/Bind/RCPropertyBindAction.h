// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Action/RCPropertyAction.h"
#include "RCPropertyBindAction.generated.h"

class URCController;

/**
 * Property Bind Action specifically for Bind Behaviour
 */
UCLASS()
class REMOTECONTROLLOGIC_API URCPropertyBindAction : public URCPropertyAction
{
	GENERATED_BODY()

public:
	URCPropertyBindAction() = default;

	//~ Begin URCAction interface
	virtual void Execute() const override;
	//~ End URCAction interface

	/** The Controller that drives us */
	UPROPERTY()
	TObjectPtr<URCController> Controller;
};
