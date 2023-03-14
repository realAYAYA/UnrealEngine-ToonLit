// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TemplateSequence.h"
#include "CameraAnimationSequence.generated.h"

/*
 * A template sequence specifically designed for playing on cameras.
 */
UCLASS(BlueprintType)
class TEMPLATESEQUENCE_API UCameraAnimationSequence : public UTemplateSequence
{
public:
	GENERATED_BODY()

	UCameraAnimationSequence(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	virtual FText GetDisplayName() const override;
#endif
};
