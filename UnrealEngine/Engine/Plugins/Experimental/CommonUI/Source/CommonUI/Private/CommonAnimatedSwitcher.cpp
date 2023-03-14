// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonAnimatedSwitcher.h"

#include "CommonWidgetPaletteCategories.h"
#include "Components/PanelSlot.h"
#include "Components/WidgetSwitcherSlot.h"
#include "Templates/UnrealTemplate.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SOverlay.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonAnimatedSwitcher)

UCommonAnimatedSwitcher::UCommonAnimatedSwitcher(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TransitionType(ECommonSwitcherTransition::FadeOnly)
	, TransitionCurveType(ETransitionCurve::CubicInOut)
	, TransitionDuration(0.4f)
{

}

void UCommonAnimatedSwitcher::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	bSetOnce = false;

	MyOverlay.Reset();
	MyInputGuard.Reset();
	MyAnimatedSwitcher.Reset();
}

#if WITH_EDITOR
const FText UCommonAnimatedSwitcher::GetPaletteCategory()
{
	return CommonWidgetPaletteCategories::Default;
}
#endif

void UCommonAnimatedSwitcher::SetActiveWidgetIndex(int32 Index)
{
	SetActiveWidgetIndex_Internal(Index);
}

void UCommonAnimatedSwitcher::SetActiveWidget(UWidget* Widget)
{
	const int32 ChildIndex = GetChildIndex(Widget);
	SetActiveWidgetIndex_Internal(ChildIndex);
}

void UCommonAnimatedSwitcher::ActivateNextWidget(bool bCanWrap)
{
	if (Slots.Num() > 1)
	{
		if (ActiveWidgetIndex == Slots.Num() - 1)
		{
			if (bCanWrap)
			{
				SetActiveWidgetIndex(0);
			}
		}
		else
		{
			SetActiveWidgetIndex(ActiveWidgetIndex + 1);
		}
	}
}

void UCommonAnimatedSwitcher::ActivatePreviousWidget(bool bCanWrap)
{
	if (Slots.Num() > 1)
	{
		if (ActiveWidgetIndex == 0)
		{
			if (bCanWrap)
			{
				SetActiveWidgetIndex(Slots.Num() - 1);
			}
		}
		else
		{
			SetActiveWidgetIndex(ActiveWidgetIndex - 1);
		}
	}
}

bool UCommonAnimatedSwitcher::HasWidgets() const
{
	return Slots.Num() > 0;
}

void UCommonAnimatedSwitcher::SetDisableTransitionAnimation(bool bDisableAnimation)
{
	bInstantTransition = bDisableAnimation;
}

bool UCommonAnimatedSwitcher::IsCurrentlySwitching() const
{
	return bCurrentlySwitching;
}

void UCommonAnimatedSwitcher::HandleSlateActiveIndexChanged(int32 ActiveIndex)
{
	if (Slots.IsValidIndex(ActiveWidgetIndex))
	{
		OnActiveWidgetIndexChanged.Broadcast(GetWidgetAtIndex(ActiveWidgetIndex), ActiveWidgetIndex);
	}
}

TSharedRef<SWidget> UCommonAnimatedSwitcher::RebuildWidget()
{
	MyWidgetSwitcher = MyAnimatedSwitcher = SNew(SCommonAnimatedSwitcher)
		.InitialIndex(ActiveWidgetIndex)
		.TransitionCurveType(TransitionCurveType)
		.TransitionDuration(TransitionDuration)
		.TransitionType(TransitionType)
		.OnActiveIndexChanged_UObject(this, &UCommonAnimatedSwitcher::HandleSlateActiveIndexChanged)
		.OnIsTransitioningChanged_UObject(this, &UCommonAnimatedSwitcher::HandleSlateIsTransitioningChanged);

	for (UPanelSlot* CurrentSlot : Slots)
	{
		if (UWidgetSwitcherSlot* TypedSlot = Cast<UWidgetSwitcherSlot>(CurrentSlot))
		{
			TypedSlot->Parent = this;
			TypedSlot->BuildSlot(MyWidgetSwitcher.ToSharedRef());
		}
	}

	return SAssignNew(MyOverlay, SOverlay)
		+ SOverlay::Slot()
		[
			MyAnimatedSwitcher.ToSharedRef()
		]
		+ SOverlay::Slot()
		[
			SAssignNew(MyInputGuard, SSpacer)
			.Visibility(EVisibility::Collapsed)
		];
}

void UCommonAnimatedSwitcher::HandleSlateIsTransitioningChanged(bool bIsTransitioning)
{
	// While the switcher is transitioning, put up the guard to intercept all input
	MyInputGuard->SetVisibility(bIsTransitioning ? EVisibility::Visible : EVisibility::Collapsed);

	OnTransitioningChanged.Broadcast(bIsTransitioning);
}

void UCommonAnimatedSwitcher::SetActiveWidgetIndex_Internal(int32 Index)
{
#if WITH_EDITOR
	if (IsDesignTime())
	{
		Super::SetActiveWidgetIndex(Index);
		return;
	}
#endif
	
	TGuardValue<bool> bCurrentlySwitchingGuard(bCurrentlySwitching, true);

	if (Index >= 0 && Index < Slots.Num() && (Index != ActiveWidgetIndex || !bSetOnce))
	{
		HandleOutgoingWidget();

		ActiveWidgetIndex = Index;

		if (MyAnimatedSwitcher.IsValid())
		{
			// Ensure the index is clamped to a valid range.
			int32 SafeIndex = FMath::Clamp(ActiveWidgetIndex, 0, FMath::Max(0, Slots.Num() - 1));
			MyAnimatedSwitcher->TransitionToIndex(SafeIndex, bInstantTransition);
		}

		//When we're setting up, and the index goes 0->0, MyAnimatedSwitcher won't fire its ActiveIndexChanged Event.
		if (!bSetOnce)
		{
			HandleSlateActiveIndexChanged(ActiveWidgetIndex);
		}

		bSetOnce = true;
	}
}

bool UCommonAnimatedSwitcher::IsTransitionPlaying() const
{
	if (MyAnimatedSwitcher.IsValid())
	{
		return MyAnimatedSwitcher->IsTransitionPlaying();
	}
	else
	{
		return false;
	}
}

