// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimSubsystem.h"
#include "Animation/ExposedValueHandler.h"
#include "AnimSubsystem_Base.generated.h"

/** Provides common built-in anim node functionality as a subsystem */
USTRUCT()
struct ENGINE_API FAnimSubsystem_Base : public FAnimSubsystem
{
	GENERATED_BODY()

	friend class UAnimBlueprintExtension_Base;

	// FAnimSubsystem interface
	virtual void OnPostLoadDefaults(FAnimSubsystemPostLoadDefaultsContext& InContext) override;

	// Get the exposed value handlers held on this subsystem
	const TArray<FExposedValueHandler>& GetExposedValueHandlers() const { return ExposedValueHandlers; }

private:
	UPROPERTY()
	TArray<FExposedValueHandler> ExposedValueHandlers;
};