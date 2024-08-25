// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Camera/CameraShakeBase.h"
#include "SimpleCameraShakePattern.generated.h"

/**
 * A base class for a simple camera shake.
 */
UCLASS(Abstract)
class GAMEPLAYCAMERAS_API USimpleCameraShakePattern : public UCameraShakePattern
{
public:

	GENERATED_BODY()

	USimpleCameraShakePattern(const FObjectInitializer& ObjInit) : Super(ObjInit) {}

public:

	/** Duration in seconds of this shake. Zero or less means infinite. */
	UPROPERTY(EditAnywhere, Category=Timing)
	float Duration = 1.f;

	/** Blend-in time for this shake. Zero or less means no blend-in. */
	UPROPERTY(EditAnywhere, Category=Timing)
	float BlendInTime = 0.2f;

	/** Blend-out time for this shake. Zero or less means no blend-out. */
	UPROPERTY(EditAnywhere, Category=Timing)
	float BlendOutTime = 0.2f;

protected:

	// UCameraShakePattern interface
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual bool IsFinishedImpl() const override;
	virtual void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) override;
	virtual void TeardownShakePatternImpl()  override;

protected:

	/** The ongoing state for this shake */
	FCameraShakeState State;
};

