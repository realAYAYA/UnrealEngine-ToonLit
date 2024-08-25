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
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual void ScrubShakePatternImpl(const FCameraShakePatternScrubParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override;
	virtual void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) override;
	virtual void TeardownShakePatternImpl() override;
};

