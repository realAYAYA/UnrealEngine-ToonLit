// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Animation/CurveSequence.h"

#include "SCommonAnimatedSwitcher.generated.h"

UENUM(BlueprintType)
enum class ECommonSwitcherTransition : uint8
{
	/** Fade transition only with no movement */
	FadeOnly,
	/** Increasing the active index goes right, decreasing goes left */
	Horizontal,
	/** Increasing the active index goes up, decreasing goes down */
	Vertical,
	/** Increasing the active index zooms in, decreasing zooms out */
	Zoom
};

UENUM(BlueprintType)
enum class ETransitionCurve : uint8
{
	/** Linear interpolation, with no easing */
	Linear,
	/** Quadratic ease in */
	QuadIn,
	/** Quadratic ease out */
	QuadOut,
	/** Quadratic ease in, quadratic ease out */
	QuadInOut,
	/** Cubic ease in */
	CubicIn,
	/** Cubic ease out */
	CubicOut,
	/** Cubic ease in, cubic ease out */
	CubicInOut,
};

static FORCEINLINE ECurveEaseFunction TransitionCurveToCurveEaseFunction(ETransitionCurve CurveType)
{
	switch (CurveType)
	{
		default:
		case ETransitionCurve::Linear: return ECurveEaseFunction::Linear;
		case ETransitionCurve::QuadIn: return ECurveEaseFunction::QuadIn;
		case ETransitionCurve::QuadOut: return ECurveEaseFunction::QuadOut;
		case ETransitionCurve::QuadInOut: return ECurveEaseFunction::QuadInOut;
		case ETransitionCurve::CubicIn: return ECurveEaseFunction::CubicIn;
		case ETransitionCurve::CubicOut: return ECurveEaseFunction::CubicOut;
		case ETransitionCurve::CubicInOut: return ECurveEaseFunction::CubicInOut;
	}
}

class COMMONUI_API SCommonAnimatedSwitcher : public SWidgetSwitcher
{
public:
	DECLARE_DELEGATE_OneParam(FOnActiveIndexChanged, int32);
	DECLARE_DELEGATE_OneParam(FOnIsTransitioningChanged, bool);

	SLATE_BEGIN_ARGS(SCommonAnimatedSwitcher)
		: _InitialIndex(0)
		, _TransitionType(ECommonSwitcherTransition::FadeOnly)
		, _TransitionCurveType(ETransitionCurve::CubicInOut)
		, _TransitionDuration(0.4f)
		{
			_Visibility = EVisibility::SelfHitTestInvisible;
		}
		
		SLATE_ARGUMENT(int32, InitialIndex)

		SLATE_ARGUMENT(ECommonSwitcherTransition, TransitionType)
		SLATE_ARGUMENT(ETransitionCurve, TransitionCurveType)
		SLATE_ARGUMENT(float, TransitionDuration)
		SLATE_EVENT(FOnActiveIndexChanged, OnActiveIndexChanged)
		SLATE_EVENT(FOnIsTransitioningChanged, OnIsTransitioningChanged)

	SLATE_END_ARGS()
	
public:
	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	void TransitionToIndex(int32 NewWidgetIndex, bool bInstantTransition = false);

	FSlot* GetChildSlot(int32 SlotIndex);

	void SetTransition(float Duration, ETransitionCurve Curve);

	bool IsTransitionPlaying() const;

private:
	EActiveTimerReturnType UpdateTransition(double InCurrentTime, float InDeltaTime);
	float GetTransitionProgress() const;

private:
	/** Anim sequence for the transition; plays twice per transition */
	FCurveSequence TransitionSequence;

	/** The pending active widget, set when the initial transition out completes. If set to a null value then we don't have any pending widget */
	TWeakPtr<SWidget> PendingActiveWidget;

	/** If true, we are transitioning content out and need to play the sequence again to transition it in */
	bool bTransitioningOut = false;

	bool bIsTransitionTimerRegistered = false;
	ECommonSwitcherTransition TransitionType;

	FOnActiveIndexChanged OnActiveIndexChanged;
	FOnIsTransitioningChanged OnIsTransitioningChanged;
};