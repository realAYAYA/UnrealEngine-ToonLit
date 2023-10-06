// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamAnyKeyWidget.h"
#include "Containers/Ticker.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Input processor class that forwards any relevant input to the owning widget
// All Handle events return true to consume the input
class FVCamPressAnyKeyInputProcessor : public IInputProcessor
{
public:
	FVCamPressAnyKeyInputProcessor(UDEPRECATED_VCamPressAnyKey* InOwner) : Owner(InOwner) {};

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {};

	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		HandleKey(InKeyEvent.GetKey());
		return true;
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		return true;
	}

	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		HandleKey(MouseEvent.GetEffectingButton());
		return true;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		return true;
	}

	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		HandleKey(MouseEvent.GetEffectingButton());
		return true;
	}

	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override
	{
		if (InWheelEvent.GetWheelDelta() != 0)
		{
			const FKey Key = InWheelEvent.GetWheelDelta() < 0 ? EKeys::MouseScrollDown : EKeys::MouseScrollUp;
			HandleKey(Key);
		}
		return true;
	}

private:
	void HandleKey(const FKey& Key) const
	{
		// We should always have a valid owner as the owner keeps us alive via a shared pointer
		check(IsValid(Owner));

		// Cancel the input if the key is escape or a touch event, otherwise pass the key back to the owner
		if (Key == EKeys::LeftCommand || Key == EKeys::RightCommand)
		{
			// Ignore
		}
		else if (Key == EKeys::Escape || Key.IsTouch())
		{
			Owner->HandleKeySelectionCanceled();
		}
		else
		{
			Owner->HandleKeySelected(Key);
		}
	}

	
	// Keep a reference to the widget owning this processor so we can notify on key events
	UDEPRECATED_VCamPressAnyKey* Owner = nullptr;
};
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void UDEPRECATED_VCamPressAnyKey::NativeOnActivated()
{
	Super::NativeOnActivated();

	bKeySelected = false;

	InputProcessor = MakeShared<FVCamPressAnyKeyInputProcessor>(this);
	FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor, 0);
}

void UDEPRECATED_VCamPressAnyKey::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
}

void UDEPRECATED_VCamPressAnyKey::HandleKeySelected(FKey InKey)
{
	if (!bKeySelected)
	{
		bKeySelected = true;
		Dismiss([this, InKey]()
		{
			OnKeySelected.Broadcast(InKey);
		});
	}
}

void UDEPRECATED_VCamPressAnyKey::HandleKeySelectionCanceled()
{
if (!bKeySelected)
	{
		bKeySelected = true;
		Dismiss([this]()
		{
			OnKeySelectionCanceled.Broadcast();
		});
	}
}

void UDEPRECATED_VCamPressAnyKey::Dismiss(TFunction<void()> PostDismissCallback)
{
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this, PostDismissCallback](float DeltaTime)
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
		}

		DeactivateWidget();
		PostDismissCallback();

		return false;
	}));
}
