// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonUserWidget.h"
#include "CommonUIPrivate.h"

#include "Engine/GameInstance.h"
#include "CommonInputSubsystem.h"
#include "CommonUISubsystemBase.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Input/CommonUIInputTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonUserWidget)

UCommonUserWidget::UCommonUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{	
#if WITH_EDITORONLY_DATA
	PaletteCategory = FText::FromString(TEXT("Common UI"));
#endif
}

void UCommonUserWidget::SetConsumePointerInput(bool bInConsumePointerInput)
{
	bConsumePointerInput = bInConsumePointerInput;
}

UCommonInputSubsystem* UCommonUserWidget::GetInputSubsystem() const
{
	return UCommonInputSubsystem::Get(GetOwningLocalPlayer());
}

UCommonUISubsystemBase* UCommonUserWidget::GetUISubsystem() const
{
	return UGameInstance::GetSubsystem<UCommonUISubsystemBase>(GetGameInstance());
}

TSharedPtr<FSlateUser> UCommonUserWidget::GetOwnerSlateUser() const
{
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSlateUser() : nullptr;
}

FReply UCommonUserWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return bConsumePointerInput ? FReply::Handled() : Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UCommonUserWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return bConsumePointerInput ? FReply::Handled() : Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UCommonUserWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return bConsumePointerInput ? FReply::Handled() : Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

FReply UCommonUserWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return bConsumePointerInput ? FReply::Handled() : Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
}

FReply UCommonUserWidget::NativeOnTouchGesture(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	return bConsumePointerInput ? FReply::Handled() : Super::NativeOnTouchGesture(InGeometry, InGestureEvent);
}

FReply UCommonUserWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	return bConsumePointerInput ? FReply::Handled() : Super::NativeOnTouchStarted(InGeometry, InGestureEvent);
}

FReply UCommonUserWidget::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	return bConsumePointerInput ? FReply::Handled() : Super::NativeOnTouchMoved(InGeometry, InGestureEvent);
}

FReply UCommonUserWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	return bConsumePointerInput ? FReply::Handled() : Super::NativeOnTouchEnded(InGeometry, InGestureEvent);
}

FUIActionBindingHandle UCommonUserWidget::RegisterUIActionBinding(const FBindUIActionArgs& BindActionArgs)
{
	if (UCommonUIActionRouterBase* ActionRouter = UCommonUIActionRouterBase::Get(*this))
	{
		FBindUIActionArgs FinalBindActionArgs = BindActionArgs;
		if (bDisplayInActionBar && !BindActionArgs.bDisplayInActionBar)
		{
			FinalBindActionArgs.bDisplayInActionBar = true;
		}
		FUIActionBindingHandle BindingHandle = ActionRouter->RegisterUIActionBinding(*this, FinalBindActionArgs);
		ActionBindings.Add(BindingHandle);
		return BindingHandle;
	}

	return FUIActionBindingHandle();
}

void UCommonUserWidget::RemoveActionBinding(FUIActionBindingHandle ActionBinding)
{
	ActionBindings.Remove(ActionBinding);
	if (UCommonUIActionRouterBase* ActionRouter = UCommonUIActionRouterBase::Get(*this))
	{
		ActionRouter->RemoveBinding(ActionBinding);
	}
}

void UCommonUserWidget::AddActionBinding(FUIActionBindingHandle ActionBinding)
{
	ActionBindings.Add(ActionBinding);
	if (UCommonUIActionRouterBase* ActionRouter = UCommonUIActionRouterBase::Get(*this))
	{
		ActionRouter->AddBinding(ActionBinding);
	}
}

void UCommonUserWidget::RegisterScrollRecipient(const UWidget& AnalogScrollRecipient)
{
	if (!ScrollRecipients.Contains(&AnalogScrollRecipient))
	{
		ScrollRecipients.Add(&AnalogScrollRecipient);
		if (GetCachedWidget())
		{
			if (UCommonUIActionRouterBase* ActionRouter = UCommonUIActionRouterBase::Get(*this))
			{
				ActionRouter->RegisterScrollRecipient(AnalogScrollRecipient);
			}
		}
	}
}

void UCommonUserWidget::UnregisterScrollRecipient(const UWidget& AnalogScrollRecipient)
{
	if (ScrollRecipients.Remove(&AnalogScrollRecipient) && GetCachedWidget())
	{
		if (UCommonUIActionRouterBase* ActionRouter = UCommonUIActionRouterBase::Get(*this))
		{
			ActionRouter->UnregisterScrollRecipient(AnalogScrollRecipient);
		}
	}
}

void UCommonUserWidget::OnWidgetRebuilt()
{
	// Using OnWidgetRebuilt instead of NativeConstruct to ensure we register ourselves with the ActionRouter before the child receives NativeConstruct
	if (!IsDesignTime())
	{
		// Clear out any invalid bindings before we bother trying to register them
		for (int32 BindingIdx = ActionBindings.Num() - 1; BindingIdx >= 0; --BindingIdx)
		{
			if (!ActionBindings[BindingIdx].IsValid())
			{
				ActionBindings.RemoveAt(BindingIdx);
			}
		}

		if (ActionBindings.Num() > 0)
		{
			if (UCommonUIActionRouterBase* ActionRouter = UCommonUIActionRouterBase::Get(*this))
			{
				ActionRouter->NotifyUserWidgetConstructed(*this);
			}
		}
	}

	Super::OnWidgetRebuilt();
}

void UCommonUserWidget::NativeDestruct()
{
	if (ActionBindings.Num() > 0)
	{
		if (UCommonUIActionRouterBase* ActionRouter = UCommonUIActionRouterBase::Get(*this))
		{
			ActionRouter->NotifyUserWidgetDestructed(*this);
		}
	}

	Super::NativeDestruct();
}


