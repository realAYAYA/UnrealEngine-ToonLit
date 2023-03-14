// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Curves/RichCurve.h"
#include "Generators/MovieSceneEasingFunction.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "UObject/Class.h"
#include "UObject/Interface.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneEasingCurves.generated.h"

class UCurveFloat;

UENUM()
enum class EMovieSceneBuiltInEasing : uint8
{
	// Linear easing
	Linear UMETA(Grouping=Linear,DisplayName="Linear"),
	// Sinusoidal easing
	SinIn UMETA(Grouping=Sinusoidal,DisplayName="Sinusoidal In"), SinOut UMETA(Grouping=Sinusoidal,DisplayName="Sinusoidal Out"), SinInOut UMETA(Grouping=Sinusoidal,DisplayName="Sinusoidal InOut"),
	// Quadratic easing
	QuadIn UMETA(Grouping=Quadratic,DisplayName="Quadratic In"), QuadOut UMETA(Grouping=Quadratic,DisplayName="Quadratic Out"), QuadInOut UMETA(Grouping=Quadratic,DisplayName="Quadratic InOut"),
	// Cubic easing
	CubicIn UMETA(Grouping=Cubic,DisplayName="Cubic In"), CubicOut UMETA(Grouping=Cubic,DisplayName="Cubic Out"), CubicInOut UMETA(Grouping=Cubic,DisplayName="Cubic InOut"),
	// Quartic easing
	QuartIn UMETA(Grouping=Quartic,DisplayName="Quartic In"), QuartOut UMETA(Grouping=Quartic,DisplayName="Quartic Out"), QuartInOut UMETA(Grouping=Quartic,DisplayName="Quartic InOut"),
	// Quintic easing
	QuintIn UMETA(Grouping=Quintic,DisplayName="Quintic In"), QuintOut UMETA(Grouping=Quintic,DisplayName="Quintic Out"), QuintInOut UMETA(Grouping=Quintic,DisplayName="Quintic InOut"),
	// Exponential easing
	ExpoIn UMETA(Grouping=Exponential,DisplayName="Exponential In"), ExpoOut UMETA(Grouping=Exponential,DisplayName="Exponential Out"), ExpoInOut UMETA(Grouping=Exponential,DisplayName="Exponential InOut"),
	// Circular easing
	CircIn UMETA(Grouping=Circular,DisplayName="Circular In"), CircOut UMETA(Grouping=Circular,DisplayName="Circular Out"), CircInOut UMETA(Grouping=Circular,DisplayName="Circular InOut"),
};


UCLASS(BlueprintType, meta=(DisplayName = "Built-In Function"))
class MOVIESCENE_API UMovieSceneBuiltInEasingFunction : public UObject, public IMovieSceneEasingFunction
{
public:
	GENERATED_BODY()

	UMovieSceneBuiltInEasingFunction(const FObjectInitializer& Initiailizer);

	virtual float Evaluate(float InTime) const override;

#if WITH_EDITOR
	virtual FText GetDisplayName() const override { return StaticEnum<EMovieSceneBuiltInEasing>()->GetDisplayNameTextByValue((int64)Type); }
#endif

	UPROPERTY(EditAnywhere, Category=Easing)
	EMovieSceneBuiltInEasing Type;
};


UCLASS(meta=(DisplayName="Curve Asset"))
class MOVIESCENE_API UMovieSceneEasingExternalCurve : public UObject, public IMovieSceneEasingFunction
{
public:
	GENERATED_BODY()

	virtual float Evaluate(float InTime) const override;

#if WITH_EDITOR
	virtual FText GetDisplayName() const override { return StaticClass()->GetDisplayNameText(); }
#endif

	/** Curve data */
	UPROPERTY(EditAnywhere, Category=Easing)
	TObjectPtr<UCurveFloat> Curve;
};
