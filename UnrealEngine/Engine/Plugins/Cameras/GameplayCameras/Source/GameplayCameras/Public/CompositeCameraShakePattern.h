// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Camera/CameraShakeBase.h"
#include "CompositeCameraShakePattern.generated.h"

/**
 * A base class for a simple camera shake.
 */
UCLASS(meta=(AutoExpandCategories="CameraShake"))
class GAMEPLAYCAMERAS_API UCompositeCameraShakePattern : public UCameraShakePattern
{
public:

	GENERATED_BODY()

	UCompositeCameraShakePattern(const FObjectInitializer& ObjInit) : Super(ObjInit) {}

	template<typename PatternClass>
	PatternClass* AddChildPattern()
	{
		PatternClass* NewChildPattern = NewObject<PatternClass>();
		ChildPatterns.Add(NewChildPattern);
		return NewChildPattern;
	}

public:

	/** The list of child shake patterns */
	UPROPERTY(EditAnywhere, Instanced, Category=CameraShake)
	TArray<TObjectPtr<UCameraShakePattern>> ChildPatterns;

private:

	// UCameraShakePattern interface
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakeStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakeUpdateParams& Params, FCameraShakeUpdateResult& OutResult) override;
	virtual void ScrubShakePatternImpl(const FCameraShakeScrubParams& Params, FCameraShakeUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override;
	virtual void StopShakePatternImpl(const FCameraShakeStopParams& Params) override;
	virtual void TeardownShakePatternImpl() override;

private:

	bool IsChildPatternFinished(const FCameraShakeState& ChildState, const UCameraShakePattern* ChildPattern) const;

private:

	/** Play state of the children patterns */
	TArray<FCameraShakeState> ChildStates;
};
