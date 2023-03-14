// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/CommonBoundActionButton.h"
#include "CommonTextBlock.h"
#include "Input/UIActionBinding.h"
#include "CommonActionWidget.h"
#include "Framework/Application/SlateApplication.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonBoundActionButton)

#define LOCTEXT_NAMESPACE "CommonBoundActionButton"

void UCommonBoundActionButton::SetRepresentedAction(FUIActionBindingHandle InBindingHandle)
{
	if (TSharedPtr<FUIActionBinding> OldBinding = FUIActionBinding::FindBinding(BindingHandle))
	{
		OldBinding->OnHoldActionProgressed.RemoveAll(this);
	}

	BindingHandle = InBindingHandle;
	UpdateInputActionWidget();

	//@todo DanH: Need to figure out a better way to do this so we don't have to go mining into the (supposedly private/hidden) FUIActionBinding itself
	if (TSharedPtr<FUIActionBinding> NewBinding = FUIActionBinding::FindBinding(InBindingHandle))
	{
		NewBinding->OnHoldActionProgressed.AddUObject(this, &UCommonBoundActionButton::NativeOnActionProgress);
	}
}

void UCommonBoundActionButton::NativeOnClicked()
{
	Super::NativeOnClicked();

	if (TSharedPtr<FUIActionBinding> ActionBinding = FUIActionBinding::FindBinding(BindingHandle))
	{
		ActionBinding->OnExecuteAction.ExecuteIfBound();
	}
}

void UCommonBoundActionButton::NativeOnCurrentTextStyleChanged()
{
	Super::NativeOnCurrentTextStyleChanged();

	if (Text_ActionName)
	{
		Text_ActionName->SetStyle(GetCurrentTextStyleClass());
	}
}

void UCommonBoundActionButton::UpdateInputActionWidget()
{
	if (InputActionWidget) //optional bound widget
	{
		InputActionWidget->SetInputActionBinding(BindingHandle);
		
		FText ActionDisplayName = BindingHandle.GetDisplayName();
		if (BindingHandle.IsValid())
		{
			const UWidget* BoundWidget = BindingHandle.GetBoundWidget();
			ULocalPlayer* BindingOwner = BoundWidget ? BoundWidget->GetOwningLocalPlayer() : nullptr;
			if (ensure(BindingOwner) && BindingOwner != GetOwningLocalPlayer())
			{
				TOptional<int32> BoundPlayerIndex = FSlateApplication::Get().GetUserIndexForController(BindingOwner->GetControllerId());
				if (BoundPlayerIndex.IsSet())
				{
					// This action belongs to a player that isn't our owner, so append a little indicator of the player it's for
					ActionDisplayName = FText::FormatNamed(LOCTEXT("OtherPlayerActionFormat", "[P{PlayerNum}] {ActionName}"),
						TEXT("PlayerNum"), FText::AsNumber(BoundPlayerIndex.GetValue() + 1),
						TEXT("ActionName"), ActionDisplayName);
				}
			}
		}

		if (Text_ActionName)
		{
			Text_ActionName->SetText(ActionDisplayName);
		}
		
		OnUpdateInputAction();
	}
}

#undef LOCTEXT_NAMESPACE
